# VPBroadcast 代码清理设计文档

## 第一性原理：系统的唯一职责

**一句话**：前端/WebUI 调参 → Python UDP 发包 → UE5 接收 → 设置属性 → 立即应用

## 当前问题诊断

### 问题1：两套并行系统做同一件事
- `VPStageVisualProxyActor`：独立UDP接收器 + 手动HashToParamMap + 直接写MPC
- `VPBroadcastSubsystem`：统一UDP接收器 + 自动扫描反射 + 写属性+MPC
- **结论**：VPStageVisualProxyActor 与 Subsystem 功能完全重叠，删除

### 问题2：VPStageEffectActor 的 Tick 轮询是冗余的
- `SetAgentParameterValue` 已实现「设置→事件→应用」的即时闭环
- Tick 中的 Prev* 变量检测变化是多余的二次检测
- 但 Tick 中的 Event 上升沿检测（Firework/Transition）仍需要保留
- **结论**：删除 Tick 中的 Prev* 变量和参数变化检测，仅保留事件上升沿

### 问题3：VPStageEffectActor 与 VPAgentComponent 职责重叠
- `VPStageEffectActor`：硬编码全息+灯光参数，自带组件
- `VPAgentComponent`：通用Agent组件，自动同步MPC和组件，支持蓝图扩展
- 两者都有 `ApplyAllParameters`，都有 Tick 检测变化
- **结论**：VPStageEffectActor 是特定场景的硬编码Actor，VPAgentComponent 是通用方案。保留两者但消除重复逻辑

### 问题4：VPBroadcastSubsystem::HashToOwnerMap 映射错误
- 第188-190行：把所有 PropertyMeta 的 Hash 都映射到每个找到的 Actor
- 这意味着如果有2个Actor，每个Hash都会映射到最后一个Actor
- **结论**：改为按属性名精确匹配Actor的属性，只映射该Actor实际拥有的属性

### 问题5：VPBroadcastSubsystem::ApplyControlCommand 三层回退过于复杂
- 先尝试 SetAgentParameterValue → 再回退反射设置 → 最后尝试 MPC
- **结论**：统一为一条路径：反射设置属性 → 通知Actor应用 → MPC兜底

## 清理方案

### 删除
1. **删除 VPStageVisualProxyActor** (.h + .cpp) — 与Subsystem完全重叠
2. **删除 VPStageEffectActor::Tick 中的 Prev* 变量和参数变化检测** — SetAgentParameterValue 已覆盖
3. **删除 VPStageEffectActor.h 中7个 Prev* 成员变量** — 不再需要
4. **删除 VPBroadcastSubsystem::ApplyControlCommand 中的 SetAgentParameterValue 分支** — 反射设置+ApplyAllParameters已足够

### 修复
5. **修复 HashToOwnerMap** — 按属性名精确映射到拥有该属性的Actor
6. **简化 ApplyControlCommand** — 反射设置 → ApplyAllParameters → MPC兜底

### 保留（最小闭环）
- VPBroadcastSubsystem：UDP接收 + 反射设置 + Schema上报 + MPC兜底
- VPStageEffectActor：参数定义 + ApplyAllParameters + 事件上升沿 + 蓝图事件
- VPAgentComponent：通用Agent组件 + 自动同步
- VPAgentMetadataScanner：Schema扫描
- VPStageUDPReceiver：UDP接收线程

## 修改文件清单

| 文件 | 操作 |
|------|------|
| VPStageVisualProxyActor.h | 删除 |
| VPStageVisualProxyActor.cpp | 删除 |
| VPStageEffectActor.h | 删除7个Prev*变量，删除bFireworkTriggered/bTransitionTriggered |
| VPStageEffectActor.cpp | 简化Tick仅保留事件上升沿，删除SetAgentParameterValue |
| VPBroadcastSubsystem.h | 删除FMaterialParamMapping（移入cpp），删除ApplyToMaterialParameterCollection声明中的冗余 |
| VPBroadcastSubsystem.cpp | 修复HashToOwnerMap，简化ApplyControlCommand |
