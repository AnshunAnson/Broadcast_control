import React, { useState, useCallback } from 'react';
import type { StageParameter, ParamValueMap } from '../types/schema';
import ParameterSlider from './ParameterSlider';
import ParameterSwitch from './ParameterSwitch';
import ParameterColor from './ParameterColor';
import EventButton from './EventButton';

interface DynamicPanelProps {
  parameters: StageParameter[];
  onCommand: (paramName: string, value: number) => void;
  highlightParams?: string[];
}

function groupByCategory(params: StageParameter[]): Map<string, StageParameter[]> {
  const map = new Map<string, StageParameter[]>();
  for (const p of params) {
    const cat = p.category || 'Default';
    if (!map.has(cat)) {
      map.set(cat, []);
    }
    map.get(cat)!.push(p);
  }
  return map;
}

const DynamicPanel: React.FC<DynamicPanelProps> = ({ parameters, onCommand, highlightParams = [] }) => {
  const [values, setValues] = useState<ParamValueMap>({});

  const getValue = useCallback((param: StageParameter): number => {
    if (param.name in values) {
      return values[param.name];
    }
    return param.default;
  }, [values]);

  const handleChange = useCallback((param: StageParameter, value: number) => {
    const clamped = Math.min(
      Math.max(value, param.min_value ?? 0),
      param.max_value ?? 1,
    );
    setValues((prev) => ({ ...prev, [param.name]: clamped }));
    onCommand(param.name, clamped);
  }, [onCommand]);

  const grouped = groupByCategory(parameters);
  const categories = Array.from(grouped.entries());

  if (parameters.length === 0) {
    return (
      <div className="panel-empty">
        <div className="panel-empty-spinner" />
        <p>等待 UE5 上报舞台参数...</p>
        <p className="panel-empty-hint">
          请确保 UE5 编辑器已启动并加载了 VPBroadcast 插件
        </p>
      </div>
    );
  }

  return (
    <div className="dynamic-panel">
      {categories.map(([category, params]) => (
        <section key={category} className="param-category">
          <h3 className="param-category-title">{category}</h3>
          <div className="param-grid">
            {params.map((param) => {
              const val = getValue(param);
              const isHighlighted = highlightParams.includes(param.name);
              const wrapperClass = isHighlighted ? 'param-wrapper-highlight' : '';

              switch (param.type) {
                case 'float':
                  return (
                    <div key={param.name} className={wrapperClass}>
                      <ParameterSlider
                        name={param.name}
                        value={val}
                        min={param.min_value ?? 0}
                        max={param.max_value ?? 1}
                        label={param.name}
                        onChange={(v) => handleChange(param, v)}
                      />
                    </div>
                  );
                case 'bool':
                  return (
                    <div key={param.name} className={wrapperClass}>
                      <ParameterSwitch
                        name={param.name}
                        value={val}
                        label={param.name}
                        onChange={(v) => handleChange(param, v)}
                      />
                    </div>
                  );
                case 'color':
                  return (
                    <div key={param.name} className={wrapperClass}>
                      <ParameterColor
                        name={param.name}
                        value={val}
                        label={param.name}
                        onChange={(v) => handleChange(param, v)}
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
                        onTrigger={() => handleChange(param, 1.0)}
                      />
                    </div>
                  );
                default:
                  return null;
              }
            })}
          </div>
        </section>
      ))}
    </div>
  );
};

export default DynamicPanel;