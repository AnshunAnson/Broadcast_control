import React, { useState } from 'react';
import type { StageFeature, LayoutConfig } from '../types/schema';
import FeatureControlPanel from './FeatureControlPanel';

interface FeatureDashboardProps {
  features: StageFeature[];
  layout: LayoutConfig;
  onCommand: (paramName: string, value: number) => void;
  onSetIdle: (name: string) => void;
}

const FeatureDashboard: React.FC<FeatureDashboardProps> = ({
  features,
  layout,
  onCommand,
  onSetIdle,
}) => {
  const [expanded, setExpanded] = useState<Set<string>>(new Set());

  const toggleExpand = (name: string) => {
    setExpanded((prev) => {
      const next = new Set(prev);
      if (next.has(name)) {
        next.delete(name);
      } else {
        next.add(name);
      }
      return next;
    });
  };

  const sorted = [...features].sort((a, b) => {
    const ia = layout.feature_order.indexOf(a.name);
    const ib = layout.feature_order.indexOf(b.name);
    if (ia >= 0 && ib >= 0) return ia - ib;
    if (ia >= 0) return -1;
    if (ib >= 0) return 1;
    return a.name.localeCompare(b.name);
  });

  const activeCount = features.filter((f) => f.status === 'active').length;
  const idleCount = features.filter((f) => f.status === 'idle').length;

  if (features.length === 0) {
    return (
      <div className="feature-dashboard-empty">
        <div className="feature-dashboard-empty-icon">⚙</div>
        <h3>功能执行看板</h3>
        <p>暂无功能封装。AI 正在等待你的需求...</p>
        <p className="feature-dashboard-hint">
          例如对 Trae 说："我要控制场景灯光" → AI 自动封装相关参数
        </p>
      </div>
    );
  }

  return (
    <div className="feature-dashboard">
      <div className="feature-dashboard-header">
        <h2 className="feature-dashboard-title">功能执行看板</h2>
        <div className="feature-dashboard-stats">
          <span className="feature-stat active">● {activeCount} 活跃</span>
          <span className="feature-stat idle">○ {idleCount} 待命</span>
          <span className="feature-stat total">{features.length} 功能</span>
        </div>
      </div>

      <div className="feature-grid">
        {sorted.map((feature) => {
          const isExpanded = expanded.has(feature.name) ||
            !layout.collapsed_features.includes(feature.name);
          const isActive = feature.status === 'active';

          return (
            <div
              key={feature.name}
              className={`feature-card ${isActive ? 'feature-card-active' : ''}`}
            >
              <div
                className="feature-card-header"
                onClick={() => toggleExpand(feature.name)}
              >
                <div className="feature-card-status">
                  <span
                    className={`feature-status-dot ${isActive ? 'dot-active' : 'dot-idle'}`}
                  />
                  <span className="feature-card-name">{feature.name}</span>
                </div>
                <div className="feature-card-meta">
                  <span className="feature-card-count">
                    {feature.params.length} 参数
                  </span>
                  <span className="feature-card-toggle">
                    {isExpanded ? '收起' : '展开'}
                  </span>
                </div>
              </div>

              {feature.description && (
                <p className="feature-card-desc">{feature.description}</p>
              )}

              {isExpanded && (
                <FeatureControlPanel
                  feature={feature}
                  highlightParams={layout.highlight_params}
                  onCommand={(paramName, value) => {
                    onCommand(paramName, value);
                    if (!isActive) {
                      onSetIdle(feature.name);
                    }
                  }}
                />
              )}
            </div>
          );
        })}
      </div>
    </div>
  );
};

export default FeatureDashboard;