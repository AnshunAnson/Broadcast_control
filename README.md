# VPBroadcast — AI 驱动虚拟制片播控中台

实时连接 AI / Web 前端与 Unreal Engine 5 舞台参数的端到端播控系统。

## 背景

虚拟制片（Virtual Production）场景中，需要实时控制 UE5 舞台的灯光、全息、特效等参数。传统方式需要美术人员在 UE5 编辑器中手动调整，效率低且无法与 AI 协作。VPBroadcast 通过三层架构解决了这个问题：

- **AI 只做功能封装**：读取 UE5 参数 → 按逻辑组合为 Feature → 推送到前端
- **UE5 是参数唯一源头**：参数名、类型、默认值、范围、标签全部由 UE5 定义
- **前端面向人类**：人类在画布上调整参数值，触发 UE5 渲染

## 系统架构

```
┌──────────────────────────────────────────────────────────────────┐
│                      Agent 控制端                                 │
│  ┌──────────────────┐    ┌──────────────────────────────────┐    │
│  │ Trae AI (MCP)    │    │ React Webview (人类功能画布)      │    │
│  │ 自然语言控场      │    │ 功能看板 / 参数控制 / Schema      │    │
│  └────────┬─────────┘    └──────────────┬───────────────────┘    │
│           │ MCP (stdio)                 │ WebSocket               │
├───────────┴─────────────────────────────┴────────────────────────┤
│                    播控中台 (Python FastAPI)                       │
│  Schema 缓存 → Feature 注册 → 语义检索 → 8字节 UDP 打包 → 发送    │
├──────────────────────────────────────────────────────────────────┤
│                    渲染端 (UE5 C++ Plugin)                        │
│  VPBroadcastSubsystem → UDP 接收 → FProperty 反射 → 材质/灯光     │
└──────────────────────────────────────────────────────────────────┘
```

## 核心数据流

**单路径、零冗余、即时生效：**

```
前端/AI 调参 → Python UDP 发包(8字节) → VPBroadcastSubsystem 反射设置属性 → 通知 Actor/Component 应用 → 立即生效
```

### 通信协议

| 通道 | 协议 | 格式 | 方向 | 用途 |
|------|------|------|------|------|
| MCP | stdio | JSON-RPC | AI ↔ 中台 | AI 自然语言控场 + 功能封装 |
| WebSocket | ws | JSON | 前端 ↔ 中台 | UI 实时交互 + Feature 同步 |
| HTTP | POST | JSON | UE5 → 中台 | Schema 上报 |
| UDP | Raw | 8 Bytes | 中台 → UE5 | 实时参数指令 |

### UDP 包格式（8 字节大端序）

```
[  uint32 ParamHash (FNV-1a)  ][  float32 Value  ]
  Byte 0  1  2  3               4   5   6   7
```

## 技术栈

| 层 | 技术 | 端口/协议 |
|----|------|-----------|
| UE5 插件 | C++ / UE 5.1 / Reflection / Niagara | UDP 7000 接收 |
| Python 中台 | FastAPI + uvicorn + MCP SDK | HTTP 8000 / WebSocket |
| Web 前端 | React 18 + TypeScript + Vite | HTTP 5173 |

## 项目结构

```
Broadcast_control/
├── BrodcastMVP/                          # UE5 项目
│   ├── BrodcastMVP.uproject
│   └── plugins/VPBroadcast/              # UE5 插件
│       └── Source/VPBroadcast/
│           ├── Public/
│           │   ├── VPBroadcastTypes.h    # 类型定义 + FNV1a Hash + 常量
│           │   ├── VPBroadcastSubsystem.h # 中枢：UDP接收 + 反射 + Schema上报
│           │   ├── VPStageEffectActor.h  # 舞台效果Actor（全息/灯光/特效）
│           │   ├── VPAgentComponent.h    # 通用Agent组件（蓝图可扩展）
│           │   ├── VPAgentMetadata.h     # Schema扫描器
│           │   └── VPStageUDPReceiver.h  # UDP接收线程
│           └── Private/
│               ├── VPBroadcastSubsystem.cpp
│               ├── VPStageEffectActor.cpp
│               ├── VPAgentComponent.cpp
│               ├── VPAgentMetadata.cpp
│               └── VPStageUDPReceiver.cpp
├── broadcast_control/                    # Python 中台
│   ├── main.py                           # FastAPI 入口 + WebSocket
│   ├── config.py                         # 配置（端口/地址）
│   ├── mcp_server.py                     # MCP 工具服务器（5个工具）
│   ├── mcp_launcher.py                   # MCP 独立启动入口
│   ├── models.py                         # Pydantic 数据模型 + Hash + UDP打包
│   ├── schema_registry.py                # Schema 注册表 + Feature + 语义检索
│   ├── udp_sender.py                     # UDP 发包器
│   └── pyproject.toml
├── webview_ui/                           # Web 前端
│   ├── src/
│   │   ├── App.tsx                       # 主布局：三Tab
│   │   ├── components/
│   │   │   ├── FeatureDashboard.tsx      # 功能看板
│   │   │   ├── FeatureControlPanel.tsx   # 参数控制面板
│   │   │   ├── DynamicPanel.tsx          # 全量参数面板
│   │   │   ├── ParameterSlider.tsx       # 滑块控件
│   │   │   ├── ParameterColor.tsx        # 颜色控件
│   │   │   ├── ParameterSwitch.tsx       # 开关控件
│   │   │   └── EventButton.tsx           # 事件触发按钮
│   │   ├── hooks/useWebSocket.ts         # WebSocket 连接
│   │   └── types/schema.ts              # 类型定义
│   └── package.json
├── tests/                                # 测试
│   ├── test_core.py                      # 核心模块验证
│   ├── test_integration.py               # 集成测试
│   └── test_verification.py              # 全线功能验证
├── start.ps1                             # 一键启动脚本
└── README.md
```

## UE5 插件模块说明

### VPBroadcastSubsystem（中枢）

`UGameInstanceSubsystem`，系统级单例：

- **UDP 接收**：监听端口 7000，接收 8 字节 `(ParamHash, Value)` 包
- **反射设置属性**：通过 `SetPropertyValue_InContainer` 直接写入 Actor/Component 属性
- **即时应用**：设置后通知 VPStageEffectActor / VPAgentComponent 调用 `ApplyAllParameters()`
- **Schema 上报**：启动时自动扫描所有 `VPAgent|` 类别属性，上报到 Python 中台

### VPStageEffectActor（舞台效果）

预置舞台效果 Actor，包含 AR 全息、环境光、舞台事件三组参数。蓝图事件：
- `On Parameter Changed` — 单个参数变化时触发
- `On Any Agent Parameter Changed` — 任何参数变化时触发

### VPAgentComponent（通用 Agent 组件）

可挂载到任意 Actor 的通用组件，支持蓝图自由扩展参数：
- 自动同步属性到 `MaterialParameterCollection`
- 自动同步到 Actor 的 `StaticMeshComponent` / `PointLightComponent`
- `OnParametersChanged` / `OnEffectTriggered` 蓝图事件

### VPAgentMetadataScanner（Schema 扫描器）

静态工具类，扫描 UE5 反射系统：
- 自动发现所有带 `VPAgent|` Category 标签的 `UPROPERTY`
- 读取 `VPA_Tags` / `VPA_Description` / `ClampMin` / `ClampMax` 等元数据
- 生成 JSON Schema 上报到 Python 中台

## MCP 工具清单

| 工具名 | 参数 | 用途 |
|--------|------|------|
| `get_capabilities` | 无 | 获取 UE5 全量参数清单 + 已有 Feature |
| `compose_feature` | `name`, `description`, `param_names` | AI 将 UE5 参数封装为功能 Feature |
| `configure_layout` | `feature_order`, `collapsed`, `highlights` | AI 编排前端画布布局 |
| `set_float_parameter` | `param_name`, `value` | 精确参数控制（快捷调试通道） |
| `execute_stage_cue` | `cue_name`, `value`, `target` | 语义解析 → 控前端/控 UE5 |

## HTTP API

| 方法 | 路径 | 描述 |
|------|------|------|
| POST | `/schema/upload` | UE5 上报参数清单 |
| GET | `/schema/list` | 获取当前所有参数 |
| POST | `/command/send` | 发送控制指令（支持语义匹配） |
| GET | `/feature/list` | 获取所有 Feature + Layout |
| POST | `/feature/compose` | 创建 Feature |
| DELETE | `/feature/{name}/remove` | 删除 Feature |
| POST | `/feature/{name}/status` | 更新 Feature 状态 |
| GET | `/health` | 健康检查 |
| WS | `/ws` | WebSocket 实时双向通信 |

## 快速启动

### 前置条件

- Unreal Engine 5.1 (`D:\UE\UE_5.1`)
- Python 3.10+
- Node.js 18+

### 一键启动

```powershell
.\start.ps1
```

### 手动启动

```powershell
# 1. 编译 UE5 插件
& "D:\UE\UE_5.1\Engine\Build\BatchFiles\Build.bat" BrodcastMVPEditor Win64 Development "D:\UE\Project\MyProject\Broadcast_control\BrodcastMVP\BrodcastMVP.uproject" -WaitMutex -FromMsBuild

# 2. 启动 Python 中台
python -m broadcast_control.main

# 3. 启动 Web 前端
cd webview_ui && npm install && npm run dev

# 4. 启动 UE5 编辑器，打开关卡后点击 Play
```

## 扩展指南

### 添加新的可控参数

1. 在 `VPStageEffectActor.h` 或蓝图子类中添加 `UPROPERTY`：

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VPAgent|新分类",
    meta = (ClampMin = "0.0", ClampMax = "1.0",
            VPA_Tags = "标签1,标签2",
            VPA_Description = "参数描述"))
float My_New_Param = 0.5f;
```

2. 在 `ApplyAllParameters()` 中添加应用逻辑
3. 重新编译，Schema 会自动上报到前端

### VPAgent 元数据标签

| 标签 | 必须 | 用途 | 示例 |
|------|------|------|------|
| `Category = "VPAgent\|xxx"` | **必须** | 激活自动发现 | `VPAgent\|灯光控制` |
| `ClampMin` / `ClampMax` | 推荐 | 值范围约束 | `0.0` / `1.0` |
| `VPA_Tags` | 推荐 | AI 语义匹配标签 | `亮度,发光,Emissive` |
| `VPA_Description` | 可选 | 参数说明 | `控制舞台主灯亮度` |

## 配置

| 配置项 | 默认值 | 环境变量 |
|--------|--------|----------|
| UDP 目标地址 | 127.0.0.1 | `UDP_TARGET_HOST` |
| UDP 目标端口 | 7000 | `UDP_TARGET_PORT` |
| HTTP 监听地址 | 0.0.0.0 | `HTTP_LISTEN_HOST` |
| HTTP 监听端口 | 8000 | `HTTP_LISTEN_PORT` |
| Schema TTL | 300s | `SCHEMA_TTL_SECONDS` |
| 语义匹配阈值 | 0.3 | `SEMANTIC_SEARCH_THRESHOLD` |

## License

Private — Internal Use Only
