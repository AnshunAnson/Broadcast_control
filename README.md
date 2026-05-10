# VPBroadcast — AI 驱动虚拟制片播控中台

实时连接 AI / Web 前端与 Unreal Engine 5 舞台参数的端到端播控系统。

## 背景

虚拟制片（Virtual Production）场景中，需要实时控制 UE5 舞台的灯光、全息、特效等参数。传统方式需要美术人员在 UE5 编辑器中手动调整，效率低且无法与 AI 协作。VPBroadcast 通过三层架构解决了这个问题：

- **AI 只做功能封装**：读取 UE5 参数 → 按逻辑组合为 Feature → 推送到前端
- **UE5 是参数唯一源头**：参数名、类型、默认值、范围、标签全部由 UE5 定义
- **前端面向人类**：人类在画布上调整参数值，触发 UE5 渲染
- **⚠️ AI 禁止直接控场**：AI 只能根据用户需求组织播控逻辑，绝对不能自动执行任何控场操作

## 系统架构

```
┌──────────────────────────────────────────────────────────────────┐
│                      Agent 控制端                                 │
│  ┌──────────────────┐    ┌──────────────────────────────────┐    │
│  │ Trae AI (MCP)    │    │ React Webview (人类功能画布)      │    │
│  │ 组织播控逻辑      │    │ 功能看板 / 参数控制 / Schema      │    │
│  │ (不能直接控场)    │    │ (人类触发操作)                    │    │
│  └────────┬─────────┘    └──────────────┬───────────────────┘    │
│           │ MCP (stdio)                 │ WebSocket               │
├───────────┴─────────────────────────────┴────────────────────────┤
│                    播控中台 (Python FastAPI)                       │
│  Schema 缓存 → Feature 注册 → 语义检索 → 8字节 UDP 打包 → 发送    │
├──────────────────────────────────────────────────────────────────┤
│                    渲染端 (UE5 C++ Plugin)                        │
│  VPBroadcastSubsystem → UDP 接收 → FProperty 反射 → 触发蓝图事件  │
└──────────────────────────────────────────────────────────────────┘
```

## 核心设计理念：事件驱动，蓝图执行

**C++ 只管"传值"，蓝图管"执行"。** 和 Widget 滑条的 `On Value Changed` 一样：

```
前端调参 → Python UDP 发包 → UE5 设值 → 触发蓝图事件 → 蓝图决定做什么
```

C++ 框架的职责边界：
- ✅ 接收 UDP 数据包
- ✅ 通过反射写入属性值
- ✅ 触发蓝图事件（通知参数名和新值）
- ❌ 不自动执行任何业务逻辑

蓝图的职责：
- ✅ 在事件中决定如何响应参数变化
- ✅ 调用 `ApplyAllParameters()` 使用默认行为，或自己写逻辑
- ✅ 播放动画、音效、触发其他 Actor 等自定义功能
- ✅ 在蓝图子类中添加新变量（Category 设为 `VPAgent|...` 即可被自动发现）

## 核心约束

| 约束 | 说明 |
|------|------|
| UE5 是参数唯一源头 | 参数名、类型、默认值、范围、标签全部由 UE5 `VPAgent|` 属性定义 |
| **⚠️ AI 禁止直接控场** | **AI 绝对不能自动执行任何控场操作，只能根据用户需求组织播控逻辑** |
| AI 只做功能封装 | AI 读取 UE5 参数 → 组合为 Feature → 参数值用 UE5 默认值 |
| AI 不创造参数值 | 默认值来自 UE5，范围来自 UE5，AI 只负责命名和组织 |
| 单路径数据流 | 前端/AI → Python UDP → UE5，无旁路 |
| 8字节 UDP 协议 | uint32 FNV-1a Hash + float32 Value，大端序 |
| 事件驱动执行 | C++ 不自动执行业务逻辑，蓝图在事件中决定做什么 |

## 核心数据流

```
前端/AI 调参 → Python UDP 发包(8字节) → VPBroadcastSubsystem 反射设置属性 → 触发蓝图事件 → 蓝图执行
```

### 通信协议

| 通道 | 协议 | 格式 | 方向 | 用途 |
|------|------|------|------|------|
| MCP | stdio | JSON-RPC | AI ↔ 中台 | AI 组织播控逻辑 + 功能封装 |
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
│       ├── VPBroadcast.uplugin
│       └── Source/VPBroadcast/
│           ├── Public/
│           │   ├── VPBroadcastTypes.h    # 类型定义 + FNV1a Hash + 常量
│           │   ├── VPBroadcastSubsystem.h # 中枢：UDP接收 + 反射 + Schema上报
│           │   ├── VPStageEffectActor.h  # 舞台效果Actor（示例）
│           │   ├── VPAgentComponent.h    # 通用Agent组件（推荐）
│           │   ├── VPAgentMetadata.h     # Schema扫描器
│           │   └── VPStageUDPReceiver.h  # UDP接收线程
│           └── Private/
│               ├── VPBroadcastSubsystem.cpp
│               ├── VPStageEffectActor.cpp
│               ├── VPAgentComponent.cpp
│               ├── VPAgentMetadata.cpp
│               ├── VPStageUDPReceiver.cpp
│               └── VPBroadcastModule.cpp
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
- **触发蓝图事件**：设值后触发 `On Control Command` / `On Parameter Changed` 等事件，**不自动执行业务逻辑**
- **Schema 上报**：启动时自动扫描所有 `VPAgent|` 类别属性，上报到 Python 中台

### VPAgentComponent（推荐 · 通用轻量）

可挂载到任意 Actor 的通用组件，**推荐作为主要使用方式**：

- 无场景组件，纯逻辑组件，极轻量
- 4 个基础参数（Opacity / Emissive / Scale / Color）+ 1 个事件触发器
- 所有执行逻辑交给蓝图事件
- 蓝图事件：

| 事件 | 类型 | 说明 |
|------|------|------|
| `On Control Command` | 委托 (Bind Event) | 任何蓝图可绑定，收到 `(ParameterName, Value)` |
| `On Event Triggered` | 委托 (Bind Event) | 事件上升沿触发，收到 `(EventName)` |
| `On Parameter Changed` | Override | 蓝图子类覆写，收到 `(ParameterName, NewValue)` |
| `On Any Parameter Changed` | Override | 任何参数变化时触发 |
| `On Event Triggered` | Override | 蓝图子类覆写，收到 `(EventName)` |
| `On Effect Triggered` | Override | 蓝图子类覆写，收到 `(EffectName)` |
| `On Parameters Changed` | Override | 蓝图子类覆写，收到变更参数 TMap |

- 蓝图可调用工具函数：

| 函数 | 说明 |
|------|------|
| `ApplyAllParameters()` | 同步参数到材质/MPC/灯光（默认行为，蓝图手动调用） |
| `SyncToActorComponents()` | 仅同步到 StaticMesh/PointLight |
| `SyncAllToMPC()` | 仅同步到 MaterialParameterCollection |
| `TriggerEffectByName(Name)` | 触发指定特效 |
| `SpawnDefaultEffect()` | 生成默认 Niagara 特效 |

### VPStageEffectActor（示例 · 重量级）

预置舞台效果 Actor，**作为使用示例**，包含 AR 全息、环境光、舞台事件三组参数。自带 HologramMesh / KeySpotLight / AmbientLight 组件。

蓝图事件与 VPAgentComponent 相同，额外提供：

| 函数 | 说明 |
|------|------|
| `ApplyAllParameters()` | 应用全部参数（默认行为） |
| `ApplyARHologramParams()` | 仅应用全息参数 |
| `ApplyAmbientLightParams()` | 仅应用灯光参数 |
| `TriggerFirework()` | 触发烟花特效 |
| `TriggerSceneTransition()` | 触发转场特效 |
| `SpawnNiagaraAtLocation(System, Location, Offset)` | 在指定位置生成 Niagara |

### VPAgentMetadataScanner（Schema 扫描器）

静态工具类，扫描 UE5 反射系统：
- 自动发现所有带 `VPAgent|` Category 标签的 `UPROPERTY`
- 读取 `VPA_Tags` / `VPA_Description` / `ClampMin` / `ClampMax` 等元数据
- 生成 JSON Schema 上报到 Python 中台

## 蓝图使用指南

### 方式 1：VPAgentComponent + Bind Event（最轻量）

1. 创建你的自定义 Actor
2. 添加 `VP Stage Agent` 组件
3. 在 Event Graph 中：

```
Begin Play
  → Bind Event to On Control Command Received
  → Bind Event to On Event Triggered

On Control Command (ParameterName, Value)
  ├── "Agent_Opacity" → 设置你的 PointLight 亮度
  ├── "Agent_Color"   → 设置你的 PointLight 颜色
  └── 其他            → ApplyAllParameters() (用默认行为)
```

### 方式 2：继承 VPStageEffectActor + Override（带预设参数）

1. 创建蓝图子类继承 `VPStageEffectActor`
2. Override `On Parameter Changed` 事件
3. 在事件中写自定义逻辑

### 方式 3：添加自定义可控参数

在蓝图子类中添加新变量，设置 Category 为 `VPAgent|你的分类`：

```
UPROPERTY 格式:
  Category = "VPAgent|分类名"
  meta = (ClampMin, ClampMax, VPA_Tags, VPA_Description)
```

Subsystem 会自动发现并纳入 UDP 控制，无需修改 C++ 代码。

## MCP 工具清单

| 工具名 | 参数 | 用途 |
|--------|------|------|
| `get_capabilities` | 无 | 获取 UE5 全量参数清单 + 已有 Feature |
| `compose_feature` | `name`, `description`, `param_names` | AI 将 UE5 参数封装为功能 Feature |
| `configure_layout` | `feature_order`, `collapsed`, `highlights` | AI 编排前端画布布局 |
| `set_float_parameter` | `param_name`, `value` | 精确参数控制（快捷调试通道） |
| `execute_stage_cue` | `cue_name`, `value`, `target` | 语义解析 → 控前端/控 UE5 |

⚠️ **重要**：AI 使用这些工具必须完全由用户授权，不能自动执行！

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

1. 在蓝图子类中添加变量，设置 Category 为 `VPAgent|新分类`：

```cpp
// C++ 方式
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VPAgent|新分类",
    meta = (ClampMin = "0.0", ClampMax = "1.0",
            VPA_Tags = "标签1,标签2",
            VPA_Description = "参数描述"))
float My_New_Param = 0.5f;
```

2. 在蓝图的 `On Parameter Changed` 事件中添加响应逻辑
3. 重新编译（C++）或直接保存蓝图，Schema 会自动上报到前端

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
