from __future__ import annotations

import hashlib
import struct
from enum import Enum
from typing import Optional

from pydantic import BaseModel, Field


class ParamType(str, Enum):
    FLOAT = "float"
    BOOL = "bool"
    COLOR = "color"
    EVENT = "event"


class StageParameter(BaseModel):
    name: str = Field(..., description="参数名称，如 AR_Hologram_Alpha")
    type: ParamType = Field(..., description="参数类型")
    default: float = Field(default=0.0, description="默认值")
    min_value: Optional[float] = Field(default=None, description="最小值")
    max_value: Optional[float] = Field(default=None, description="最大值")
    tags: list[str] = Field(default_factory=list, description="中文语义标签，如 ['全息', '透明度', 'AR']")
    description: str = Field(default="", description="参数用途描述")
    category: str = Field(default="", description="分组类别")


class StageSchema(BaseModel):
    source: str = Field(default="ue5", description="来源客户端标识")
    version: int = Field(default=1, description="Schema 版本号")
    parameters: list[StageParameter] = Field(
        default_factory=list, description="舞台特效参数清单"
    )


class ControlCommand(BaseModel):
    param_hash: int = Field(..., description="参数名的 FNV-1a 32-bit Hash ID")
    value: float = Field(..., description="浮点控制值")


class CommandResult(BaseModel):
    success: bool
    param_hash: int
    param_name: str = ""
    value: float
    message: str = ""


# ==================== AI 编排模型 (V2.1) ====================


class FeatureStatus(str, Enum):
    ACTIVE = "active"
    IDLE = "idle"


class StageFeature(BaseModel):
    """AI 封装的功能 Feature —— 将 UE5 已有参数按逻辑组合命名。

    参数值全部来自 UE5 上报的 Schema（默认值/范围），AI 只负责命名和组织。
    """
    name: str = Field(..., description="功能名，如 '场景灯光'")
    description: str = Field(default="", description="功能描述")
    params: list[StageParameter] = Field(
        default_factory=list,
        description="包含的 UE5 参数（值来自 UE5 默认值）",
    )
    status: FeatureStatus = Field(default=FeatureStatus.IDLE, description="运行状态")


class LayoutConfig(BaseModel):
    """AI 编排前端画布布局配置。"""
    feature_order: list[str] = Field(
        default_factory=list,
        description="功能卡片排序（按 name 列表）",
    )
    collapsed_features: list[str] = Field(
        default_factory=list,
        description="默认折叠的功能",
    )
    highlight_params: list[str] = Field(
        default_factory=list,
        description="高亮显示的参数名",
    )


class AICommandType(str, Enum):
    FEATURE_CREATED = "feature_created"
    FEATURE_REMOVED = "feature_removed"
    FEATURE_UPDATED = "feature_updated"
    LAYOUT_UPDATED = "layout_updated"


class AICommandMessage(BaseModel):
    """AI 到前端的 WebSocket 消息统一封装。"""
    type: AICommandType
    payload: dict = Field(default_factory=dict)


def compute_param_hash(name: str) -> int:
    """FNV-1a 32-bit hash，与 UE5 端保持一致。

    UE5 使用 TCHAR (UTF-16 LE) 编码遍历字符，因此 Python 端也必须使用
    UTF-16 LE 编码来计算 Hash，确保两端 Hash 值相同。
    """
    FNV_OFFSET: int = 0x811C9DC5
    FNV_PRIME: int = 0x01000193
    h: int = FNV_OFFSET
    for byte in name.encode("utf-16-le"):
        h ^= byte
        h = (h * FNV_PRIME) & 0xFFFFFFFF
    return h


def pack_udp_message(param_hash: int, value: float) -> bytes:
    """封装 8 字节大端 UDP 包: uint32 hash + float32 value."""
    return struct.pack(">If", param_hash, value)


def unpack_udp_message(data: bytes) -> tuple[int, float]:
    """解包 8 字节大端 UDP 包，返回 (param_hash, value)。"""
    if len(data) != 8:
        raise ValueError(f"UDP 包长度必须为 8 字节，收到 {len(data)} 字节")
    param_hash, value = struct.unpack(">If", data)
    return param_hash, value