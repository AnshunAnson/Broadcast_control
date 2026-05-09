import React from 'react';

interface EventButtonProps {
  name: string;
  label?: string;
  description?: string;
  onTrigger: () => void;
}

const EventButton: React.FC<EventButtonProps> = ({
  name,
  label,
  description,
  onTrigger,
}) => {
  const displayLabel = label || name;

  return (
    <div className="param-event">
      <button
        className="param-event-btn"
        onClick={onTrigger}
        title={description || displayLabel}
      >
        <span className="param-event-icon">&#9654;</span>
        <span className="param-event-label">{displayLabel}</span>
      </button>
      {description && (
        <p className="param-event-desc">{description}</p>
      )}
    </div>
  );
};

export default EventButton;