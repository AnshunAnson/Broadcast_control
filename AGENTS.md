# AGENTS.md — VPBroadcast 项目环境与需求边界

## 项目环境

| 项 | 值 |
|----|-----|
| 项目根目录 | `D:\UE\Project\MyProject\Broadcast_control` |
| UE5 引擎路径 | `D:\UE\UE_5.1` |
| UE5 项目文件 | `BrodcastMVP\BrodcastMVP.uproject` |
| Python 中台 | `broadcast_control/` (Python 3.10+, FastAPI + MCP) |
| Web 前端 | `webview_ui/` (React 18 + TypeScript + Vite) |
| UE5 插件 | `BrodcastMVP/plugins/VPBroadcast/` (C++ 5.1) |
| 编译命令 | `& "D:\UE\UE_5.1\Engine\Build\BatchFiles\Build.bat" BrodcastMVPEditor Win64 Development "D:\UE\Project\MyProject\Broadcast_control\BrodcastMVP\BrodcastMVP.uproject" -WaitMutex -FromMsBuild` |
| 启动命令 | `.\start.ps1` |
| Python 启动 | `python -m broadcast_control.main` |
| MCP 启动 | `python -m broadcast_control.mcp_launcher` |
| 前端启动 | `cd webview_ui && npm run dev` |

## 需求边界

### 系统唯一职责

前端/AI 调参 → Python UDP 发包 → UE5 接收 → 设置属性 → 立即应用

### 核心约束

| 约束 | 说明 |
|------|------|
| UE5 是参数唯一源头 | 参数名、类型、默认值、范围、标签全部由 UE5 VPAgent\| 属性定义 |
| AI 只做功能封装 | AI 读取 UE5 参数 → 组合为 Feature → 参数值用 UE5 默认值 |
| AI 不创造参数值 | 默认值来自 UE5，范围来自 UE5，AI 只负责命名和组织 |
| 单路径数据流 | 前端/AI → Python UDP → UE5，无旁路 |
| 8字节 UDP 协议 | uint32 FNV-1a Hash + float32 Value，大端序 |

### 不做什么

- 不在 Python 端定义参数（参数定义只在 UE5）
- 不做双向同步（UE5 → Python 是 Schema 上报，Python → UE5 是单向 UDP 指令）
- 不做参数持久化（所有状态在内存中，重启后从 UE5 重新上报）
- 不做 UE5 编辑器内 UI（控制面板在 Web 前端）
- 不引入消息队列（WebSocket + UDP 已满足实时性需求）

### 数据流边界

```
UE5 ──HTTP POST──→ Python (Schema上报)
Python ──UDP 8B──→ UE5 (参数指令)
AI ──MCP stdio──→ Python (工具调用)
Web ──WebSocket──→ Python (双向交互)
```

## AI 操作约束

### MCP 工具使用规则

1. **先调 get_capabilities** 再做任何操作，确认 UE5 已上报 Schema
2. **compose_feature 必须校验** param_names 中的每个参数是否存在于 Schema
3. **execute_stage_cue 的 target 默认 ue5**，除非用户明确要求推前端
4. **不要连续高频调用 set_float_parameter**，UDP 无拥塞控制，可能丢包

### 代码修改规则

1. **UE5 C++ 修改后必须编译验证**：使用上方编译命令
2. **Python 修改后运行测试**：`python tests/test_core.py`
3. **前端修改后检查 TypeScript**：`cd webview_ui && npx tsc --noEmit`
4. **架构变动必须更新 README.md**
5. **不引入新依赖** 除非用户明确要求

### 文件修改边界

| 目录 | 允许修改 | 说明 |
|------|----------|------|
| `BrodcastMVP/plugins/VPBroadcast/Source/` | ✅ | UE5 插件源码 |
| `broadcast_control/` | ✅ | Python 中台 |
| `webview_ui/src/` | ✅ | Web 前端 |
| `tests/` | ✅ | 测试 |
| `BrodcastMVP/Content/` | ❌ | UE5 资产文件，不要修改 |
| `webview_ui/node_modules/` | ❌ | 依赖目录 |
| `.trae/` | ❌ | IDE 配置 |

## 关键文件索引

| 文件 | 职责 |
|------|------|
| `VPBroadcastTypes.h` | FNV-1a Hash + 常量 + 类型定义 |
| `VPBroadcastSubsystem.h/.cpp` | 中枢：UDP接收 + 反射设置 + Schema上报 |
| `VPStageEffectActor.h/.cpp` | 舞台效果Actor：全息+灯光+事件 |
| `VPAgentComponent.h/.cpp` | 通用Agent组件：蓝图可扩展 |
| `VPAgentMetadata.h/.cpp` | Schema扫描器 |
| `VPStageUDPReceiver.h/.cpp` | UDP接收线程 |
| `models.py` | Pydantic模型 + Hash + UDP打包 |
| `schema_registry.py` | Schema缓存 + Feature + 语义检索 |
| `mcp_server.py` | MCP Server 5个工具 |
| `main.py` | FastAPI HTTP + WebSocket |
| `udp_sender.py` | UDP异步发送器 |
| `config.py` | 配置 |
