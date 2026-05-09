import React from 'react';

interface ParameterColorProps {
  name: string;
  value: number;
  label?: string;
  onChange: (value: number) => void;
}

const PRESET_COLORS = [
  { name: 'Red', value: 0.1 },
  { name: 'Orange', value: 0.2 },
  { name: 'Yellow', value: 0.3 },
  { name: 'Green', value: 0.4 },
  { name: 'Cyan', value: 0.5 },
  { name: 'Blue', value: 0.6 },
  { name: 'Purple', value: 0.7 },
  { name: 'Magenta', value: 0.8 },
  { name: 'White', value: 0.9 },
  { name: 'Warm', value: 1.0 },
];

const ParameterColor: React.FC<ParameterColorProps> = ({
  name,
  value,
  label,
  onChange,
}) => {
  const displayLabel = label || name;

  return (
    <div className="param-color">
      <span className="param-color-label">{displayLabel}</span>
      <div className="param-color-presets">
        {PRESET_COLORS.map((c) => (
          <button
            key={c.name}
            className={`param-color-btn ${value === c.value ? 'active' : ''}`}
            style={{ '--color-val': c.value } as React.CSSProperties}
            title={c.name}
            onClick={() => onChange(c.value)}
          >
            <span className="param-color-swatch" style={{
              background: `hsl(${(c.value * 360) % 360}, 80%, 55%)`,
            }} />
          </button>
        ))}
      </div>
      <div className="param-color-slider">
        <input
          type="range"
          min={0}
          max={1}
          step={0.01}
          value={value}
          onChange={(e) => onChange(parseFloat(e.target.value))}
        />
      </div>
    </div>
  );
};

export default ParameterColor;