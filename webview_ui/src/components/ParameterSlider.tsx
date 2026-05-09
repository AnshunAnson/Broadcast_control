import React from 'react';

interface ParameterSliderProps {
  name: string;
  value: number;
  min: number;
  max: number;
  label?: string;
  onChange: (value: number) => void;
}

const ParameterSlider: React.FC<ParameterSliderProps> = ({
  name,
  value,
  min,
  max,
  label,
  onChange,
}) => {
  const displayLabel = label || name;
  const displayMin = min ?? 0;
  const displayMax = max ?? 1;

  return (
    <div className="param-slider">
      <div className="param-slider-header">
        <span className="param-slider-label">{displayLabel}</span>
        <span className="param-slider-value">{value.toFixed(3)}</span>
      </div>
      <input
        type="range"
        className="param-slider-input"
        min={displayMin}
        max={displayMax}
        step={(displayMax - displayMin) / 1000}
        value={value}
        onChange={(e) => onChange(parseFloat(e.target.value))}
      />
      <div className="param-slider-range">
        <span>{displayMin.toFixed(1)}</span>
        <span>{displayMax.toFixed(1)}</span>
      </div>
    </div>
  );
};

export default ParameterSlider;