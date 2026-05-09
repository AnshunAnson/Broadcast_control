from __future__ import annotations

import asyncio
import json
import logging
from typing import Callable, Awaitable

from mcp.server import Server
from mcp.server.stdio import stdio_server
from mcp.types import Resource, ResourceTemplate, TextContent, Tool

from .models import (
    compute_param_hash,
    StageFeature,
    FeatureStatus,
    LayoutConfig,
    AICommandMessage,
    AICommandType,
)
from .schema_registry import get_stage_registry
from .udp_sender import get_broadcaster

logger = logging.getLogger(__name__)

STAGE_SERVER = Server("stage-broadcast-control")

_frontend_broadcast: Callable[[AICommandMessage], Awaitable[None]] | None = None


def set_frontend_broadcast(fn: Callable[[AICommandMessage], Awaitable[None]]) -> None:
    global _frontend_broadcast
    _frontend_broadcast = fn


async def _push_to_frontend(msg_type: AICommandType, payload: dict) -> None:
    if _frontend_broadcast is not None:
        await _frontend_broadcast(AICommandMessage(type=msg_type, payload=payload))


@STAGE_SERVER.list_tools()
async def list_tools() -> list[Tool]:
    return [
        Tool(
            name="get_capabilities",
            description=(
                "获取当前舞台全量能力清单 —— 返回 UE5 上报的所有可控参数及其类型、默认值、范围和中文语义标签。"
                "AI 通过此工具了解有哪些原始参数可用于封装功能。"
            ),
            inputSchema={
                "type": "object",
                "properties": {},
                "required": [],
            },
        ),
        Tool(
            name="compose_feature",
            description=(
                "【核心工具】AI 将 UE5 已有参数按逻辑组合封装为功能 Feature。\n"
                "参数值全部来自 UE5 上报的默认值，AI 只负责命名和组织。\n"
                "创建后自动推送到前端功能执行看板展示。\n\n"
                "使用示例:\n"
                "  用户说'我要控制场景灯光' → AI 调用 get_capabilities 找到发光/颜色参数\n"
                "  → compose_feature(name='场景灯光', description='控制场景灯光效果',\n"
                "    param_names=['Agent_Emissive', 'Agent_Color'])\n"
                "  → 前端看板出现'场景灯光'功能卡片"
            ),
            inputSchema={
                "type": "object",
                "properties": {
                    "name": {
                        "type": "string",
                        "description": "功能名称，如 '场景灯光'、'全息投影控制'",
                    },
                    "description": {
                        "type": "string",
                        "description": "功能描述，说明该功能的用途",
                    },
                    "param_names": {
                        "type": "array",
                        "items": {"type": "string"},
                        "description": "要包含的 UE5 参数名列表，如 ['Agent_Emissive', 'Agent_Color']",
                    },
                },
                "required": ["name", "param_names"],
            },
        ),
        Tool(
            name="configure_layout",
            description=(
                "AI 编排前端功能画布的布局 —— 控制 Feature 卡片的排序、折叠状态、参数高亮。\n"
                "配置后实时推送到前端。"
            ),
            inputSchema={
                "type": "object",
                "properties": {
                    "feature_order": {
                        "type": "array",
                        "items": {"type": "string"},
                        "description": "功能卡片排序列表，按 name 排列",
                    },
                    "collapsed": {
                        "type": "array",
                        "items": {"type": "string"},
                        "description": "默认折叠的功能 name 列表",
                    },
                    "highlights": {
                        "type": "array",
                        "items": {"type": "string"},
                        "description": "高亮显示的参数名列表",
                    },
                },
                "required": [],
            },
        ),
        Tool(
            name="set_float_parameter",
            description=(
                "精确参数控制 —— 通过参数名直接发送控制指令到 UE5 渲染端。"
                "适用于已知精确参数名的场景，参数名大小写不敏感。"
                "作为 AI 快捷调试通道保留。"
            ),
            inputSchema={
                "type": "object",
                "properties": {
                    "param_name": {
                        "type": "string",
                        "description": "精确参数名称，如 'AR_Hologram_Alpha'",
                    },
                    "value": {
                        "type": "number",
                        "description": "目标控制值",
                    },
                },
                "required": ["param_name", "value"],
            },
        ),
        Tool(
            name="execute_stage_cue",
            description=(
                "智能导播指令 —— 接收自然语言描述的舞台控制意图，自动映射到对应的舞台参数并发送控制指令。\n"
                "支持的中文语义指令示例：\n"
                "  - '淡入全息投影' / '把全息调亮到 0.8'\n"
                "  - '将环境光调暗为 0.2' / '降低环境亮度'\n"
                "  - '启动烟花特效' / '触发转场动画'\n"
                "cue_name 为参数的名称或者中文语义描述，value 为 0.0~1.0 的浮点值（事件类参数 value 填 1.0 触发即可）。\n"
                "target: 'ue5' 直接 UDP 控制 / 'frontend' 推送到前端 / 'both' 两者 (默认 'ue5')"
            ),
            inputSchema={
                "type": "object",
                "properties": {
                    "cue_name": {
                        "type": "string",
                        "description": "参数名称或中文语义描述，例如 'AR_Hologram_Alpha'、'全息投影透明度'、'环境光亮度'",
                    },
                    "value": {
                        "type": "number",
                        "description": "目标控制值，浮点数 0.0~1.0；事件类型填 1.0 触发",
                        "default": 1.0,
                    },
                    "target": {
                        "type": "string",
                        "description": "目标: 'ue5' (默认) / 'frontend' / 'both'",
                        "default": "ue5",
                    },
                },
                "required": ["cue_name", "value"],
            },
        ),
    ]


@STAGE_SERVER.list_resources()
async def list_resources() -> list[Resource]:
    return [
        Resource(
            uri="vp://schema",
            name="当前舞台 Schema",
            description="完整的 UE5 舞台特效参数清单 JSON Schema",
            mimeType="application/json",
        )
    ]


@STAGE_SERVER.read_resource()
async def read_resource(uri: str):
    if uri == "vp://schema":
        registry = get_stage_registry()
        schema = registry.get_schema()
        if schema is None:
            return []
        return schema.model_dump_json(indent=2)


@STAGE_SERVER.call_tool()
async def call_tool(name: str, arguments: dict) -> list[TextContent]:
    if name == "get_capabilities":
        return await _handle_get_capabilities(arguments)
    elif name == "compose_feature":
        return await _handle_compose_feature(arguments)
    elif name == "configure_layout":
        return await _handle_configure_layout(arguments)
    elif name == "set_float_parameter":
        return await _handle_set_float_parameter(arguments)
    elif name == "execute_stage_cue":
        return await _handle_execute_stage_cue(arguments)
    else:
        raise ValueError(f"未知工具: {name}")


async def _handle_execute_stage_cue(arguments: dict) -> list[TextContent]:
    """智能导播指令 —— 语义解析 + 目标分发。"""
    cue_name: str = arguments.get("cue_name", "")
    value: float = float(arguments.get("value", 1.0))
    target: str = arguments.get("target", "ue5")

    if not cue_name:
        raise ValueError("cue_name 不能为空")

    registry = get_stage_registry()
    schema = registry.get_schema()
    if schema is None or registry.parameter_count == 0:
        raise ValueError("当前无可用舞台 Schema，请等待 UE5 上报参数清单后重试。")

    param = registry.resolve_cue(cue_name)
    if param is None:
        available = [p.name for p in registry.all_parameters]
        raise ValueError(
            f"未找到匹配参数: '{cue_name}'。\n"
            f"当前可用参数: {', '.join(available)}\n"
            f"请使用 get_capabilities 获取完整能力清单。"
        )

    results: list[str] = []
    param_hash = compute_param_hash(param.name)
    clamp_value = clamp(value, param.min_value, param.max_value)

    if target in ("ue5", "both"):
        broadcast = get_broadcaster()
        success = await broadcast.broadcast(param_hash, clamp_value, retries=2)
        if success:
            results.append(f"UDP → UE5: {param.name} = {clamp_value:.4f}")
        else:
            raise RuntimeError(f"UDP 发送失败: {param.name} = {clamp_value}")

    if target in ("frontend", "both"):
        await _push_to_frontend(
            AICommandType.FEATURE_UPDATED,
            {
                "param_name": param.name,
                "value": clamp_value,
                "param_hash": param_hash,
            },
        )
        results.append(f"WS → 前端: {param.name} = {clamp_value:.4f}")

    response = (
        f"✓ 导播指令已执行: '{cue_name}' → {param.name}\n"
        f"  Hash: 0x{param_hash:08X}  Value: {clamp_value:.4f}\n"
        f"  目标: {target}\n"
        + "\n".join(f"  {r}" for r in results)
    )
    logger.info(response)
    return [TextContent(type="text", text=response)]


async def _handle_get_capabilities(_arguments: dict) -> list[TextContent]:
    """获取全量能力清单。"""
    registry = get_stage_registry()
    schema = registry.get_schema()

    if schema is None or registry.parameter_count == 0:
        return [
            TextContent(
                type="text",
                text="当前无可用舞台 Schema，请等待 UE5 上报参数清单。",
            )
        ]

    lines = [f"## 舞台能力清单 (v{schema.version}, 来源: {schema.source})\n"]
    lines.append(f"共 {registry.parameter_count} 个可控参数:\n")

    for param in registry.all_parameters:
        line = f"- **{param.name}** [{param.type.value}]"
        range_info = ""
        if param.min_value is not None or param.max_value is not None:
            lo = param.min_value if param.min_value is not None else "∞"
            hi = param.max_value if param.max_value is not None else "∞"
            range_info = f" 范围: [{lo}, {hi}]"
        line += f" 默认: {param.default}{range_info}"
        if param.tags:
            line += f" 标签: {', '.join(param.tags)}"
        if param.description:
            line += f" — {param.description}"
        lines.append(line)

    # 列出已有 Feature
    if registry.feature_count > 0:
        lines.append(f"\n### 当前已封装的功能 ({registry.feature_count} 个):\n")
        for feat in registry.list_features():
            pnames = [p.name for p in feat.params]
            lines.append(f"- **{feat.name}**: {', '.join(pnames)} [{feat.status.value}]")

    return [TextContent(type="text", text="\n".join(lines))]


async def _handle_compose_feature(arguments: dict) -> list[TextContent]:
    """AI 封装功能 —— 将 UE5 参数组合为 Feature。"""
    feature_name: str = arguments.get("name", "")
    param_names: list[str] = arguments.get("param_names", [])

    if not feature_name:
        raise ValueError("name 不能为空")
    if not param_names:
        raise ValueError("param_names 不能为空")

    registry = get_stage_registry()
    schema = registry.get_schema()
    if schema is None or registry.parameter_count == 0:
        raise ValueError("当前无可用舞台 Schema，请等待 UE5 上报参数清单后重试。")

    # 检查是否已存在同名 Feature
    existing = registry.get_feature(feature_name)
    if existing is not None:
        raise ValueError(
            f"功能 '{feature_name}' 已存在，包含 {len(existing.params)} 个参数。"
            f"如需重建请先删除。"
        )

    params = []
    for pname in param_names:
        param = registry.lookup_exact(pname)
        if param is None:
            available = [p.name for p in registry.all_parameters]
            raise ValueError(
                f"UE5 中不存在参数: '{pname}'。\n"
                f"当前可用参数: {', '.join(available)}"
            )
        params.append(param.model_copy())

    feature = StageFeature(
        name=feature_name,
        description=arguments.get("description", ""),
        params=params,
        status=FeatureStatus.IDLE,
    )

    registry.register_feature(feature)
    await _push_to_frontend(
        AICommandType.FEATURE_CREATED,
        feature.model_dump(),
    )

    response = (
        f"✓ 功能 '{feature_name}' 已创建\n"
        f"  包含 {len(params)} 个参数: {', '.join(p.name for p in params)}\n"
        f"  参数默认值/范围均来自 UE5 定义\n"
        f"  已推送到前端功能执行看板"
    )
    return [TextContent(type="text", text=response)]


async def _handle_configure_layout(arguments: dict) -> list[TextContent]:
    """AI 编排前端布局。"""
    registry = get_stage_registry()

    layout = LayoutConfig(
        feature_order=arguments.get("feature_order", []),
        collapsed_features=arguments.get("collapsed", []),
        highlight_params=arguments.get("highlights", []),
    )

    registry.update_layout(layout)
    await _push_to_frontend(
        AICommandType.LAYOUT_UPDATED,
        layout.model_dump(),
    )

    response = (
        f"✓ 前端布局已更新\n"
        f"  排序: {layout.feature_order or '默认'}\n"
        f"  折叠: {layout.collapsed_features or '无'}\n"
        f"  高亮: {layout.highlight_params or '无'}"
    )
    return [TextContent(type="text", text=response)]


async def _handle_set_float_parameter(arguments: dict) -> list[TextContent]:
    """精确参数控制。"""
    param_name: str = arguments.get("param_name", "")
    value: float = float(arguments.get("value", 0.0))

    if not param_name:
        raise ValueError("param_name 不能为空")

    registry = get_stage_registry()
    param = registry.lookup_exact(param_name)
    if param is None:
        raise ValueError(f"未找到参数: '{param_name}'")

    broadcast = get_broadcaster()
    param_hash = compute_param_hash(param.name)
    clamp_value = clamp(value, param.min_value, param.max_value)

    success = await broadcast.broadcast(param_hash, clamp_value, retries=2)
    if success:
        return [
            TextContent(
                type="text",
                text=f"✓ {param.name} = {clamp_value:.4f} (hash: 0x{param_hash:08X})",
            )
        ]
    else:
        raise RuntimeError(f"UDP 发送失败: {param.name} = {clamp_value}")


def clamp(value: float, lo: float | None, hi: float | None) -> float:
    if lo is not None and value < lo:
        return lo
    if hi is not None and value > hi:
        return hi
    return value


async def run_mcp_server() -> None:
    """启动 MCP Server，通过 stdio 与 Trae/AI IDE 通信。"""
    logger.info("MCP Server (舞台播控) 启动中...")
    async with stdio_server() as (read_stream, write_stream):
        await STAGE_SERVER.run(
            read_stream,
            write_stream,
            STAGE_SERVER.create_initialization_options(),
        )