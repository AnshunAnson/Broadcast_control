"""集成测试：验证 MCP 工具与 HTTP 服务共享 StageSchemaRegistry。"""
import asyncio
import threading

import uvicorn

from broadcast_control.config import HTTP_LISTEN_HOST, HTTP_LISTEN_PORT
from broadcast_control.main import create_app
from broadcast_control.models import StageSchema, StageParameter, ParamType
from broadcast_control.mcp_server import _handle_get_capabilities
from broadcast_control.schema_registry import get_stage_registry


def _run_http_server():
    app = create_app(start_mcp=False)
    uvicorn.run(app, host=HTTP_LISTEN_HOST, port=HTTP_LISTEN_PORT, log_level="warning", reload=False)


async def test_shared_registry():
    http_thread = threading.Thread(target=_run_http_server, daemon=True)
    http_thread.start()
    await asyncio.sleep(1)

    registry = get_stage_registry()
    assert registry.parameter_count == 0, "初始应为空"

    schema = StageSchema(
        source="ue5",
        version=1,
        parameters=[
            StageParameter(
                name="Test_Param",
                type=ParamType.FLOAT,
                default=0.5,
                min_value=0.0,
                max_value=1.0,
                tags=["测试"],
                description="测试参数",
                category="Test",
            )
        ],
    )

    import httpx
    async with httpx.AsyncClient() as client:
        resp = await client.post(
            f"http://127.0.0.1:{HTTP_LISTEN_PORT}/schema/upload",
            json=schema.model_dump(),
        )
        assert resp.status_code == 200
        data = resp.json()
        assert data["registered"] == 1

    assert registry.parameter_count == 1, "HTTP 上报后 Registry 应有 1 个参数"

    result = await _handle_get_capabilities({})
    text = result[0].text
    assert "Test_Param" in text, f"MCP get_capabilities 应返回 Test_Param，实际: {text}"

    print("PASS: HTTP 和 MCP 共享同一个 StageSchemaRegistry")


if __name__ == "__main__":
    asyncio.run(test_shared_registry())
