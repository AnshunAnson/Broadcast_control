"""MCP Server 独立启动入口 — 同时启动 HTTP 服务器以接收 UE5 Schema 上报。

架构定位 (严格遵循 V2.1 架构文档):
  Trae (AI IDE) ──→ MCP (stdio) ──→ 播控中台 (Python FastAPI + MCP Server)
                                         │
                                         ├── HTTP 服务器 (端口 8000) ← UE5 Schema 上报
                                         ├── MCP Server (stdio) ← Trae AI 工具调用
                                         └── StageSchemaRegistry (共享内存缓存)

可通过以下方式启动:
  python -m broadcast_control.mcp_launcher
  python broadcast_control/mcp_launcher.py
"""
from __future__ import annotations

import asyncio
import logging
import sys

import uvicorn

from broadcast_control.config import HTTP_LISTEN_HOST, HTTP_LISTEN_PORT
from broadcast_control.main import create_app
from broadcast_control.mcp_server import run_mcp_server

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
    stream=sys.stderr,
)
logger = logging.getLogger(__name__)


class _UvicornServer:
    """封装 Uvicorn 服务器，支持在 asyncio 任务中启动/停止。"""

    def __init__(self, app, host: str, port: int) -> None:
        self._config = uvicorn.Config(
            app,
            host=host,
            port=port,
            log_level="info",
            reload=False,
        )
        self._server = uvicorn.Server(self._config)

    async def serve(self) -> None:
        await self._server.serve()

    def shutdown(self) -> None:
        self._server.should_exit = True


async def main() -> None:
    app = create_app(start_mcp=False)
    http_server = _UvicornServer(app, HTTP_LISTEN_HOST, HTTP_LISTEN_PORT)

    http_task = asyncio.create_task(http_server.serve(), name="http-server")
    logger.info("HTTP 服务器任务已创建: %s:%d", HTTP_LISTEN_HOST, HTTP_LISTEN_PORT)

    mcp_task = asyncio.create_task(run_mcp_server(), name="mcp-server")
    logger.info("MCP Server 任务已创建 (stdio)")

    done, pending = await asyncio.wait(
        [http_task, mcp_task],
        return_when=asyncio.FIRST_COMPLETED,
    )

    for task in pending:
        task.cancel()
        try:
            await task
        except asyncio.CancelledError:
            pass

    for task in done:
        if task.exception():
            logger.error("任务 %s 异常: %s", task.get_name(), task.exception())
            raise task.exception()


if __name__ == "__main__":
    asyncio.run(main())
