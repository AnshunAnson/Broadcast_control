import { useEffect, useRef, useCallback, useState } from 'react';
import type { WsMessage, StageSchema, StageFeature, LayoutConfig } from '../types/schema';

const WS_URL = 'ws://127.0.0.1:8000/ws';
const RECONNECT_DELAY_MS = 3000;
const MAX_RECONNECT_ATTEMPTS = 10;

export function useWebSocket() {
  const wsRef = useRef<WebSocket | null>(null);
  const reconnectAttempts = useRef(0);
  const reconnectTimer = useRef<ReturnType<typeof setTimeout> | null>(null);

  const [connected, setConnected] = useState(false);
  const [schema, setSchema] = useState<StageSchema | null>(null);
  const [lastResult, setLastResult] = useState<WsMessage | null>(null);
  const [features, setFeatures] = useState<StageFeature[]>([]);
  const [layout, setLayout] = useState<LayoutConfig>({
    feature_order: [],
    collapsed_features: [],
    highlight_params: [],
  });

  const connect = useCallback(() => {
    if (wsRef.current?.readyState === WebSocket.OPEN) {
      return;
    }

    const ws = new WebSocket(WS_URL);
    wsRef.current = ws;

    ws.onopen = () => {
      setConnected(true);
      reconnectAttempts.current = 0;
    };

    ws.onmessage = (event) => {
      try {
        const msg: WsMessage = JSON.parse(event.data);
        switch (msg.type) {
          case 'schema_current':
          case 'schema_updated':
            setSchema({
              source: msg.source || 'unknown',
              version: msg.version || 1,
              parameters: msg.parameters || [],
            });
            break;
          case 'features_current':
            setFeatures(msg.features || []);
            if (msg.layout) {
              setLayout(msg.layout);
            }
            break;
          case 'feature_created':
            if (msg.name && msg.params) {
              setFeatures((prev) => {
                const exists = prev.find((f) => f.name === msg.name);
                if (exists) {
                  return prev.map((f) =>
                    f.name === msg.name
                      ? { ...f, ...msg as unknown as StageFeature }
                      : f
                  );
                }
                return [...prev, msg as unknown as StageFeature];
              });
            }
            break;
          case 'feature_removed':
            if (msg.name) {
              setFeatures((prev) => prev.filter((f) => f.name !== msg.name));
            }
            break;
          case 'feature_updated':
            if (msg.name) {
              setFeatures((prev) =>
                prev.map((f) =>
                  f.name === msg.name
                    ? { ...f, ...msg as unknown as StageFeature }
                    : f
                )
              );
            }
            break;
          case 'layout_updated':
            setLayout({
              feature_order: msg.feature_order || [],
              collapsed_features: msg.collapsed_features || [],
              highlight_params: msg.highlight_params || [],
            });
            break;
          case 'command_result':
            setLastResult(msg);
            break;
        }
      } catch {
      }
    };

    ws.onclose = () => {
      setConnected(false);
      wsRef.current = null;
      scheduleReconnect();
    };

    ws.onerror = () => {
      ws.close();
    };
  }, []);

  const scheduleReconnect = useCallback(() => {
    if (reconnectTimer.current) {
      clearTimeout(reconnectTimer.current);
    }
    if (reconnectAttempts.current >= MAX_RECONNECT_ATTEMPTS) {
      return;
    }
    reconnectAttempts.current += 1;
    reconnectTimer.current = setTimeout(() => {
      connect();
    }, RECONNECT_DELAY_MS);
  }, [connect]);

  const sendCommand = useCallback((paramName: string, value: number) => {
    const ws = wsRef.current;
    if (!ws || ws.readyState !== WebSocket.OPEN) {
      return;
    }
    ws.send(JSON.stringify({
      type: 'set_parameter',
      param_name: paramName,
      value,
    }));
  }, []);

  const setFeatureIdle = useCallback((name: string) => {
    const ws = wsRef.current;
    if (!ws || ws.readyState !== WebSocket.OPEN) {
      return;
    }
    ws.send(JSON.stringify({
      type: 'feature_idle',
      name,
    }));
    setFeatures((prev) =>
      prev.map((f) => (f.name === name ? { ...f, status: 'idle' as const } : f))
    );
  }, []);

  useEffect(() => {
    connect();
    return () => {
      if (reconnectTimer.current) {
        clearTimeout(reconnectTimer.current);
      }
      wsRef.current?.close();
    };
  }, [connect]);

  return {
    connected,
    schema,
    lastResult,
    features,
    layout,
    sendCommand,
    setFeatureIdle,
    reconnect: connect,
  };
}