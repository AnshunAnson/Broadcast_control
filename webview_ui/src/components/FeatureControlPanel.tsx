import React from 'react';
import type { StageFeature } from '../types/schema';
import ParameterSlider from './ParameterSlider';
import ParameterSwitch from './ParameterSwitch';
import ParameterColor from './ParameterColor';
import EventButton from './EventButton';

interface FeatureControlPanelProps {
  feature: StageFeature;
  highlightParams: string[];
  onCommand: (paramName: string, value: number) => void;
}

const FeatureControlPanel: React.FC<FeatureControlPanelProps> = ({
  feature,
  highlightParams,
  onCommand,
}) => {
  return (
    <div className="feature-control-panel">
      {feature.params.length === 0 && (
        <p className="feature-control-empty">此功能暂无参数</p>
      )}
      <div className="param-grid">
        {feature.params.map((param) => {
          const isHighlighted = highlightParams.includes(param.name);
          const wrapperClass = isHighlighted ? 'param-wrapper-highlight' : '';

          switch (param.type) {
            case 'float':
              return (
                <div key={param.name} className={wrapperClass}>
                  <ParameterSlider
                    name={param.name}
                    value={param.default}
                    min={param.min_value ?? 0}
                    max={param.max_value ?? 1}
                    label={param.name}
                    onChange={(v) => onCommand(param.name, v)}
                  />
                </div>
              );
            case 'bool':
              return (
                <div key={param.name} className={wrapperClass}>
                  <ParameterSwitch
                    name={param.name}
                    value={param.default}
                    label={param.name}
                    onChange={(v) => onCommand(param.name, v)}
                  />
                </div>
              );
            case 'color':
              return (
                <div key={param.name} className={wrapperClass}>
                  <ParameterColor
                    name={param.name}
                    value={param.default}
                    label={param.name}
                    onChange={(v) => onCommand(param.name, v)}
                  />
                </div>
              );
            case 'event':
              return (
                <div key={param.name} className={wrapperClass}>
                  <EventButton
                    name={param.name}
                    label={param.name}
                    description={param.description}
                    onTrigger={() => onCommand(param.name, 1.0)}
                  />
                </div>
              );
            default:
              return null;
          }
        })}
      </div>
    </div>
  );
};

export default FeatureControlPanel;