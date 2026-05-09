from __future__ import annotations

import os

UDP_TARGET_HOST: str = os.getenv("UDP_TARGET_HOST", "127.0.0.1")
UDP_TARGET_PORT: int = int(os.getenv("UDP_TARGET_PORT", "7000"))
HTTP_LISTEN_HOST: str = os.getenv("HTTP_LISTEN_HOST", "0.0.0.0")
HTTP_LISTEN_PORT: int = int(os.getenv("HTTP_LISTEN_PORT", "8000"))
SCHEMA_TTL_SECONDS: int = int(os.getenv("SCHEMA_TTL_SECONDS", "300"))
SEMANTIC_SEARCH_SIMILARITY_THRESHOLD: float = float(
    os.getenv("SEMANTIC_SEARCH_THRESHOLD", "0.3")
)
MAX_UDP_SEND_RETRIES: int = 3