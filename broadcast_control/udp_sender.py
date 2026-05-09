from __future__ import annotations

import asyncio
import logging

from .config import UDP_TARGET_HOST, UDP_TARGET_PORT, MAX_UDP_SEND_RETRIES
from .models import pack_udp_message

logger = logging.getLogger(__name__)


class UDPLiveBroadcaster:
    """UDP 异步发送器 — 非阻塞地将控制指令发送给 UE5 渲染端。

    协议: 8 字节大端序 — uint32 (FNV-1a Param Hash) + float32 (Value)

    职责:
    1. 管理一个长期复用的 UDP 传输对象
    2. 直接连接 UE5 的 UDP 监听端口
    3. 支持高频重发（丢包容忍）
    """

    def __init__(self) -> None:
        self._transport: asyncio.DatagramTransport | None = None
        self._lock = asyncio.Lock()
        self._connected = False

    async def connect(self) -> None:
        """建立 UDP 传输连接。复用同一个 transport 减少开销。"""
        if self._connected and self._transport is not None:
            return

        loop = asyncio.get_running_loop()
        remote_addr = (UDP_TARGET_HOST, UDP_TARGET_PORT)

        self._transport, _ = await loop.create_datagram_endpoint(
            lambda: _SimpleDatagramProtocol(),
            remote_addr=remote_addr,
        )
        self._connected = True
        logger.info("UDP Broadcaster 已连接: %s:%d", UDP_TARGET_HOST, UDP_TARGET_PORT)

    async def broadcast(
        self, param_hash: int, value: float, retries: int = 0
    ) -> bool:
        """发送一条 8 字节控制指令到 UE5。

        Args:
            param_hash: FNV-1a 32-bit 参数 Hash ID
            value: 浮点控制值
            retries: 额外重发次数（用于丢包容忍）

        Returns:
            是否成功投递。
        """
        if not self._connected or self._transport is None:
            await self.connect()

        packet = pack_udp_message(param_hash, value)
        total_attempts = 1 + retries

        async with self._lock:
            for attempt in range(total_attempts):
                try:
                    self._transport.sendto(packet)
                    logger.debug(
                        "UDP 发送: hash=0x%08X value=%.4f (%d/%d)",
                        param_hash,
                        value,
                        attempt + 1,
                        total_attempts,
                    )
                    return True
                except OSError as exc:
                    logger.warning(
                        "UDP 发送失败 (%d/%d): %s",
                        attempt + 1,
                        total_attempts,
                        exc,
                    )
                    if attempt < total_attempts - 1:
                        await asyncio.sleep(0.001)

        return False

    async def close(self) -> None:
        if self._transport is not None:
            self._transport.close()
            self._transport = None
            self._connected = False
            logger.info("UDP Broadcaster 已关闭")


class _SimpleDatagramProtocol(asyncio.DatagramProtocol):
    """极简 UDP 协议处理器，不需处理接收逻辑。"""

    def connection_made(self, transport: asyncio.DatagramTransport) -> None:
        pass

    def datagram_received(self, data: bytes, addr: tuple) -> None:
        pass

    def error_received(self, exc: Exception) -> None:
        logger.error("UDP 错误: %s", exc)


_broadcaster: UDPLiveBroadcaster | None = None


def get_broadcaster() -> UDPLiveBroadcaster:
    global _broadcaster
    if _broadcaster is None:
        _broadcaster = UDPLiveBroadcaster()
    return _broadcaster