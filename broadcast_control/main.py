from __future__ import annotations

import asyncio
import logging
import sys
from contextlib import asynccontextmanager
from typing import Optional

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.responses import JSONResponse

from .config import HTTP_LISTEN_HOST, HTTP_LISTEN_PORT
from .mcp_server import run_mcp_server, set_frontend_broadcast
from .models import (
    CommandResult,
    StageSchema,
    StageFeature,
    FeatureStatus,
    LayoutConfig,
    AICommandMessage,
    AICommandType,
    compute_param_hash,
    pack_udp_message,
)
from .schema_registry import get_stage_registry
from .udp_sender import get_broadcaster

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
    stream=sys.stderr,
)
logger = logging.getLogger(__name__)

active_websockets: set[WebSocket] = set()


async def broadcast_feature_to_frontend(msg: AICommandMessage) -> None:
    disconnected: list[WebSocket] = []
    payload = {"type": msg.type.value, **msg.payload}
    for ws in active_websockets:
        try:
            await ws.send_json(payload)
        except (WebSocketDisconnect, Exception):
            disconnected.append(ws)
    for ws in disconnected:
        active_websockets.discard(ws)


async def broadcast_ws_message(data: dict) -> None:
    disconnected: list[WebSocket] = []
    for ws in active_websockets:
        try:
            await ws.send_json(data)
        except (WebSocketDisconnect, Exception):
            disconnected.append(ws)
    for ws in disconnected:
        active_websockets.discard(ws)


def _make_lifespan(start_mcp: bool = True):
    @asynccontextmanager
    async def lifespan(app: FastAPI):
        logger.info("播控中台启动中 — 正在初始化 UDP Broadcaster...")
        broadcaster = get_broadcaster()
        await broadcaster.connect()

        set_frontend_broadcast(broadcast_feature_to_frontend)

        if start_mcp:
            asyncio.create_task(run_mcp_server())
            logger.info("MCP Server 子任务已启动 (stdio)")

        yield

        logger.info("播控中台关闭中...")
        await broadcaster.close()
        for ws in list(active_websockets):
            await ws.close()
        active_websockets.clear()

    return lifespan


def create_app(start_mcp: bool = True) -> FastAPI:
    app = FastAPI(
        title="虚拟制片播控中台",
        description="Stage Schema Registry + UDP Live Broadcaster + MCP Tools",
        version="1.0.0",
        lifespan=_make_lifespan(start_mcp),
    )

    @app.post("/schema/upload")
    async def upload_schema(schema: StageSchema):
        registry = get_stage_registry()
        count = registry.register(schema)

        capability_msg = {
            "type": "schema_updated",
            "source": schema.source,
            "version": schema.version,
            "parameter_count": count,
            "parameters": [p.model_dump() for p in registry.all_parameters],
        }

        disconnected: list[WebSocket] = []
        for ws in active_websockets:
            try:
                await ws.send_json(capability_msg)
            except WebSocketDisconnect:
                disconnected.append(ws)
            except Exception:
                disconnected.append(ws)
        for ws in disconnected:
            active_websockets.discard(ws)

        return JSONResponse(
            content={
                "status": "ok",
                "registered": count,
                "message": f"已接收 {count} 个参数",
            }
        )

    @app.get("/schema/list")
    async def list_schema():
        registry = get_stage_registry()
        schema = registry.get_schema()
        if schema is None:
            return JSONResponse(
                content={"status": "empty", "parameters": [], "message": "无可用 Schema"},
                status_code=200,
            )
        return JSONResponse(
            content={
                "status": "ok",
                "source": schema.source,
                "version": schema.version,
                "expired": registry.is_expired(),
                "parameter_count": registry.parameter_count,
                "parameters": [p.model_dump() for p in registry.all_parameters],
            }
        )

    @app.post("/command/send")
    async def send_command(param_name: str, value: float):
        registry = get_stage_registry()
        param = registry.lookup_exact(param_name)
        if param is None:
            param = registry.resolve_cue(param_name)
            if param is None:
                return JSONResponse(
                    content=CommandResult(
                        success=False,
                        param_hash=0,
                        param_name=param_name,
                        value=value,
                        message=f"未找到参数: {param_name}",
                    ).model_dump(),
                    status_code=404,
                )

        param_hash = compute_param_hash(param.name)
        clamp_value = value
        if param.min_value is not None and value < param.min_value:
            clamp_value = param.min_value
        if param.max_value is not None and value > param.max_value:
            clamp_value = param.max_value

        broadcaster = get_broadcaster()
        success = await broadcaster.broadcast(param_hash, clamp_value, retries=2)

        return JSONResponse(
            content=CommandResult(
                success=success,
                param_hash=param_hash,
                param_name=param.name,
                value=clamp_value,
                message="已发送" if success else "发送失败",
            ).model_dump()
        )

    @app.get("/health")
    async def health():
        registry = get_stage_registry()
        return JSONResponse(
            content={
                "status": "ok",
                "schema_cached": registry.parameter_count > 0,
                "parameter_count": registry.parameter_count,
                "feature_count": registry.feature_count,
                "ws_clients": len(active_websockets),
            }
        )

    @app.get("/feature/list")
    async def list_features():
        registry = get_stage_registry()
        features = registry.list_features()
        return JSONResponse(
            content={
                "status": "ok",
                "features": [f.model_dump() for f in features],
                "layout": registry.get_layout().model_dump(),
            }
        )

    @app.post("/feature/compose")
    async def create_feature(feature: StageFeature):
        registry = get_stage_registry()
        existing = registry.get_feature(feature.name)
        if existing is not None:
            return JSONResponse(
                content={"status": "error", "message": f"功能 '{feature.name}' 已存在"},
                status_code=409,
            )

        params = []
        for p in feature.params:
            original = registry.lookup_exact(p.name)
            if original is None:
                return JSONResponse(
                    content={"status": "error", "message": f"UE5 中不存在参数: {p.name}"},
                    status_code=400,
                )
            params.append(original.model_copy())

        feature.params = params
        feature.status = FeatureStatus.IDLE
        registry.register_feature(feature)

        await broadcast_feature_to_frontend(
            AICommandMessage(type=AICommandType.FEATURE_CREATED, payload=feature.model_dump())
        )

        return JSONResponse(
            content={"status": "ok", "feature": feature.model_dump()}
        )

    @app.delete("/feature/{name}/remove")
    async def remove_feature(name: str):
        registry = get_stage_registry()
        feature = registry.get_feature(name)
        if feature is None:
            return JSONResponse(
                content={"status": "error", "message": f"功能 '{name}' 不存在"},
                status_code=404,
            )

        registry.remove_feature(name)
        await broadcast_feature_to_frontend(
            AICommandMessage(
                type=AICommandType.FEATURE_REMOVED,
                payload={"name": name},
            )
        )

        return JSONResponse(content={"status": "ok", "message": f"功能 '{name}' 已删除"})

    @app.post("/feature/{name}/status")
    async def update_feature_status(name: str, status: FeatureStatus):
        registry = get_stage_registry()
        if not registry.set_feature_status(name, status):
            return JSONResponse(
                content={"status": "error", "message": f"功能 '{name}' 不存在"},
                status_code=404,
            )

        feature = registry.get_feature(name)
        await broadcast_feature_to_frontend(
            AICommandMessage(
                type=AICommandType.FEATURE_UPDATED,
                payload=feature.model_dump() if feature else {},
            )
        )

        return JSONResponse(content={"status": "ok", "name": name, "status": status.value})

    @app.websocket("/ws")
    async def websocket_endpoint(ws: WebSocket):
        await ws.accept()
        active_websockets.add(ws)
        logger.info("WebSocket 客户端已连接 (当前 %d 个)", len(active_websockets))

        registry = get_stage_registry()
        schema = registry.get_schema()
        if schema is not None:
            await ws.send_json({
                "type": "schema_current",
                "source": schema.source,
                "version": schema.version,
                "parameter_count": registry.parameter_count,
                "parameters": [p.model_dump() for p in registry.all_parameters],
            })

        features = registry.list_features()
        if features:
            await ws.send_json({
                "type": "features_current",
                "features": [f.model_dump() for f in features],
                "layout": registry.get_layout().model_dump(),
            })

        try:
            while True:
                data = await ws.receive_json()
                msg_type = data.get("type", "")

                if msg_type == "set_parameter":
                    param_name = data.get("param_name", "")
                    value = float(data.get("value", 0.0))
                    param = registry.lookup_exact(param_name)
                    if param is not None:
                        param_hash = compute_param_hash(param.name)
                        broadcaster = get_broadcaster()
                        clamp_value = value
                        if param.min_value is not None and value < param.min_value:
                            clamp_value = param.min_value
                        if param.max_value is not None and value > param.max_value:
                            clamp_value = param.max_value
                        success = await broadcaster.broadcast(param_hash, clamp_value, retries=1)
                        await ws.send_json({
                            "type": "command_result",
                            "param_name": param_name,
                            "value": clamp_value,
                            "success": success,
                        })
                        for feat in registry.list_features():
                            if any(p.name.lower() == param_name.lower() for p in feat.params):
                                registry.set_feature_status(feat.name, FeatureStatus.ACTIVE)

                elif msg_type == "feature_idle":
                    feature_name = data.get("name", "")
                    registry.set_feature_status(feature_name, FeatureStatus.IDLE)
                    feature = registry.get_feature(feature_name)
                    if feature:
                        await broadcast_feature_to_frontend(
                            AICommandMessage(
                                type=AICommandType.FEATURE_UPDATED,
                                payload=feature.model_dump(),
                            )
                        )

                elif msg_type == "ping":
                    await ws.send_json({"type": "pong"})

        except WebSocketDisconnect:
            pass
        except Exception as exc:
            logger.warning("WebSocket 异常: %s", exc)
        finally:
            active_websockets.discard(ws)
            logger.info("WebSocket 客户端已断开 (当前 %d 个)", len(active_websockets))

    return app


app = create_app(start_mcp=True)


def main():
    import uvicorn

    uvicorn.run(
        "broadcast_control.main:app",
        host=HTTP_LISTEN_HOST,
        port=HTTP_LISTEN_PORT,
        log_level="info",
        reload=False,
    )


if __name__ == "__main__":
    main()
