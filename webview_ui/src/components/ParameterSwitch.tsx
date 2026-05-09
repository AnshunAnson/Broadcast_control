import React from 'react';

interface ParameterSwitchProps {
  name: string;
  value: number;
  label?: string;
  onChange: (value: number) => void;
}

const ParameterSwitch: React.FC<ParameterSwitchProps> = ({
  name,
  value,
  label,
  onChange,
}) => {
  const displayLabel = label || name;
  const isOn = value >= 0.5;

  return (
    <div className="param-switch">
      <span className="param-switch-label">{displayLabel}</span>
      <button
        className={`param-switch-btn ${isOn ? 'on' : 'off'}`}
        onClick={() => onChange(isOn ? 0.0 : 1.0)}
      >
        <span className="param-switch-track" />
        <span className="param-switch-state">{isOn ? 'ON' : 'OFF'}</span>
      </button>
    </div>
  );
};

export default ParameterSwitch;