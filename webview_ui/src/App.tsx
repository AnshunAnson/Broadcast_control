import React, { useState } from 'react';
import { useWebSocket } from './hooks/useWebSocket';
import DynamicPanel from './components/DynamicPanel';
import FeatureDashboard from './components/FeatureDashboard';

enum Tab {
  Dashboard = 'dashboard',
  Control = 'control',
  Schema = 'schema',
}

const App: React.FC = () => {
  const {
    connected,
    schema,
    lastResult,
    features,
    layout,
    sendCommand,
    setFeatureIdle,
  } = useWebSocket();
  const [tab, setTab] = useState<Tab>(Tab.Dashboard);

  const handleCommand = (paramName: string, value: number) => {
    sendCommand(paramName, value);
  };

  return (
    <div className="app-container">
      <header className="app-header">
        <div className="app-header-left">
          <h1 className="app-title">虚拟制片播控中台</h1>
          <span className={`app-status ${connected ? 'online' : 'offline'}`}>
            {connected ? '已连接 UE5' : '未连接'}
          </span>
        </div>
        {!connected && (
          <span className="app-reconnect-hint">等待中台连接... (ws://127.0.0.1:8000)</span>
        )}
      </header>

      <nav className="app-tabs">
        <button
          className={`app-tab ${tab === Tab.Dashboard ? 'active' : ''}`}
          onClick={() => setTab(Tab.Dashboard)}
        >
          功能看板
          {features.length > 0 && (
            <span className="app-tab-count">{features.length}</span>
          )}
        </button>
        <button
          className={`app-tab ${tab === Tab.Control ? 'active' : ''}`}
          onClick={() => setTab(Tab.Control)}
        >
          参数控制
          {schema && (
            <span className="app-tab-count">{schema.parameters.length}</span>
          )}
        </button>
        <button
          className={`app-tab ${tab === Tab.Schema ? 'active' : ''}`}
          onClick={() => setTab(Tab.Schema)}
        >
          Schema 查看
        </button>
      </nav>

      <main className="app-main">
        {tab === Tab.Dashboard && (
          <FeatureDashboard
            features={features}
            layout={layout}
            onCommand={handleCommand}
            onSetIdle={setFeatureIdle}
          />
        )}
        {tab === Tab.Control && (
          <DynamicPanel
            parameters={schema?.parameters || []}
            onCommand={handleCommand}
            highlightParams={layout.highlight_params}
          />
        )}
        {tab === Tab.Schema && (
          <div className="schema-view">
            <h2>当前舞台 Schema</h2>
            {schema ? (
              <>
                <div className="schema-meta">
                  <span>来源: {schema.source}</span>
                  <span>版本: v{schema.version}</span>
                  <span>参数数量: {schema.parameters.length}</span>
                  {features.length > 0 && (
                    <span>功能数量: {features.length}</span>
                  )}
                </div>
                <pre className="schema-json">
                  {JSON.stringify(schema, null, 2)}
                </pre>
              </>
            ) : (
              <div className="panel-empty">
                <p>等待 Schema 数据...</p>
              </div>
            )}
          </div>
        )}
      </main>

      {lastResult && (
        <footer className="app-footer">
          <span className={`app-footer-status ${lastResult.success ? 'ok' : 'err'}`}>
            {lastResult.success ? '✓' : '✗'}
          </span>
          <span>{lastResult.param_name} = {lastResult.value}</span>
        </footer>
      )}
    </div>
  );
};

export default App;