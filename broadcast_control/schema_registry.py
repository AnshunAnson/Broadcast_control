from __future__ import annotations

import logging
import time
from difflib import SequenceMatcher

from .config import SCHEMA_TTL_SECONDS, SEMANTIC_SEARCH_SIMILARITY_THRESHOLD
from .models import (
    StageParameter,
    StageSchema,
    StageFeature,
    FeatureStatus,
    LayoutConfig,
    compute_param_hash,
)

logger = logging.getLogger(__name__)


class StageSchemaRegistry:
    """现场状态注册中心 — 内存缓存 UE5 传来的舞台特效清单。

    职责：
    1. 接收并缓存 UE5 启动时通过 HTTP POST 上报的 Schema
    2. 提供参数名 → Hash ID 的精确和语义匹配
    3. 维护 TTL 过期的自动清理机制
    4. 管理 AI 封装的功能 Feature 注册与查询
    5. 管理前端画布布局配置
    """

    def __init__(self) -> None:
        self._schema: StageSchema | None = None
        self._last_updated: float = 0.0
        self._name_index: dict[str, StageParameter] = {}
        self._hash_index: dict[int, StageParameter] = {}
        self._features: dict[str, StageFeature] = {}
        self._layout: LayoutConfig = LayoutConfig()

    def register(self, schema: StageSchema) -> int:
        """接收 UE5 上报的 Schema 并建立索引。

        Returns:
            注册成功的参数数量。
        """
        self._schema = schema
        self._last_updated = time.monotonic()
        self._name_index.clear()
        self._hash_index.clear()

        for param in schema.parameters:
            name_lower = param.name.lower()
            self._name_index[name_lower] = param
            param_hash = compute_param_hash(param.name)
            self._hash_index[param_hash] = param

        logger.info(
            "Schema 已注册: source=%s version=%d parameters=%d",
            schema.source,
            schema.version,
            len(schema.parameters),
        )
        return len(schema.parameters)

    def is_expired(self) -> bool:
        if self._last_updated == 0.0:
            return True
        return (time.monotonic() - self._last_updated) > SCHEMA_TTL_SECONDS

    def get_schema(self) -> StageSchema | None:
        if self.is_expired():
            logger.warning("Schema 已过期，等待 UE5 重新上报")
        return self._schema

    def lookup_exact(self, name: str) -> StageParameter | None:
        """按参数名精确查找。"""
        return self._name_index.get(name.lower())

    def lookup_by_hash(self, param_hash: int) -> StageParameter | None:
        """按 FNV-1a Hash 查找。"""
        return self._hash_index.get(param_hash)

    def search_semantic(self, query: str, top_k: int = 5) -> list[tuple[StageParameter, float]]:
        """语义检索 — 在参数名和 tags 中模糊匹配。

        匹配策略（优先级从高到低）：
        1. 参数名精确子串匹配 → 权重 1.0
        2. 参数名部分词匹配 → SequenceMatcher 相似度
        3. Tags 子串匹配 → 权重 0.7 × 最佳相似度

        Returns:
            [(parameter, score), ...] 按相似度降序排列。
        """
        query_lower = query.lower()
        candidates: list[tuple[StageParameter, float]] = []

        for param_name, param in self._name_index.items():
            score = 0.0

            # 策略 1: 精确子串
            if query_lower in param_name:
                score = max(score, 1.0)

            # 策略 2: 参数名模糊匹配
            name_sim = SequenceMatcher(None, query_lower, param_name).ratio()
            score = max(score, name_sim)

            # 策略 3: Tags 匹配
            for tag in param.tags:
                tag_lower = tag.lower()
                if query_lower in tag_lower or tag_lower in query_lower:
                    tag_sim = SequenceMatcher(None, query_lower, tag_lower).ratio()
                    score = max(score, max(0.7, tag_sim))
                else:
                    tag_sim = SequenceMatcher(None, query_lower, tag_lower).ratio()
                    score = max(score, tag_sim * 0.7)

            if score >= SEMANTIC_SEARCH_SIMILARITY_THRESHOLD:
                candidates.append((param, score))

        candidates.sort(key=lambda x: x[1], reverse=True)
        return candidates[:top_k]

    def resolve_cue(self, cue_name: str) -> StageParameter | None:
        """智能导播语义解析：将自然语言 cue 名映射到参数。

        先用精确查找，失败后走语义检索取最高分匹配。
        """
        exact = self.lookup_exact(cue_name)
        if exact is not None:
            return exact

        candidates = self.search_semantic(cue_name, top_k=1)
        if candidates:
            param, score = candidates[0]
            logger.info("语义匹配: '%s' → '%s' (score=%.2f)", cue_name, param.name, score)
            return param

        return None

    @property
    def parameter_count(self) -> int:
        return len(self._name_index)

    @property
    def all_parameters(self) -> list[StageParameter]:
        return list(self._name_index.values())

    # ==================== Feature Registry (V2.1) ====================

    def register_feature(self, feature: StageFeature) -> None:
        """AI 调用 compose_feature 后，注册一个功能封装。"""
        key = feature.name.lower()
        self._features[key] = feature
        logger.info(
            "Feature 已注册: '%s' (%d 个参数)",
            feature.name,
            len(feature.params),
        )

    def remove_feature(self, name: str) -> bool:
        """删除一个功能封装。"""
        key = name.lower()
        if key in self._features:
            del self._features[key]
            logger.info("Feature 已移除: '%s'", name)
            return True
        return False

    def get_feature(self, name: str) -> StageFeature | None:
        """按名称获取功能封装。"""
        return self._features.get(name.lower())

    def list_features(self) -> list[StageFeature]:
        """列出所有 AI 封装的功能。"""
        features = list(self._features.values())
        if self._layout.feature_order:
            order_map = {n.lower(): i for i, n in enumerate(self._layout.feature_order)}
            features.sort(key=lambda f: order_map.get(f.name.lower(), 999))
        return features

    @property
    def feature_count(self) -> int:
        return len(self._features)

    # ==================== Layout Config (V2.1) ====================

    def update_layout(self, layout: LayoutConfig) -> None:
        """AI 调用 configure_layout 时更新布局配置。"""
        self._layout = layout
        logger.info(
            "Layout 已更新: order=%s collapsed=%s highlights=%s",
            layout.feature_order,
            layout.collapsed_features,
            layout.highlight_params,
        )

    def get_layout(self) -> LayoutConfig:
        return self._layout

    def set_feature_status(self, name: str, status: FeatureStatus) -> bool:
        """更新功能运行状态（人类操控时触发）。"""
        key = name.lower()
        feature = self._features.get(key)
        if feature is None:
            return False
        feature.status = status
        return True


_stage_registry: StageSchemaRegistry | None = None


def get_stage_registry() -> StageSchemaRegistry:
    global _stage_registry
    if _stage_registry is None:
        _stage_registry = StageSchemaRegistry()
    return _stage_registry