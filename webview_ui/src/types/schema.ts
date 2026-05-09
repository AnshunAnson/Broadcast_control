export type ParamType = 'float' | 'bool' | 'color' | 'event';

export interface StageParameter {
  name: string;
  type: ParamType;
  default: number;
  min_value: number | null;
  max_value: number | null;
  tags: string[];
  description: string;
  category: string;
}

export interface StageSchema {
  source: string;
  version: number;
  parameters: StageParameter[];
}

export interface WsMessage {
  type: string;
  source?: string;
  version?: number;
  parameter_count?: number;
  parameters?: StageParameter[];
  param_name?: string;
  value?: number;
  success?: boolean;
  features?: StageFeature[];
  layout?: LayoutConfig;
  name?: string;
  description?: string;
  params?: StageParameter[];
  status?: FeatureStatus;
  feature_order?: string[];
  collapsed_features?: string[];
  highlight_params?: string[];
}

export interface ParamValueMap {
  [name: string]: number;
}

// ==================== AI 编排类型 (V2.1) ====================

export type FeatureStatus = 'active' | 'idle';

export interface StageFeature {
  name: string;
  description: string;
  params: StageParameter[];
  status: FeatureStatus;
}

export interface LayoutConfig {
  feature_order: string[];
  collapsed_features: string[];
  highlight_params: string[];
}