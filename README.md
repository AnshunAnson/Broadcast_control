# VPBroadcast — AI 驱动虚拟制片播控中台

实时连接 AI / Web 前端与 Unreal Engine 5 舞台参数的端到端播控系统。

## 系统架构

```
┌─────────────┐     ┌──────────────────┐     UDP      ┌──────────────────────┐
│  AI / MCP   │────▶│  Python 中台      │────────────▶│  UE5 VPBroadcast     │
│  自然语言    │     │  FastAPI + MCP    │   8 bytes    │  Subsystem           │
│  控制指令    │     │  WebSocket        │  (Hash,Val)  │  反射设置属性         │
└─────────────┘     │  Schema Registry  │              │  ApplyAllParameters  │
                    └────────┬─────────┘              └──────────────────────┘
                             │                                  │
                    WebSocket│                                 立即生效
                             ▼                                  ▼
                    ┌──────────────────┐              ┌──────────────────────┐
                    │  Web UI          │              │  材质 / 灯光 / 缩放   │
                    │  React + Vite    │              │  Niagara 特效        │
                    │  实时参数控制     │              │  蓝图事件通知         │
                    └──────────────────┘              └──────────────────────┘
```

## 核心数据流

**单路径、零冗余、即时生效：**

```
前端调参 → Python UDP 发包 → VPBroadcastSubsystem 反射设置属性 → 通知 Actor 应用 → 立即生效
```

## 技术栈

| 层 | 技术 | 端口/协议 |
|----|------|-----------|
| UE5 插件 | C++ / UE 5.1 / Reflection / Niagara | UDP 7000 接收 |
| Python 中台 | FastAPI + uvicorn + MCP SDK | HTTP 8000 / WebSocket |
| Web 前端 | React 18 + TypeScript + Vite | HTTP 5173 |
| 通信协议 | FNV-1a Hash + 8字节 UDP 包 | UDP |

## 项目结构

```
Broadcast_control/
├── BrodcastMVP/                          # UE5 项目
│   ├── BrodcastMVP.uproject
│   ├── Content/
│   │   ├── MCP_Var.uasset                # 材质参数集
│   │   └── VprodProject/Maps/Main.umap   # 主关卡
│   └── plugins/VPBroadcast/              # UE5 插件
│       ├── VPBroadcast.uplugin
│       └── Source/VPBroadcast/
│           ├── Public/
│           │   ├── VPBroadcastTypes.h    # 类型定义 + FNV1a Hash
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
│   ├── main.py                           # FastAPI 入口
│   ├── config.py                         # 配置（端口/地址）
│   ├── mcp_server.py                     # MCP 工具服务器
│   ├── mcp_launcher.py                   # MCP 启动器
│   ├── models.py                         # Pydantic 数据模型
│   ├── schema_registry.py                # Schema 注册表
│   ├── udp_sender.py                     # UDP 发包器
│   └── pyproject.toml
├── webview_ui/                           # Web 前端
│   ├── src/
│   │   ├── App.tsx
│   │   ├── components/
│   │   │   ├── FeatureDashboard.tsx      # 功能看板
│   │   │   ├── FeatureControlPanel.tsx   # 参数控制面板
│   │   │   ├── ParameterSlider.tsx       # 滑块控件
│   │   │   ├── ParameterColor.tsx        # 颜色控件
│   │   │   ├── EventButton.tsx           # 事件触发按钮
│   │   │   └── DynamicPanel.tsx          # 动态面板
│   │   ├── hooks/useWebSocket.ts         # WebSocket 连接
│   │   └── types/schema.ts              # 类型定义
│   └── package.json
├── tests/                                # 测试
│   ├── test_core.py
│   ├── test_integration.py
│   └── test_verification.py
├── demo_hash.py                          # Hash 计算工具
└── start.bat / start.ps1                 # 一键启动脚本
```

## UE5 插件模块说明

### VPBroadcastSubsystem（中枢）

`UGameInstanceSubsystem`，系统级单例，负责：

- **UDP 接收**：监听端口 7000，接收 8 字节 `(ParamHash, Value)` 包
- **反射设置属性**：通过 `SetPropertyValue_InContainer` 直接写入 Actor 属性
- **即时应用**：设置后立即调用 `ApplyAllParameters()` + 触发蓝图事件
- **Schema 上报**：启动时自动扫描所有 `VPAgent|` 类别属性，上报到 Python 中台

### VPStageEffectActor（舞台效果）

预置舞台效果 Actor，包含：

| 参数 | 类别 | 范围 | 说明 |
|------|------|------|------|
| `AR_Hologram_Alpha` | AR 全息 | 0~1 | 全息透明度 |
| `AR_EmissivePower` | AR 全息 | 0~5 | 全息发光强度 |
| `AR_Hologram_Color` | AR 全息 | Color | 全息颜色 |
| `AR_ModelScale` | AR 全息 | 0.1~3 | 全息缩放 |
| `Ambient_Intensity` | 环境光 | 0~1 | 环境光强度 |
| `Spotlight_Angle` | 环境光 | 0~1 | 聚光灯角度 |
| `Light_Temperature` | 环境光 | 1000~10000 | 灯光色温(K) |
| `Event_Firework` | 舞台事件 | 0/1 | 烟花特效触发 |
| `Event_Transition` | 舞台事件 | 0/1 | 转场动画触发 |

蓝图事件：
- `On Parameter Changed` — 单个参数变化时触发，传入参数名和新值
- `On Any Agent Parameter Changed` — 任何参数变化时触发

### VPAgentComponent（通用 Agent 组件）

可挂载到任意 Actor 的通用组件，支持蓝图自由扩展参数：

- 自动同步属性到 `MaterialParameterCollection`
- 自动同步到 Actor 的 `StaticMeshComponent` / `PointLightComponent`
- `OnParametersChanged` 蓝图事件
- `OnEffectTriggered` 特效触发事件

### VPStageUDPReceiver（UDP 接收线程）

`FRunnable` 实现，独立线程接收 UDP 数据包：

- 数据格式：8 字节 `(int32 ParamHash, float Value)`
- 线程安全队列 `TQueue<FVPStageCommand, EQueueMode::Mpsc>`
- 支持优雅关闭

### VPAgentMetadataScanner（Schema 扫描器）

静态工具类，扫描 UE5 反射系统：

- 自动发现所有带 `VPAgent|` Category 标签的 `UPROPERTY`
- 读取 `VPA_Tags` / `VPA_Description` / `ClampMin` / `ClampMax` 等元数据
- 生成 JSON Schema 上报到 Python 中台

## 通信协议

### UDP 数据包格式

```
偏移  大小   字段
0     4B    ParamHash (int32, FNV-1a 32-bit)
4     4B    Value (float, IEEE 754)
```

### Hash 计算规则

```python
# demo_hash.py
def fnv1a_32(s: str) -> int:
    hash = 0x811C9DC5
    for byte in s.encode('ascii'):
        hash ^= byte
        hash = (hash * 0x01000193) & 0xFFFFFFFF
    return hash
```

## 快速启动

### 前置条件

- Unreal Engine 5.1 (`D:\UE\UE_5.1`)
- Python 3.12+
- Node.js 18+

### 1. 编译 UE5 插件

```powershell
& "D:\UE\UE_5.1\Engine\Build\BatchFiles\Build.bat" BrodcastMVPEditor Win64 Development "D:\UE\Project\MyProject\Broadcast_control\BrodcastMVP\BrodcastMVP.uproject" -WaitMutex -FromMsBuild
```

### 2. 启动 Python 中台

```powershell
cd D:\UE\Project\MyProject\Broadcast_control
python -m broadcast_control.main
```

### 3. 启动 Web 前端

```powershell
cd D:\UE\Project\MyProject\Broadcast_control\webview_ui
npm install
npm run dev
```

### 4. 启动 UE5 编辑器

双击 `BrodcastMVP.uproject`，打开关卡后点击 Play。

### 一键启动

```powershell
.\start.ps1
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

### 使用 VPAgentComponent

1. 将 `VP Stage Agent` 组件添加到任意 Actor
2. 在蓝图中覆写 `OnParametersChanged` 响应参数变化
3. 在蓝图中覆写 `OnEffectTriggered` 响应特效触发

## 配置

| 配置项 | 默认值 | 环境变量 |
|--------|--------|----------|
| UDP 目标地址 | 127.0.0.1 | `UDP_TARGET_HOST` |
| UDP 目标端口 | 7000 | `UDP_TARGET_PORT` |
| HTTP 监听地址 | 0.0.0.0 | `HTTP_LISTEN_HOST` |
| HTTP 监听端口 | 8000 | `HTTP_LISTEN_PORT` |
| Schema TTL | 300s | `SCHEMA_TTL_SECONDS` |

## License

Private — Internal Use Only
