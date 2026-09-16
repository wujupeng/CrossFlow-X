# CrossFlow-X · CF2 macOS 输入捕获编码任务规划

> **阶段标记**：CF2 — macOS Input Capture
> **对应需求规格**：`.codeartsdoer/specs/cf2_mac_capture/spec.md`（v1.4，1144 行，CF2-S01～S06 六个 macOS 输入捕获地基 + 双通道 SPSC + authoritative resynchronization + bitmap ownership 方案 B + releaseAllPressed bounded completion ≤100ms + Callback Boundary 统一，已 FROZEN / AUTHORIZED commit 1ba1454）
> **对应实现方案**：`.codeartsdoer/specs/cf2_mac_capture/design.md`（v1.7，2915 行，方案 A++ Word-Atomic Payload + Seqlock Double-Read Validation + logicalDropBoundary 派生量 + R17 概念分离 + R18 P1-P8 线性化 + Epoch Validity Linearization Theorem + 工程边界量化 + CF0 Safety Invariant 保障 + Verification Matrix 回映，已 PASS / AUTHORIZED commit b93318a）
> **CF0 冻结基线引用**：`.codeartsdoer/specs/cf0_arch_freeze/spec.md`（v2，892 行）+ `.codeartsdoer/specs/cf0_arch_freeze/design.md`（v3，3449 行）+ `.codeartsdoer/specs/cf0_arch_freeze/tasks.md`（69 个任务，CF0-TASK-001~069 已实现）
> **CF1 冻结基线引用**：`.codeartsdoer/specs/cf1_endpoint_disc/spec.md`（v2，1432 行）+ `.codeartsdoer/specs/cf1_endpoint_disc/design.md`（v4，2972 行）+ `.codeartsdoer/specs/cf1_endpoint_disc/tasks.md`（50 个任务，CF1-TASK-001~050 已 FINAL PASS / FROZEN / CLOSED）
> **技术栈**：C++20 + CMake + CTest + ThreadSanitizer（复用 CF0/CF1 基础设施，不引入新线程、不修改 CF0/CF1 Frozen 接口签名）
> **第一原则**：Driverless User-Mode Architecture —— macOS 输入捕获与注入必须完全在用户态完成
> **任务编号格式**：`CF2-TASK-XXX`（大写连字符，延续 CF0/CF1 编号风格）
> **文件命名规范**：snake_case（如 `mac_event_tap.cpp`、`atomic_payload.hpp`、`spsc_ring_buffer.hpp`）
> **任务粒度**：每个任务可在 1-4 小时内完成，包含明确输入/输出/验收标准
> **执行纪律**：严格遵守大G项目经理执行纪律（Design PASS ≠ Coding Authorization；不修改 CF0/CF1 Frozen / 不重新设计 Handoff FSM / 不引入 Coordinator election / Raft / Paxos / 不改变 NodeID/Topology Authority 语义 / Input/Control 双平面隔离 / Evidence-First / Gate Review 后再编码）
> **文档状态**：DRAFT v1 → 待用户审查冻结后进入 Coding Authorization → C++ Implementation
> **状态维度（双维度，禁止单维度 overclaim，延续 CF1 风格）**：
> - **Implementation Status**（编码实现状态）：✅ Implemented / 🟡 Partial / ❌ Not Implemented / N/A
> - **Acceptance / Evidence Status**（验收/证据状态）：🟢 Full / 🟡 Partial / 🔵 Gate Review / ❌ None
> - **Final Status**：CLOSED（Implementation ✅ ∧ Acceptance 🟢）/ OPEN（任一维度未达 🟢）/ GATE（待 Final Gate Review 裁决）
> **10 项必须原样转化为可执行任务和验收证据的 Contract**（大G项目经理特别要求）：
> 1. **SPSC_DATA=256 / Drop-Oldest**（design §2.4.8 方案 A++ tryPushDropOldest）→ CF2-TASK-005 + CF2-TASK-045
> 2. **logicalDropBoundary**（design §2.4.8 INV6 + Consumer tryPop 对齐）→ CF2-TASK-006 + CF2-TASK-046
> 3. **AtomicPayload word-atomic + lock-free**（design §2.4.8 AtomicPayload\<T\> + 平台契约）→ CF2-TASK-003 + CF2-TASK-008 + CF2-TASK-049
> 4. **Slot lifetime / R15-LIFETIME**（design §2.4.8 seqlock double-read + J1-J8 interleaving）→ CF2-TASK-004 + CF2-TASK-045
> 5. **R16 logical-drop boundary**（design §2.4.8 INV2/INV6/INV7a/INV7b + 区间分解）→ CF2-TASK-006 + CF2-TASK-007 + CF2-TASK-046
> 6. **R18 P1-P8 interleavings**（design §2.4.8 R18 修复子节 + Epoch Validity Linearization Theorem）→ CF2-TASK-009 + CF2-TASK-048
> 7. **SPSC_STATE=64 saturation**（design §2.4.1 + §2.1.3.3 饱和安全降级状态机）→ CF2-TASK-018 + CF2-TASK-051
> 8. **callback → enqueue → Capture thread 严格边界**（design §2.1.3.1 Callback Boundary + Design-R5 严格命名）→ CF2-TASK-012 + CF2-TASK-015 + CF2-TASK-050
> 9. **releaseAllPressed ≤100ms**（design §2.1.3.4 + spec §5.3.1 规则 6 + §9.3 CF2-S03-REQ-002）→ CF2-TASK-024 + CF2-TASK-052
> 10. **P3 只证明 bounded safety degradation，不证明 deterministic RECOVERY→NORMAL**（design §2.4.6 + §2.5.3 P3 表述严格限定）→ CF2-TASK-054 + CF2-TASK-055

---

## 任务依赖总览

```
[Group 1 基础设施扩展(错误码+领域模型+AtomicPayload+SPSC A++)] ──┬──> [Group 2 CF2-S01 CGEventTap 捕获]
                                                                ├──> [Group 3 CF2-S02 CGEvent 规范化 + 双通道 SPSC]
                                                                ├──> [Group 4 CF2-S03 CGEventPost 注入 + releaseAllPressed]
                                                                ├──> [Group 5 CF2-S04 屏幕边界 + 边缘越界检测]
                                                                ├──> [Group 6 CF2-S05 修饰键状态追踪]
                                                                └──> [Group 7 CF2-S06 线程生命周期 + PressedStateSnapshot bitmap]
                                                                         │
                                                                         v
[Group 8 单元测试] <─────────── 各实现组并行触发 ──────────────────────────
                                                                         │
                                                                         v
[Group 9 集成测试 + 10 项 Contract 验证] ──> [Group 10 CF0 Safety Invariant + DFX + 架构冻结]
```

**关键依赖链**：
- 基础设施扩展（Group 1）必须最先完成，SPSC RingBuffer 方案 A++ 是双通道 SPSC 的核心算法，所有上层依赖 AtomicPayload + SpscRingBuffer
- AtomicPayload（TASK-003）必须先于 SpscRingBuffer Slot（TASK-004），Slot 含 AtomicPayload 成员
- SpscRingBuffer tryPush/tryPushDropOldest/tryPop（TASK-005）必须先于 logicalDropBoundary 对齐（TASK-006），对齐步骤嵌入 tryPop
- CF2-S01 CGEventTap（Group 2）必须先于 CF2-S02 规范化（Group 3），callback 产出 RawInputEvent 供 Capture thread 消费
- CF2-S02 双通道 SPSC（TASK-017）依赖 SpscRingBuffer（TASK-005），双通道复用 SpscRingBuffer
- CF2-S03 releaseAllPressed（TASK-024）依赖 PressedStateSnapshot bitmap（TASK-036），释放清单经 bitmap 生成
- CF2-S04 EdgeDetector（TASK-028）依赖 Capture thread（TASK-034），越界检测在 Capture thread 内完成
- CF2-S06 PressedStateSnapshot 方案 B（TASK-036）依赖 SpscRingBuffer（TASK-005），snapshot publication 经 SPSC
- 单元测试（Group 8）与实现同步，集成测试（Group 9）在组件完成后
- 10 项 Contract 验证（Group 9）依赖全部实现组完成，是最高优先级
- CF0 Safety Invariant 保持验证（Group 10）为最终 Gate，任一失败 CF2 不得冻结

---

## 任务总览表

> **状态维度说明（双维度，禁止单维度 overclaim）**：
> - **Implementation**：✅ Implemented（编码实现完成）/ 🟡 Partial（部分实现）/ ❌ Not Implemented / N/A
> - **Acceptance/Evidence**：🟢 Full（完整证据，代码+测试+日志三联）/ 🟡 Partial（部分证据，待补齐）/ 🔵 Gate Review（仅 Gate 级证据）/ ❌ None
> - **Final**：CLOSED（Implementation ✅ ∧ Acceptance 🟢）/ OPEN（任一维度未达 🟢）/ GATE（待 Final Gate Review 裁决）

| 任务 ID | 任务标题 | 优先级 | 依赖任务 | Implementation | Acceptance/Evidence | Final |
|---------|---------|--------|---------|----------------|---------------------|-------|
| CF2-TASK-001 | CF2 错误码体系扩展（CFX-E-CAP-* / CFX-E-INJ-* / CFX-W-CAP-*） | P0 | CF0-TASK-003 | ❌ | ❌ | OPEN |
| CF2-TASK-002 | CF2 领域模型扩展（EdgeOverflowEvent + PressedStateSnapshot bitmap + stale flag） | P0 | CF0-TASK-005 | ❌ | ❌ | OPEN |
| CF2-TASK-003 | AtomicPayload\<T\> word-atomic 模板实现（Contract #3） | P0 | CF2-TASK-001 | ❌ | ❌ | OPEN |
| CF2-TASK-004 | SPSC RingBuffer Slot\<T\> 结构 + seqlock double-read validation（Contract #4 R15-LIFETIME） | P0 | CF2-TASK-003 | ❌ | ❌ | OPEN |
| CF2-TASK-005 | SPSC RingBuffer tryPush / tryPushDropOldest / tryPop 实现（Contract #1 SPSC_DATA=256/Drop-Oldest） | P0 | CF2-TASK-004 | ❌ | ❌ | OPEN |
| CF2-TASK-006 | logicalDropBoundary 派生量 + Consumer tryPop 对齐步骤（Contract #2 + Contract #5 R16） | P0 | CF2-TASK-005 | ❌ | ❌ | OPEN |
| CF2-TASK-007 | INV1~INV6 + INV7a/INV7b + 区间分解 invariant + cumulativeDropCount observational metric（Contract #5 R16/R17） | P0 | CF2-TASK-006 | ❌ | ❌ | OPEN |
| CF2-TASK-008 | AtomicPayload 平台契约（x86_64/arm64 support matrix + static_assert + architecture guard）（Contract #3） | P0 | CF2-TASK-003 | ❌ | ❌ | OPEN |
| CF2-TASK-009 | R18 P1-P8 interleavings 线性化实现 + Epoch Validity Linearization Theorem 验证（Contract #6） | P0 | CF2-TASK-006 | ❌ | ❌ | OPEN |
| CF2-TASK-010 | IInputCapture macOS 实现接口骨架（MacEventTap） | P0 | CF0-TASK-021 | ❌ | ❌ | OPEN |
| CF2-TASK-011 | A11yPermissionGuard 辅助功能权限前置检测 | P0 | CF2-TASK-010 | ❌ | ❌ | OPEN |
| CF2-TASK-012 | MacEventFieldExtractor callback 内字段提取（Contract #8 callback 边界） | P0 | CF2-TASK-010, CF2-TASK-002 | ❌ | ❌ | OPEN |
| CF2-TASK-013 | CGEventTap 回调安装 + ≤1ms 轻量化 + 失败降级 | P0 | CF2-TASK-011, CF2-TASK-012 | ❌ | ❌ | OPEN |
| CF2-TASK-014 | CaptureHandle 句柄管理 + RAII | P0 | CF2-TASK-013 | ❌ | ❌ | OPEN |
| CF2-TASK-015 | CGEventNormalizer Capture thread 内规范化（Contract #8 callback 边界） | P0 | CF2-TASK-012 | ❌ | ❌ | OPEN |
| CF2-TASK-016 | CGEvent 类型完整映射 + payload 提取 + platformTime 提取 | P0 | CF2-TASK-015 | ❌ | ❌ | OPEN |
| CF2-TASK-017 | DualChannelSpsc 双通道分发（SPSC_DATA + SPSC_STATE） | P0 | CF2-TASK-005 | ❌ | ❌ | OPEN |
| CF2-TASK-018 | SPSC_STATE=64 reserved capacity + 饱和安全降级状态机（Contract #7） | P0 | CF2-TASK-017 | ❌ | ❌ | OPEN |
| CF2-TASK-019 | AuthoritativeResync 确定性安全降级（修饰键 ground truth + bitmap best-effort + FSM RECOVERY） | P0 | CF2-TASK-018 | ❌ | ❌ | OPEN |
| CF2-TASK-020 | 平台逻辑隔离红线（platform/mac/ 适配层 + Core 层零平台头文件） | P0 | CF2-TASK-015, CF2-TASK-016 | ❌ | ❌ | OPEN |
| CF2-TASK-021 | IInputInjector macOS 实现接口骨架（MacEventInjector） | P0 | CF0-TASK-021 | ❌ | ❌ | OPEN |
| CF2-TASK-022 | CGEventPost 注入 + RelativeDelta/AbsolutePosition 双语义 | P0 | CF2-TASK-021 | ❌ | ❌ | OPEN |
| CF2-TASK-023 | 注入仅接受主控端流 + 参数校验 | P0 | CF2-TASK-021 | ❌ | ❌ | OPEN |
| CF2-TASK-024 | ReleaseAllPressedExecutor total deadline ≤100ms bounded completion（Contract #9） | P0 | CF2-TASK-022, CF2-TASK-023 | ❌ | ❌ | OPEN |
| CF2-TASK-025 | 修饰键显式同步（Handoff 后目标端对齐） | P1 | CF2-TASK-022 | ❌ | ❌ | OPEN |
| CF2-TASK-026 | IScreenQuery macOS 实现（MacScreenQuery）+ NSScreen/CGDisplay 几何查询 | P0 | CF0-TASK-021 | ❌ | ❌ | OPEN |
| CF2-TASK-027 | ScreenBoundary 归一化（native → CF0 左上角 (0,0) 语义） | P0 | CF2-TASK-026 | ❌ | ❌ | OPEN |
| CF2-TASK-028 | EdgeDetector 边缘越界检测（坐标域分层 i64/u32 + ≤500us + Capture thread 内） | P0 | CF2-TASK-027 | ❌ | ❌ | OPEN |
| CF2-TASK-029 | 分辨率变更适应（≤1s + 通知 CF0 Coordinate Engine） | P1 | CF2-TASK-026 | ❌ | ❌ | OPEN |
| CF2-TASK-030 | 多显示器合并声明 | P1 | CF2-TASK-026 | ❌ | ❌ | OPEN |
| CF2-TASK-031 | ModifierTracker std::atomic\<ModifierState\> 无锁存储 + static_assert(is_lock_free()) | P0 | CF0-TASK-005 | ❌ | ❌ | OPEN |
| CF2-TASK-032 | CGEventFlags 解析 + Modifier atomic update（callback 内 ≤200ns） | P0 | CF2-TASK-031 | ❌ | ❌ | OPEN |
| CF2-TASK-033 | 修饰键显式同步注入（Handoff 后构造修饰键按下/释放事件） | P1 | CF2-TASK-031, CF2-TASK-022 | ❌ | ❌ | OPEN |
| CF2-TASK-034 | CaptureLifecycleManager 线程复用（CF0 #2 Capture + #3 Injection，不新增线程） | P0 | CF0-TASK-021 | ❌ | ❌ | OPEN |
| CF2-TASK-035 | KeyCodeBitmap (256-bit) + MouseButtonBitmap (8-bit) fixed-size 位图 | P0 | CF2-TASK-002 | ❌ | ❌ | OPEN |
| CF2-TASK-036 | PressedStateSnapshot 方案 B SPSC snapshot publication（Capture thread owns mutable bitmap → SPSC → FSM/injection thread owns immutable snapshot） | P0 | CF2-TASK-005, CF2-TASK-035 | ❌ | ❌ | OPEN |
| CF2-TASK-037 | SnapshotRequest SPSC ownership 明确（Producer = FSM thread 唯一，Consumer = Capture thread 唯一） | P0 | CF2-TASK-036 | ❌ | ❌ | OPEN |
| CF2-TASK-038 | 捕获与注入路径隔离（独立线程 + 无共享可变状态） | P0 | CF2-TASK-034 | ❌ | ❌ | OPEN |
| CF2-TASK-039 | AtomicPayload + SPSC RingBuffer 单元测试 | P1 | CF2-TASK-005, CF2-TASK-006, CF2-TASK-007 | ❌ | ❌ | OPEN |
| CF2-TASK-040 | MacEventFieldExtractor + CGEventNormalizer 单元测试 | P1 | CF2-TASK-015, CF2-TASK-016 | ❌ | ❌ | OPEN |
| CF2-TASK-041 | DualChannelSpsc + AuthoritativeResync 单元测试 | P1 | CF2-TASK-018, CF2-TASK-019 | ❌ | ❌ | OPEN |
| CF2-TASK-042 | MacEventInjector + ReleaseAllPressedExecutor 单元测试 | P1 | CF2-TASK-024 | ❌ | ❌ | OPEN |
| CF2-TASK-043 | EdgeDetector + MacScreenQuery 单元测试 | P1 | CF2-TASK-028 | ❌ | ❌ | OPEN |
| CF2-TASK-044 | ModifierTracker + PressedStateSnapshot 单元测试 | P1 | CF2-TASK-036 | ❌ | ❌ | OPEN |
| CF2-TASK-045 | SPSC Drop-Oldest 并发压力测试 TEST-R14-LIN + TEST-R15-LIFETIME（Contract #1 + #4） | P0 | CF2-TASK-039 | ❌ | ❌ | OPEN |
| CF2-TASK-046 | R16 logicalDropBoundary 测试 TEST-R16-LOGICAL-DROP-BOUNDARY（Contract #2 + #5） | P0 | CF2-TASK-039 | ❌ | ❌ | OPEN |
| CF2-TASK-047 | R17 概念分离测试 TEST-R17-CONCEPT-SEPARATION + TEST-R17-INTERVAL-DECOMPOSITION（Contract #5） | P0 | CF2-TASK-039 | ❌ | ❌ | OPEN |
| CF2-TASK-048 | R18 线性化测试 TEST-R18-LINEARIZATION + TEST-R18-DROP-DEFINITION（Contract #6） | P0 | CF2-TASK-009, CF2-TASK-039 | ❌ | ❌ | OPEN |
| CF2-TASK-049 | AtomicPayload 平台契约测试 TEST-ATOMIC-PAYLOAD-MATRIX（Contract #3） | P0 | CF2-TASK-008, CF2-TASK-039 | ❌ | ❌ | OPEN |
| CF2-TASK-050 | Callback Boundary 集成测试（Contract #8 callback → enqueue → Capture thread 严格边界） | P0 | CF2-TASK-013, CF2-TASK-015, CF2-TASK-017 | ❌ | ❌ | OPEN |
| CF2-TASK-051 | SPSC_STATE 饱和安全降级集成测试（Contract #7 饱和 → authoritative resynchronization → FSM RECOVERY） | P0 | CF2-TASK-019, CF2-TASK-041 | ❌ | ❌ | OPEN |
| CF2-TASK-052 | releaseAllPressed bounded completion 集成测试（Contract #9 total deadline ≤100ms + 失败路径 local safety degradation） | P0 | CF2-TASK-024, CF2-TASK-042 | ❌ | ❌ | OPEN |
| CF2-TASK-053 | CF2-S01~S06 Design Contract 架构测试 | P1 | CF2-TASK-039~044 | ❌ | ❌ | OPEN |
| CF2-TASK-054 | CF0 Safety Invariant P1/P2/P3 保持验证（Contract #10 P3 只证明 bounded safety degradation） | P0 | CF2-TASK-053 | ❌ | ❌ | OPEN |
| CF2-TASK-055 | P3 表述严格限定验证（不证明 deterministic RECOVERY→NORMAL，Contract #10） | P0 | CF2-TASK-054 | ❌ | ❌ | OPEN |
| CF2-TASK-056 | DFX 红线验证（回调 ≤1ms + 无锁热路径 + 用户态约束 + ≤8 线程） | P1 | CF2-TASK-054 | ❌ | ❌ | OPEN |
| CF2-TASK-057 | CF0 Frozen Boundary Amendment 验证（additive extension + 不修改 CF0/CF1 Frozen） | P0 | CF2-TASK-056 | ❌ | ❌ | OPEN |
| CF2-TASK-058 | CF2 架构冻结审查与交付 | P0 | CF2-TASK-057 | N/A | ❌ | GATE |

**任务统计**：
- 共 10 组 58 个任务（Group 1: 9 + Group 2: 5 + Group 3: 6 + Group 4: 5 + Group 5: 5 + Group 6: 3 + Group 7: 5 + Group 8: 6 + Group 9: 9 + Group 10: 5）
- **10 项 Contract 覆盖**：全部 10 项 Contract 均有对应实现任务 + 验证任务（详见各任务验收标准中的 Contract 引用）
- **CF2-S01~S06 六模块覆盖**：S01（TASK-010~014）+ S02（TASK-015~020）+ S03（TASK-021~025）+ S04（TASK-026~030）+ S05（TASK-031~033）+ S06（TASK-034~038）

---

## 1. 基础设施扩展（错误码 + 领域模型 + AtomicPayload + SPSC RingBuffer 方案 A++）

> **本组任务为 CF2 全部上层模块的地基**，特别是 SPSC RingBuffer 方案 A++ 是双通道 SPSC 的核心算法，承载 10 项 Contract 中最危险的 6 项（#1~#6）。本组任务必须最先完成。

### 1.1 CF2-TASK-001：CF2 错误码体系扩展

- [ ] **描述**：在 CF0 错误码体系基础上扩展 CF2 阶段新增的结构化错误码，覆盖捕获/注入/权限/队列/释放/架构全部分支
- **输入**：spec.md §4.2/§4.3/§5.1.3/§5.2.3/§5.3.3 异常场景 + design.md §2.6 Verification Matrix 告警码
- **输出**：`core/common/error_codes.hpp` 新增 CF2 错误码枚举（CFX-E-CAP-TAP-FAIL / CFX-E-CAP-STATE-CHANNEL-SATURATED / CFX-E-CAP-ARCH-UNSUPPORTED / CFX-E-INJ-UNAUTHORIZED-SOURCE / CFX-E-INJ-RELEASE-FAILED / CFX-W-CAP-A11Y-DENIED / CFX-W-CAP-QUEUE-DROP / CFX-W-CAP-UNKNOWN-EVENT-TYPE / CFX-W-CAP-UNKNOWN-KEYCODE / CFX-W-CAP-CALLBACK-SLOW / CFX-W-INJ-FAIL-STREAK / CFX-W-INJ-INVALID-PARAM / CFX-W-INJ-RELEASE-INCOMPLETE）
- **验收标准**：
  - [ ] 错误码采用结构化前缀 `CFX-E-*`（错误级）/ `CFX-W-*`（警告级），延续 CF0/CF1 命名风格
  - [ ] 每个错误码含错误码值 + 简描述 + 严重级别 + 建议处置
  - [ ] 单元测试覆盖每个错误码的构造/格式化/分类
  - [ ] 不修改 CF0/CF1 既有错误码定义（CF0 Frozen Boundary）

### 1.2 CF2-TASK-002：CF2 领域模型扩展

- [ ] **描述**：扩展 CF2 阶段新增的领域对象，包括 EdgeOverflowEvent（边缘越界事件）+ PressedStateSnapshot bitmap 扩展（vector → fixed-size bitmap + stale flag）
- **输入**：spec.md §6.4 PressedStateSnapshot + §6.6 EdgeOverflowEvent + design.md §2.7.2 PressedStateSnapshot bitmap extension 边界扩展 Contract
- **输出**：`core/common/domain.hpp` 新增 `EdgeOverflowEvent{direction, overflow, cursorY}` + `PressedStateSnapshot` 扩展含 `MouseButtonBitmap(8bit)` + `KeyCodeBitmap(256bit)` + `stale` flag + `modifiers`
- **验收标准**：
  - [ ] EdgeOverflowEvent.direction 取值 {Left, Right}（第一版不含 Up/Down，复用 CF0 §5.4.1.2）
  - [ ] EdgeOverflowEvent.overflow 为无符号整数（像素距离），cursorY 为无符号整数（纵向坐标）
  - [ ] PressedStateSnapshot 含 MouseButtonBitmap(8bit) + KeyCodeBitmap(256bit) + stale flag + modifiers，**无 std::vector**（HARD CONTRACT，源码审查）
  - [ ] `stale` flag 默认 false（向后兼容，未扩展前既有消费方行为不变）
  - [ ] `IInputInjector.releaseAllPressed(pressed)` 接口签名不变（入参仍为 `PressedStateSnapshot&`，HARD CONTRACT，diff 验证）
  - [ ] additive only：不修改 CF0 既有方法签名（CF0 Frozen Boundary Amendment Contract §2.7.2）

### 1.3 CF2-TASK-003：AtomicPayload\<T\> word-atomic 模板实现（Contract #3）

- [ ] **描述**：实现 `AtomicPayload<T>` 模板，内部 `std::atomic<uint64_t> words[N]`（N = ⌈sizeof(T)/8⌉），逐 word 原子 store/load，作为 SPSC RingBuffer slot payload 的存储类型
- **输入**：design.md §2.4.8 AtomicPayload\<T\> 定义（第 1790-1816 行）+ 不可能性证明（第 1552-1558 行）+ INV5 word-atomic invariant
- **输出**：`platform/common/atomic_payload.hpp` 模板类 `AtomicPayload<T>`，含 `store(value, mo)` / `load(mo)` 方法 + `static constexpr size_t N` + 编译期 `static_assert(std::is_trivially_copyable_v<T>)`
- **验收标准**：
  - [ ] **Contract #3 AtomicPayload word-atomic + lock-free**：`words[k]` 类型为 `std::atomic<uint64_t>`，每 word 8 字节，store/load 为原子操作（C++20 [atomics] §32.4）
  - [ ] `N = (sizeof(T) + 7) / 8`（⌈sizeof(T)/8⌉ 个 8 字节 word）
  - [ ] 前置条件 `static_assert(std::is_trivially_copyable_v<T>)`（RawInputEvent 满足：platformTime(u64) + kind(enum) + payload(inline variant of POD)）
  - [ ] `reinterpret_cast` 安全：T 是 trivially-copyable POD
  - [ ] **不使用 `std::atomic<T>` 大 atomic**（与 R11 兼容：不依赖 std::atomic<256-bit> 平台 lock-free 保证）
  - [ ] 单元测试：store/load 往返一致性 + word 数量正确 + trivially-copyable 校验

### 1.4 CF2-TASK-004：SPSC RingBuffer Slot\<T\> 结构 + seqlock double-read validation（Contract #4 R15-LIFETIME）

- [ ] **描述**：实现 SPSC RingBuffer 的 `Slot<T>` 结构（`AtomicPayload<T> payload` + `std::atomic<uint64_t> seq`）+ Consumer seqlock double-read validation 读取协议（s1 → payload.load → s2 → s1==s2 校验）
- **输入**：design.md §2.4.8 方案 A++ Slot\<T\> 结构（第 1820-1829 行）+ Consumer tryPop seqlock double-read（第 1900-1920 行）+ J1-J8 interleaving（第 1970-1981 行）+ slot lifetime proof（第 1995-2005 行）+ R15 修复子节
- **输出**：`platform/common/spsc_ring_buffer.hpp` 内部 `Slot<T>` 结构 + seqlock double-read validation 读取协议实现
- **验收标准**：
  - [ ] **Contract #4 Slot lifetime / R15-LIFETIME**：Consumer 读取为 seqlock double-read（`s1 = seq.load(acquire)` → epoch 校验 `s1 == ct+1` → `item = payload.load(relaxed)` 逐 word atomic → `s2 = seq.load(acquire)` → 覆写校验 `s1 == s2`），两校验都通过才接受 item
  - [ ] `Slot<T>` 含 `AtomicPayload<T> payload` + `std::atomic<uint64_t> seq`，**无非原子跨线程访问**（源码审查）
  - [ ] **C++20 data-race free**：所有 word + seq 均为 `std::atomic`，无 data race（C++20 strict 合规）
  - [ ] **arbitrary consumer pause safety**：J7 interleaving（s1 读后暂停、Producer 覆写、Consumer 恢复读 payload + s2）下 `s1 != s2` 丢弃，无 data race（word-atomic），无错误内容返回
  - [ ] J8 interleaving（payload 读中途暂停、Producer 覆写 word 1、Consumer 恢复读 word 1）下撕裂 item 被 `s1 != s2` 校验丢弃
  - [ ] linearization point = Consumer 执行第二次 `s2 = seq.load(acquire)` 的瞬间（L2）
  - [ ] slot lifetime proof：slot i 第 k 轮 lifetime 区间 + seqlock double-read 在 lifetime 内安全
  - [ ] 单元测试：seqlock double-read 正常路径 + 覆写检测 + 撕裂 item 丢弃

### 1.5 CF2-TASK-005：SPSC RingBuffer tryPush / tryPushDropOldest / tryPop 实现（Contract #1 SPSC_DATA=256/Drop-Oldest）

- [ ] **描述**：实现 SPSC RingBuffer 的三个核心操作：tryPush（非 Drop Oldest 路径，用于 SPSC_STATE reserved capacity）+ tryPushDropOldest（Drop Oldest 路径，用于 SPSC_DATA）+ tryPop（消费路径，含 seqlock double-read validation）
- **输入**：design.md §2.4.8 方案 A++ tryPush（第 1858-1869 行）+ tryPushDropOldest（第 1872-1883 行）+ Consumer tryPop（第 1886-1920 行）+ SPSC ownership（head=Producer, tail=Consumer）+ 满判定（`h - tail == Capacity`）
- **输出**：`platform/common/spsc_ring_buffer.hpp` `SpscRingBuffer<T, Capacity>` 模板类，含 `tryPush(item)` / `tryPushDropOldest(item)` / `tryPop(item&)` 方法 + `head`/`tail` 原子变量 + `buffer[Capacity]` 槽位数组
- **验收标准**：
  - [ ] **Contract #1 SPSC_DATA=256 / Drop-Oldest**：队列容量 256（`SPSC_DATA_CAPACITY = 256`，编译期 `static_assert`，2 的幂次），Drop-Oldest 语义，Producer 永不阻塞
  - [ ] **tryPush**：满判定 `h - tail == Capacity` 返回 false（不丢，调用方走安全降级）；未满写入 slot + seq + head++
  - [ ] **tryPushDropOldest**：**不判满、不推进任何 tail、不检查 Consumer 状态**（Producer 非阻塞）；覆写 slot payload + 写新 seq + 推进 head；`if (h - tail.load(acquire) >= Capacity) droppedOldestCount.fetch_add(1, relaxed)`
  - [ ] **tryPop**：含 seqlock double-read validation（TASK-004）+ logicalDropBoundary 对齐（TASK-006）
  - [ ] **SPSC ownership 保持**：Producer 仅写 `head` + `buffer[i].payload`（逐 word atomic）+ `buffer[i].seq`，不写 `tail`；Consumer 仅写 `tail`，不写 `head`/`buffer[i].payload`/`buffer[i].seq`（源码审查）
  - [ ] **memory-order**：`head`/`buffer[i].seq`/`tail` 的 store release / load acquire，跨线程可见性保证
  - [ ] **ABA 安全**：uint64 单调递增计数器，实际系统生命周期内不回绕（2^64 ≈ 1.8×10^19 次操作）
  - [ ] **Drop Oldest 语义**：被覆写 slot 的旧 epoch 内容对 Consumer 不可读（seqlock double-read 校验失败跳过），等效丢弃最旧、保留最新
  - [ ] 单元测试：tryPush 正常/满 + tryPushDropOldest 覆写 + tryPop 正常/空/覆写检测

### 1.6 CF2-TASK-006：logicalDropBoundary 派生量 + Consumer tryPop 对齐步骤（Contract #2 + Contract #5 R16）

- [ ] **描述**：在 Consumer tryPop 中实现 logicalDropBoundary 派生量计算 + 对齐步骤：`logicalDropBoundary = max(tail, head - Capacity)`，Consumer 落后于 logical oldest boundary 时快速推进 tail 跳过已被 drop 的 slot
- **输入**：design.md §2.4.8 INV6 logicalDropBoundary 定义（第 1841 行）+ Consumer tryPop 对齐步骤（第 1891-1898 行）+ R16 修复子节（第 2110-2148 行）+ logicalDropBoundary 无竞态证明（第 2125-2131 行）
- **输出**：`platform/common/spsc_ring_buffer.hpp` Consumer tryPop 中 logicalDropBoundary 对齐步骤实现
- **验收标准**：
  - [ ] **Contract #2 logicalDropBoundary**：`logicalDropBoundary = max(tail, head - Capacity)` 派生量（非存储的原子变量，由 Producer-owned `head` + Consumer-owned `tail` 数学关系定义）
  - [ ] **Contract #5 R16 logical-drop boundary**：Consumer tryPop 对齐步骤——`ct = tail.load(relaxed)` + `h = head.load(acquire)` + `logicalDropBoundary = max(ct, h - Capacity)` + 若 `ct < logicalDropBoundary` 则 `tail.store(logicalDropBoundary, release)` 一次性跳过已被 Drop-Oldest 丢弃的 slot
  - [ ] **INV2 `head - logicalDropBoundary ≤ Capacity` 恒成立**（数学恒等式：`logicalDropBoundary ≥ head - Capacity`）
  - [ ] **logicalDropBoundary 无竞态**：派生量非存储变量，每次 tryPop 重新计算，不存在"Consumer 持有 stale logicalDropBoundary"的竞态窗口
  - [ ] **与 v1.2 publishedTail 的关键区别**：publishedTail 是存储的共享原子变量存在 stale 副本问题；logicalDropBoundary 是派生的局部变量无 stale 副本问题
  - [ ] **与 seqlock double-read 正交**：logicalDropBoundary 告诉 Consumer "应该从哪个 ct 开始读"，seqlock double-read 告诉 Consumer "这个 slot 是否仍然有效"，职责分离
  - [ ] 单元测试：Consumer 落后于 logicalDropBoundary 时对齐 + 对齐后 INV2 成立 + 无逐个 seq 校验失败循环

### 1.7 CF2-TASK-007：INV1~INV6 + INV7a/INV7b + 区间分解 invariant + cumulativeDropCount observational metric（Contract #5 R16/R17）

- [ ] **描述**：实现并验证 SPSC RingBuffer 方案 A++ 的完整不变量集合：INV1~INV6 + INV7a（pendingDropped 瞬时派生量）+ INV7b（cumulativeDropCount observational metric）+ 区间分解，明确 cumulativeDropCount 降级为 observational metric 不参与 correctness 证明
- **输入**：design.md §2.4.8 方案 A++ 不变量（第 1835-1846 行）+ R17 修复子节（第 2149-2172 行）+ 区间分解证明（第 2153-2164 行）+ cumulativeDropCount observational metric 声明（第 2166-2172 行）
- **输出**：`platform/common/spsc_ring_buffer.hpp` 不变量运行时断言（DEBUG build）+ `cumulativeDropCount` 作为 `std::atomic<uint64_t>` 可观测指标 + 区间分解验证方法
- **验收标准**：
  - [ ] **Contract #5 R16/R17**：INV1 `tail ≤ head` + INV2 `head - logicalDropBoundary ≤ Capacity`（v1.5 重新定义，v1.7 命名统一）+ INV3 head/tail 单调递增 + INV4 slot epoch invariant + INV5 word-atomic invariant + INV6 logicalDropBoundary 定义
  - [ ] **INV7a pendingDropped 瞬时派生量**：`pendingDropped = logicalDropBoundary - tail = max(0, head - tail - Capacity)`，随 Consumer 推进 tail 而归零，**correctness invariant**
  - [ ] **INV7b cumulativeDropCount 独立统计量**：`std::atomic<uint64_t>`，单调递增不回退，**降级为 observational metric，非 correctness invariant**，用途为监控告警/容量调优/DFX 指标上报
  - [ ] **区间分解**：`[0, head) = [0, logicalDropBoundary) ∪ [logicalDropBoundary, tail) ∪ [tail, head)`，其中 `[0, logicalDropBoundary)` = logically expired、`[logicalDropBoundary, tail)` = impossible/empty、`[tail, head)` = current logical window
  - [ ] **旧 INV7/INV8 已彻底删除**：不出现 `droppedCount = effectiveTail - tail` 和 `head = droppedCount + tail + (head - effectiveTail)`（v1.7 Design-R17-CONSISTENCY）
  - [ ] **cumulativeDropCount 不参与 correctness 证明**：correctness 由 seqlock double-read validation（R15）+ logicalDropBoundary 对齐（R16）+ 区间分解（R17-B）保证
  - [ ] DEBUG build 运行时断言：INV1/INV2/INV3/INV4/INV5/INV6 + 区间分解性质（tail ≤ logicalDropBoundary ≤ head + head - logicalDropBoundary ≤ Capacity + [logicalDropBoundary, tail) 为空）
  - [ ] 单元测试：不变量在正常/Drop-Oldest/Consumer-pause 各种状态下成立 + cumulativeDropCount 可观测 + pendingDropped 随 Consumer 对齐归零

### 1.8 CF2-TASK-008：AtomicPayload 平台契约（x86_64/arm64 support matrix + static_assert + architecture guard）（Contract #3）

- [ ] **描述**：实现 AtomicPayload 平台契约：明确 CF2 supported architecture = {x86_64, arm64} + 编译期 `static_assert(is_lock_free())` + CMake architecture guard + 运行时 check + unsupported → build error 不 fallback 到 mutex
- **输入**：design.md §2.4.8 AtomicPayload 平台契约子节（第 2791 行）+ AtomicPayload 平台契约强化子节（第 2795 行）+ Verification Matrix AtomicPayload 行
- **输出**：`platform/common/atomic_payload.hpp` 编译期 `static_assert` + `CMakeLists.txt` architecture guard `#if !defined(__x86_64__) && !defined(__aarch64__) #error` + 错误码 `CFX-E-CAP-ARCH-UNSUPPORTED`
- **验收标准**：
  - [ ] **Contract #3 AtomicPayload 平台契约**：CF2 platform support matrix 明确——Supported: x86_64, arm64 / Required: `std::atomic<uint64_t>::is_lock_free() == true` / Unsupported: any target where required atomic word is not lock-free / Failure: build/configuration failure before CF2 runtime activation
  - [ ] 编译期 `static_assert(std::atomic<uint64_t>{}.is_lock_free())`（使用 `is_lock_free()` 非 `is_always_lock_free`，避免 32-bit ARM 等平台误判）
  - [ ] CMake architecture guard：`#if !defined(__x86_64__) && !defined(__aarch64__) #error "CFX-E-CAP-ARCH-UNSUPPORTED: CF2 requires x86_64 or arm64 for word-atomic payload"` 
  - [ ] **不 fallback 到 mutex-based atomic**（违反 Input Plane 非阻塞设计原则）
  - [ ] **不声称"所有 C++20 平台支持"**（`is_always_lock_free` 在 32-bit ARM 等平台可能为 false，CF2 明确限定 supported architecture）
  - [ ] 与 R11 兼容：8 字节 atomic 非大 atomic，不依赖 std::atomic<256-bit> 平台 lock-free 保证
  - [ ] CI x86_64/arm64 双架构 build 通过；非 x86_64/arm64 build 失败 + 错误信息含 `CFX-E-CAP-ARCH-UNSUPPORTED`

### 1.9 CF2-TASK-009：R18 P1-P8 interleavings 线性化实现 + Epoch Validity Linearization Theorem 验证（Contract #6）

- [ ] **描述**：实现 R18 完整 Push/Pop 线性化证明 P1-P8 的可验证结构 + Epoch Validity Linearization Theorem 的运行时验证，明确每种 interleaving 的四要素（Producer LP / Consumer LP / Drop LP / Consumer 最终接受/拒绝原因）
- **输入**：design.md §2.4.8 R18 修复子节（第 2794 行）+ R18-1 Drop 发生时间统一定义（定义 A Producer-side logical drop）+ R18-2 P1-P8 线性化证明 + R18-3 核心结论 + Theorem (Epoch Validity Linearization)
- **输出**：`platform/common/spsc_ring_buffer.hpp` 线性化点标注 + DEBUG build 线性化验证 + `EpochValidityLinearization` 验证器
- **验收标准**：
  - [ ] **Contract #6 R18 P1-P8 interleavings**：Producer LP = `seq.store(h+1, release)` 瞬间（Lp）/ Consumer LP = `s2 = seq.load(acquire)` 瞬间（Lc，第二次 seq 读）/ Drop LP = 语义发生点（定义 A：Producer 推进 head 到 position + Capacity 之后该 position 立即逻辑失效）/ Consumer 最终接受/拒绝原因（epoch 校验 + 覆写校验结果）
  - [ ] **P1-P8 八种 interleaving 穷举完备**：P1 Consumer Pop before Producer Push / P2 Producer Push before Consumer Pop / P3 Consumer reads head, Producer pushes / P4 Consumer computes logicalDropBoundary, Producer pushes / P5 Consumer s1, Producer overwrite / P6 Consumer payload read, Producer overwrite / P7 Consumer s2, Producer overwrite / P8 Consumer arbitrary pause
  - [ ] **核心结论**：`Lp < Lc` ⇒ Producer 在 Consumer s2 读之前已 seq.store(new_epoch)，s2 = new_epoch > s1 → `s1 != s2` → 丢弃（old item cannot be returned）；`Lc < Lp` ⇒ Consumer 在 Producer 覆写前完成读取，读到旧 item（old item may be returned legitimately，线性化为 Consumer Pop < Producer Push）
  - [ ] **Epoch Validity Linearization Theorem**：(1) 若 epoch(i) 在 Lp(j) 时已失效（j > i 且 j - i ≥ Capacity），则不存在合法线性化使 Consumer 在 Lc(i) > Lp(j) 后返回 item(i)；(2) 若 epoch(i) 在 Lc(i) 时仍有效（∀j: Lp(j) < Lc(i) ⇒ j < i + Capacity），则 Consumer 可合法返回 item(i)
  - [ ] **Drop 发生时间统一定义**：定义 A（Producer-side logical drop）为规范定义，定义 B/C（Consumer 发现 drop 的机制）非 drop 发生定义
  - [ ] DEBUG build 线性化验证：记录每次 tryPop 的 Lc + 对应 Producer 的 Lp + 验证核心结论
  - [ ] 单元测试：P1-P8 各种 interleaving 模拟 + 核心结论验证 + Theorem 验证

---

## 2. CF2-S01 macOS 用户态输入捕获（CGEventTap）

> **本组任务实现 macOS 用户态输入捕获地基**，核心是 CGEventTap 回调边界（Contract #8）：callback 仅做 minimal field extraction + Modifier atomic update + RawInputEvent enqueue，不调用 onEvent/FSM/Edge Detection/downstream dispatch。

### 2.1 CF2-TASK-010：IInputCapture macOS 实现接口骨架（MacEventTap）

- [ ] **描述**：实现 CF0 `IInputCapture` 接口的 macOS 适配层骨架，含 `start(onEvent)` / `stop(handle)` / `queryScreenBoundary()` 方法声明 + MacEventTap 类骨架
- **输入**：spec.md §7.1 IInputCapture 接口表 + design.md §2.2.2 接口清单 + CF0 `platform/common/platform_ports.hpp` IInputCapture 定义
- **输出**：`platform/mac/mac_event_tap.hpp` + `platform/mac/mac_event_tap.cpp` MacEventTap 类骨架，实现 IInputCapture 接口
- **验收标准**：
  - [ ] MacEventTap 实现 IInputCapture 接口（start/stop/queryScreenBoundary），不修改接口签名（CF0 Frozen）
  - [ ] macOS 平台头文件（CGEvent.h、CGEventTap.h）仅出现在 `platform/mac/`，Core 层零平台头文件（契约①）
  - [ ] 类骨架含回调函数指针 + CaptureHandle 管理 + 状态字段（active/inactive/degraded）
  - [ ] 不引入 kext/IOKit 驱动（Driverless User-Mode Architecture）

### 2.2 CF2-TASK-011：A11yPermissionGuard 辅助功能权限前置检测

- [ ] **描述**：实现辅助功能权限前置检测，CGEventTap 安装前检测 Accessibility / Input Monitoring 权限，缺失时拒绝启动 + 告警 + 引导授权，权限恢复后自动重试
- **输入**：spec.md §5.1.1 规则 6 + §5.1.3 异常 1 + design.md §2.6.1 DC-S01-003
- **输出**：`platform/mac/a11y_permission_guard.hpp` + `platform/mac/a11y_permission_guard.cpp` A11yPermissionGuard 类
- **验收标准**：
  - [ ] 使用 `AXIsProcessTrustedWithOptions` 检测 Accessibility 权限 + `CGPreflightSessionEventAccess` 检测 Input Monitoring 权限
  - [ ] 权限缺失时拒绝启动 CGEventTap + 记录告警 `CFX-W-CAP-A11Y-DENIED` + 引导用户在系统偏好设置授权
  - [ ] 权限恢复后自动重试启动捕获（≤1s 内回到正常态）
  - [ ] 权限缺失时本端物理键鼠仍作用于本机（P2 No Void-Owner 保持）
  - [ ] 单元测试：权限缺失/授权/自动重试路径

### 2.3 CF2-TASK-012：MacEventFieldExtractor callback 内字段提取（Contract #8 callback 边界）

- [ ] **描述**：实现 MacEventFieldExtractor（callback 内组件），仅做 minimal field extraction（类型映射 + payload 提取 + platformTime 提取，≤100us，无堆分配），不调用 onEvent/FSM/Edge Detection/downstream dispatch
- **输入**：spec.md §5.1.1 规则 2/9（Callback Boundary）+ design.md §2.1.3.1 Callback Boundary 时序（第 533-585 行）+ Design-R5 严格命名（MacEventFieldExtractor callback 内 / CGEventNormalizer Capture thread 内）
- **输出**：`platform/mac/mac_event_field_extractor.hpp` + `platform/mac/mac_event_field_extractor.cpp` MacEventFieldExtractor 类
- **验收标准**：
  - [ ] **Contract #8 callback → enqueue → Capture thread 严格边界**：MacEventFieldExtractor 仅在 callback 内执行，**CGEventNormalizer 不出现在 callback 边界内**（Design-R5 严格命名）
  - [ ] callback 内仅做三件事：(1) MacEventFieldExtractor 字段提取（≤100us）+ (2) Modifier atomic update（std::atomic 写入，≤200ns）+ (3) RawInputEvent enqueue（双通道 SPSC 分发，无锁原子写）
  - [ ] **callback MUST NOT**：call onEvent / call FSM / perform edge detection / perform handoff decision / perform downstream dispatch / perform blocking operation
  - [ ] **无动态堆分配**：RawInputEvent 及其 RawPayload variant 为 inline variant（无 heap 分配，所有 payload 固定大小栈上构造）
  - [ ] **转换延迟 ≤100us**（仅做类型映射与字段提取，禁止锁等待/IO/重处理）
  - [ ] 单元测试：字段提取正确性 + 无堆分配验证（malloc hook）+ ≤100us 性能基准

### 2.4 CF2-TASK-013：CGEventTap 回调安装 + ≤1ms 轻量化 + 失败降级

- [ ] **描述**：实现 CGEventTap 回调安装（经 CGEventTapCreate）+ 回调 ≤1ms 轻量化保证 + 创建失败/回调异常/系统禁用 tap 时 ≤200ms 内检测并进入降级态
- **输入**：spec.md §5.1.1 规则 1/2/4/7 + §5.1.3 异常 2/3/4 + design.md §2.6.1 DC-S01-001/DC-S01-002
- **输出**：`platform/mac/mac_event_tap.cpp` CGEventTapCreate 回调安装 + 回调函数实现 + 失败降级逻辑
- **验收标准**：
  - [ ] 经 `CGEventTapCreate` 安装用户态事件 tap，监听完整事件类型（kCGEventMouseMoved / *MouseDown / *MouseUp / kCGEventScrollWheel / kCGEventKeyDown / kCGEventKeyUp）
  - [ ] **回调 ≤1ms 返回**（复用 CF0 §4.6.4），回调内禁止锁等待/IO/内存分配/跨平面调用/同步日志写入
  - [ ] CGEventTap 创建失败/回调异常/系统禁用 tap 时 ≤200ms 内检测并进入降级态，不崩溃，记录告警 `CFX-E-CAP-TAP-FAIL`
  - [ ] 降级态下本端物理键鼠仍作用于本机（P2 No Void-Owner 保持）
  - [ ] 捕获启停延迟 ≤50ms（从收到 FSM 启停指令到 CGEventTap 实际启用/禁用）
  - [ ] 单元测试：回调安装/启停/失败降级/≤1ms 性能基准

### 2.5 CF2-TASK-014：CaptureHandle 句柄管理 + RAII

- [ ] **描述**：实现 CaptureHandle 句柄管理 + RAII 生命周期，start() 返回有效句柄，stop(handle) 释放 tap 资源，句柄 active 标志准确反映 tap 状态
- **输入**：spec.md §5.1.1 规则 5 + §6.2 CaptureHandle + design.md §2.6.1
- **输出**：`platform/mac/capture_handle.hpp` CaptureHandle RAII 管理类
- **验收标准**：
  - [ ] start() 成功返回 CaptureHandle{id>0, active=true}
  - [ ] stop(handle) 释放 tap 资源，handle.active=false，无资源泄漏（RAII 析构）
  - [ ] 句柄 active 标志准确反映 tap 状态
  - [ ] ≤200ms 释放（复用 CF0 RAII）
  - [ ] 单元测试：句柄生命周期/资源泄漏检测（valgrind/ASan）

---

## 3. CF2-S02 CGEvent 规范化适配 + 双通道 SPSC 分发

> **本组任务实现 CGEvent 规范化适配 + 双通道 SPSC 分发**，核心是 SPSC_STATE=64 saturation 安全降级（Contract #7）+ callback 边界（Contract #8）。

### 3.1 CF2-TASK-015：CGEventNormalizer Capture thread 内规范化（Contract #8 callback 边界）

- [ ] **描述**：实现 CGEventNormalizer（Capture thread 内组件），在 Capture/Input thread 消费 SPSC 队列后执行 CGEvent→RawInputEvent 完整规范化 + edge detection + downstream dispatch（调用 onEvent）
- **输入**：spec.md §5.2.1 规则 1-10 + design.md §2.1.3.1 Callback Boundary 时序（第 573-584 行）+ Design-R5 严格命名
- **输出**：`platform/mac/cg_event_normalizer.hpp` + `platform/mac/cg_event_normalizer.cpp` CGEventNormalizer 类
- **验收标准**：
  - [ ] **Contract #8 callback 边界**：CGEventNormalizer 仅在 Capture thread 内运行，**不跨越 callback 边界**（Design-R5 严格命名）
  - [ ] CGEventNormalizer 在 Capture/Input thread 消费 SPSC 队列后执行：CGEvent→RawInputEvent 规范化 → edge detection → downstream dispatch（调用 onEvent）
  - [ ] **onEvent 由 Capture/Input thread 触发，不由 callback 触发**（CF2-REQ-R8 冻结）
  - [ ] 平台逻辑隔离：CGEventNormalizer 在 `platform/mac/`，Core 层不含 CGEvent.h/#ifdef __APPLE__
  - [ ] 单元测试：规范化正确性 + onEvent 由 Capture thread 触发验证

### 3.2 CF2-TASK-016：CGEvent 类型完整映射 + payload 提取 + platformTime 提取

- [ ] **描述**：实现 CGEvent 类型完整映射到 RawEventKind + payload 提取（deltaX/deltaY/scrollDelta/keyCode/button）+ platformTime 提取（CGEventTimestamp）
- **输入**：spec.md §5.2.1 规则 2-7 + design.md §2.6.2 DC-S02-002
- **输出**：`platform/mac/cg_event_normalizer.cpp` 类型映射表 + payload 提取方法
- **验收标准**：
  - [ ] CGEvent 类型完整映射：kCGEventMouseMoved → MouseMove / *MouseDown → MouseButtonPress / *MouseUp → MouseButtonRelease / kCGEventScrollWheel → Wheel / kCGEventKeyDown → KeyPress / kCGEventKeyUp → KeyRelease
  - [ ] 鼠标增量提取：deltaX/deltaY 为有符号整数（反映相对上一次位置的位移）
  - [ ] 滚轮增量提取：scrollDelta + axis（垂直/水平）
  - [ ] 键码映射：macOS 虚拟键码（kVK_*）映射到平台无关 KeyCode 枚举，覆盖字母键/数字键/功能键/方向键/修饰键，未映射键码记录告警 `CFX-W-CAP-UNKNOWN-KEYCODE` 并丢弃
  - [ ] 鼠标按钮映射：kCGMouseButtonLeft/Right/Center/Other → MouseButton::Left/Right/Middle
  - [ ] platformTime 提取：从 CGEventTimestamp 提取，作为平台时间戳供 CF0 转换为规范单调时间戳
  - [ ] 未知 CGEvent 类型丢弃 + 告警 `CFX-W-CAP-UNKNOWN-EVENT-TYPE`
  - [ ] 单元测试：全部事件类型映射 + payload 提取正确性 + 未映射键码/未知类型告警

### 3.3 CF2-TASK-017：DualChannelSpsc 双通道分发（SPSC_DATA + SPSC_STATE）

- [ ] **描述**：实现 DualChannelSpsc 双通道分发，CGEventTap callback 按事件类型将 RawInputEvent 分发到两个独立的 SpscRingBuffer——SPSC_DATA（MouseMove/Wheel，Drop Oldest）+ SPSC_STATE（Key/Button Press/Release，reserved capacity）
- **输入**：spec.md §5.2.1 规则 11 双通道模型 + design.md §2.1.3.2 双通道 SPSC 分发流程（第 595-632 行）+ §2.6.2 DC-S02-003
- **输出**：`platform/mac/dual_channel_spsc.hpp` + `platform/mac/dual_channel_spsc.cpp` DualChannelSpsc 类
- **验收标准**：
  - [ ] 双通道模型：SPSC_DATA（容量 256，MouseMove/Wheel，Drop Oldest）+ SPSC_STATE（容量 64，Key/Button Press/Release，reserved capacity）
  - [ ] 两个队列共享同一对生产者/消费者（callback 为唯一生产者，Capture/Input thread 为唯一消费者），不新增线程
  - [ ] callback 按事件类型分发：MouseMove/Wheel → SPSC_DATA.enqueue（tryPushDropOldest）/ Key/Button Press/Release → SPSC_STATE.enqueue（tryPush）
  - [ ] 队列存储预分配固定大小 RawInputEvent（inline variant），enqueue 为无锁原子写，无堆分配
  - [ ] drop policy 可观测：`droppedOldestCount`（SPSC_DATA drop）+ `stateChannelSaturatedCount`（SPSC_STATE 饱和次数）+ `stateChannelSaturated`（当前是否饱和）可经运行时指标暴露
  - [ ] 单元测试：双通道分发正确性 + 容量固定 + 无堆分配 + 指标可观测

### 3.4 CF2-TASK-018：SPSC_STATE=64 reserved capacity + 饱和安全降级状态机（Contract #7）

- [ ] **描述**：实现 SPSC_STATE=64 reserved capacity 语义 + 饱和安全降级状态机：正常设计条件下 callback 无需等待即可提交状态事件；饱和（异常过载）时 callback 设置 stateChannelSaturated + 告警，不阻塞
- **输入**：spec.md §5.2.1 规则 11 SPSC_STATE + design.md §2.1.3.3 SPSC_STATE 饱和安全降级状态机（第 641-690 行）+ §2.4.1 SPSC_STATE=64 capacity 量化论证 + §2.4.0 量化分层（WORKLOAD MODEL）
- **输出**：`platform/mac/dual_channel_spsc.cpp` SPSC_STATE 饱和检测 + 状态机实现
- **验收标准**：
  - [ ] **Contract #7 SPSC_STATE=64 saturation**：SPSC_STATE 容量 64（`SPSC_STATE_CAPACITY = 64`，编译期 `static_assert`，2 的幂次），**在声明 workload envelope 内经过验证的工程容量**
  - [ ] **reserved capacity 语义**：状态通道容量按 "单次用户操作 burst 内状态变更事件数上界 + 余量" 预留（B_burst=6 + B_rate=40 events/s × W_design=1ms = 6.04，10× 上界 ≈ 60.4 → 64），正常设计条件下 callback 无需等待即可提交状态事件
  - [ ] **B_burst=6 / B_rate=40 标注为 WORKLOAD MODEL**（非 deterministic system upper bound，声明值），backlog bound 在声明 workload envelope 内成立
  - [ ] **STATE lane 在正常设计边界内禁止丢弃**（Design-R6 修正：normal-bound reliable, saturation → safety degradation）
  - [ ] **饱和时确定性安全降级**：callback 设置 `stateChannelSaturated = true`（std::atomic<bool>）+ `stateChannelSaturatedCount++`（std::atomic<uint64_t>）+ 异步告警 `CFX-E-CAP-STATE-CHANNEL-SATURATED`（错误级）+ callback 不阻塞返回
  - [ ] **不使用 gap counter 假定 snapshot 可神奇恢复**（v1.2 的 `pressReleaseGapCounter` 模型已废弃）
  - [ ] 单元测试：正常条件下不满 + 饱和检测 + 确定性降级 + 不阻塞

### 3.5 CF2-TASK-019：AuthoritativeResync 确定性安全降级（修饰键 ground truth + bitmap best-effort + FSM RECOVERY）

- [ ] **描述**：实现 AuthoritativeResync 确定性安全降级：Capture thread 检测 stateChannelSaturated → 触发 authoritative resynchronization（修饰键 CGEventSourceFlagsState ground truth + 按键/按钮 bitmap best-effort + FSM 进 RECOVERY）
- **输入**：spec.md §5.2.1 规则 11 (c) authoritative resynchronization + design.md §2.1.3.3 饱和安全降级状态机 + §2.4.6 recovery 时序 + §2.6.2 DC-S02-003
- **输出**：`platform/mac/authoritative_resync.hpp` + `platform/mac/authoritative_resync.cpp` AuthoritativeResync 类
- **验收标准**：
  - [ ] Capture thread 检测 `stateChannelSaturated == true` → 触发 `AuthoritativeResync.resynchronize()`
  - [ ] **修饰键 ground truth**：从 `CGEventSourceFlagsState(kCGEventSourceStateCombinedSessionState)` 查询当前真实修饰键状态（macOS Core Graphics 用户态 API，ground truth，不违反 Driverless User-Mode Architecture），以此重建 ModifierState
  - [ ] **按键/按钮 bitmap best-effort**：Capture thread 当前 KeyCodeBitmap / MouseButtonBitmap 是 "饱和前最后一次成功消费的一致状态"（由 R11 SPSC snapshot publication 保证无 data race），标记 `stale = true`（不可信完整）
  - [ ] **FSM 进 RECOVERY**：通知 CF0 FSM 进入 RECOVERY 态（经 SPSC 队列递交 FSM 专属线程），FSM 在 RECOVERY 态停止捕获
  - [ ] **recovery invariant**：resynchronization 后 ModifierState = macOS HID 层当前真实状态（ground truth）；KeyCodeBitmap / MouseButtonBitmap = best-effort（饱和前一致状态，可能滞后）；FSM 在 RECOVERY 态停止捕获，用户物理松手将自然释放残留按下键
  - [ ] **不依赖 gap counter 神奇恢复**，不依赖被丢弃的单个事件
  - [ ] **降级退出**：`stateChannelSaturated` 清除需 Capture thread 完成 resynchronization + SPSC_STATE 通道排空 + FSM 确认 RECOVERY 完成，避免反复降级
  - [ ] **PressedState authoritative source 链路**：CGEvent callback → SPSC_STATE → Capture thread → mutable bitmap → SPSC snapshot publication → FSM/Injection thread，只有 Capture thread 的 bitmap 是权威状态
  - [ ] 单元测试：resynchronization 触发 + 修饰键 ground truth + bitmap best-effort + FSM RECOVERY 通知 + 降级退出

### 3.6 CF2-TASK-020：平台逻辑隔离红线（platform/mac/ 适配层 + Core 层零平台头文件）

- [ ] **描述**：验证并强制平台逻辑隔离红线：macOS 平台头文件（CGEvent.h、NSScreen.h、CGDisplay.h 等）仅允许出现在 `platform/mac/` 适配层，Core 层（`core/`）禁止包含任何平台头文件或平台条件编译宏
- **输入**：spec.md §5.2.1 规则 1/9 + §4.5.4 平台逻辑隔离红线 + design.md §2.6.2 DC-S02-001
- **输出**：CI 脚本 `scripts/check_platform_isolation.sh` + 架构测试 `tests/architecture/platform_isolation_test.cpp`
- **验收标准**：
  - [ ] grep `core/` 层不含 CGEvent.h、CGEventType、NSScreen.h、CGDisplay.h、#ifdef __APPLE__ 等平台引用
  - [ ] platform/mac/ 层消化平台逻辑，产出平台无关 RawInputEvent
  - [ ] CI 架构测试自动验证平台隔离红线（grep + 编译期检查）
  - [ ] 不修改 CF0 既有平台隔离契约（CF0 Frozen Boundary）

---

## 4. CF2-S03 macOS 用户态输入注入（CGEventPost + releaseAllPressed bounded completion）

> **本组任务实现 macOS 用户态输入注入地基**，核心是 releaseAllPressed ≤100ms bounded completion（Contract #9）。

### 4.1 CF2-TASK-021：IInputInjector macOS 实现接口骨架（MacEventInjector）

- [ ] **描述**：实现 CF0 `IInputInjector` 接口的 macOS 适配层骨架，含 `inject(event)` / `injectBatch(events)` / `releaseAllPressed(pressed)` 方法声明 + MacEventInjector 类骨架
- **输入**：spec.md §7.1 IInputInjector 接口表 + design.md §2.2.2 接口清单 + CF0 `platform/common/platform_ports.hpp` IInputInjector 定义
- **输出**：`platform/mac/mac_event_injector.hpp` + `platform/mac/mac_event_injector.cpp` MacEventInjector 类骨架
- **验收标准**：
  - [ ] MacEventInjector 实现 IInputInjector 接口（inject/injectBatch/releaseAllPressed），不修改接口签名（CF0 Frozen）
  - [ ] macOS 平台头文件仅出现在 `platform/mac/`
  - [ ] 不引入 kext/IOKit 驱动（Driverless User-Mode Architecture）

### 4.2 CF2-TASK-022：CGEventPost 注入 + RelativeDelta/AbsolutePosition 双语义

- [ ] **描述**：实现 CGEventPost 注入 + RelativeDelta/AbsolutePosition 双语义：鼠标移动注入采用方式 A（delta 字段注入）或方式 B（location 换算注入），ACTIVE 进入一次性定位用 AbsolutePosition
- **输入**：spec.md §5.3.1 规则 2（CF2-REQ-R2 修订）+ CF0 design §2.10.0 Coordinate Space 双语义 + design.md §2.6.3
- **输出**：`platform/mac/mac_event_injector.cpp` CGEventPost 注入实现
- **验收标准**：
  - [ ] **鼠标移动注入遵循 CF0 Frozen 的 RelativeDelta 语义**（CF2-REQ-R2 修订，CF0-COORD-001/003/005）
  - [ ] 方式 A（delta 字段注入）：构造 CGEvent 后调用 `CGEventSetDoubleValueField(event, kCGMouseEventDeltaX, deltaX)` / `kCGMouseEventDeltaY` + `CGEventPost(kCGHIDEventTap, event)`
  - [ ] 方式 B（location 换算注入）：查询当前有效 cursor location (curX, curY)，计算目标 location (curX + deltaX, curY + deltaY)，`CGEventCreateMouseEvent(NULL, kCGEventMouseMoved, CGPointMake(targetX, targetY), 0)` + `CGEventPost`
  - [ ] **ACTIVE 进入一次性光标定位**使用 AbsolutePosition（CF0-COORD-002）：`CGEventCreateMouseEvent(NULL, kCGEventMouseMoved, CGPointMake(entry.x, entry.y), 0)` + `CGEventPost`，定位完成后后续鼠标移动切换回 RelativeDelta
  - [ ] **禁止简单写 `CGEventCreateMouseEvent(deltaX, deltaY)`**（CGEvent mouse event creation API 使用的是鼠标位置坐标，不是 RelativeDelta）
  - [ ] 鼠标按下/释放构造对应 button state，滚轮构造 CGEventCreateScrollWheelEvent，按键按下/释放构造 CGEventCreateKeyboardEvent(keyCode, keyDown)
  - [ ] 单次注入延迟 ≤5ms，批量注入（injectBatch）单帧延迟 ≤10ms
  - [ ] 单元测试：方式 A/B 注入正确性 + AbsolutePosition 一次性定位 + ≤5ms/≤10ms 性能基准

### 4.3 CF2-TASK-023：注入仅接受主控端流 + 参数校验

- [ ] **描述**：实现注入仅接受主控端流校验 + CGEvent 构造参数校验（坐标范围/键码范围/按钮范围）
- **输入**：spec.md §5.3.1 规则 3/8/9 + design.md §2.6.3 DC-S03-001
- **输出**：`platform/mac/mac_event_injector.cpp` sourceNodeId 校验 + 参数校验逻辑
- **验收标准**：
  - [ ] 注入前校验 `sourceNodeId == 当前主控端 NodeID`，非主控端拒绝注入 + 告警 `CFX-E-INJ-UNAUTHORIZED-SOURCE`（复用 CF0 §4.3.4 控制权不可窃取）
  - [ ] CGEvent 构造前校验参数合法性：坐标在屏幕有效范围内、键码在合法范围、按钮在合法枚举内
  - [ ] 非法参数丢弃 + 告警 `CFX-W-INJ-INVALID-PARAM`，不注入
  - [ ] 坐标超出屏幕范围：钳制到有效范围后注入，或丢弃并告警
  - [ ] 主控态下拒绝注入（主控态本端物理输入直接作用于本机，无需注入）
  - [ ] 单元测试：非主控端拒绝 + 非法参数丢弃 + 主控态拒绝

### 4.4 CF2-TASK-024：ReleaseAllPressedExecutor total deadline ≤100ms bounded completion（Contract #9）

- [ ] **描述**：实现 ReleaseAllPressedExecutor total deadline ≤100ms bounded completion：Step 1 snapshot → Step 2 release attempt → Step 3 success CLOSED / Step 4 retry within remaining budget / Step 5 local safety degradation，所有步骤计入同一个 ≤100ms total budget
- **输入**：spec.md §5.3.1 规则 6（CF2-REQ-R7/R9 修订）+ §5.3.3 异常 4 + §9.3 CF2-S03-REQ-002 + design.md §2.1.3.4 releaseAllPressed bounded completion 流程（第 691-758 行）+ §2.6.3 DC-S03-002
- **输出**：`platform/mac/release_all_pressed_executor.hpp` + `platform/mac/release_all_pressed_executor.cpp` ReleaseAllPressedExecutor 类
- **验收标准**：
  - [ ] **Contract #9 releaseAllPressed ≤100ms**：total deadline ≤100ms bounded completion，**所有 snapshot / release attempt / retry / result validation / degradation notification 均计入同一个 ≤100ms bounded completion budget**（不采用"次数 × 每次时延"乘积模型，避免最坏 4×30ms=120ms > 100ms）
  - [ ] **Step 1（snapshot，计入预算）**：经 §4.6.4 ownership model 无锁生成 PressedStateSnapshot（≤1ms，计入 total budget）
  - [ ] **Step 2（release attempt，计入预算）**：根据 snapshot 构造合成释放事件，经 CGEventPost 注入；**"调用 releaseAllPressed" ≠ "物理键已经释放"**，必须验证注入结果（result validation 计入 total budget）
  - [ ] **Step 3（success CLOSED）**：若全部合成释放事件注入成功且本端无残留按下状态 → 释放完成，状态 CLOSED（total elapsed ≤ 100ms）
  - [ ] **Step 4（failure retry within remaining budget）**：若 CGEventPost 失败或释放后仍有残留 → 在剩余 total budget 内有界重试（每次 retry 含重新 snapshot + 重新注入 + result validation，均计入同一 total budget；retry 次数与每次时延不固定，由剩余预算决定），**禁止无限重试、禁止超出 100ms total deadline**
  - [ ] **Step 5（deadline reached → local safety degradation）**：total deadline ≤ 100ms reached 仍失败 → 强制清空内部按下状态记录 + 告警 `CFX-E-INJ-RELEASE-FAILED` + 通知 CF0 FSM 进入 RECOVERY 态，保证 P3 Recoverable
  - [ ] **不采用"次数 × 每次时延"乘积模型**（因最坏 4×30ms=120ms > 100ms 会破坏 P3 可证性）
  - [ ] 断线释放日志含释放清单、total elapsed 耗时、最终处置（CLOSED / degraded）；失败路径含剩余预算内重试计数与 `CFX-E-INJ-RELEASE-FAILED` 告警；total elapsed ≤ 100ms 可验证
  - [ ] 单元测试：成功路径 ≤100ms + 失败路径有界重试 + deadline reached local safety degradation + 无无限重试 + 无乘积超界

### 4.5 CF2-TASK-025：修饰键显式同步（Handoff 后目标端对齐）

- [ ] **描述**：实现修饰键显式同步：Handoff 完成后目标端进入被控态时，根据源端修饰键状态快照显式构造并注入修饰键按下/释放事件，对齐本端修饰键状态
- **输入**：spec.md §5.3.1 规则 5 + design.md §2.6.5 DC-S05-002
- **输出**：`platform/mac/mac_event_injector.cpp` 修饰键显式同步方法
- **验收标准**：
  - [ ] Handoff 完成后目标端根据源端修饰键状态快照显式构造并注入修饰键按下/释放事件
  - [ ] 对齐本端 ModifierState = 源端快照
  - [ ] 禁止依赖隐式状态传递修饰键（复用 CF0 §5.1.1.6 修饰键独立追踪）
  - [ ] 对齐日志含源端快照与目标端对齐结果
  - [ ] 单元测试：源端 Shift 按下 + Handoff 完成 → 目标端 ModifierState.Shift=true

---

## 5. CF2-S04 屏幕边界查询与边缘越界检测

> **本组任务实现 macOS 屏幕几何查询与鼠标边缘越界检测机制**，是 Handoff 触发信号与坐标换算的几何地基。

### 5.1 CF2-TASK-026：IScreenQuery macOS 实现（MacScreenQuery）+ NSScreen/CGDisplay 几何查询

- [ ] **描述**：实现 CF0 `IScreenQuery` 接口的 macOS 适配层，经 NSScreen / CGDisplay 用户态 API 查询 macOS 主显示器逻辑几何（原点、宽高）
- **输入**：spec.md §5.4.1 规则 1 + §7.1 IScreenQuery 接口表 + design.md §2.6.4 DC-S04-002
- **输出**：`platform/mac/mac_screen_query.hpp` + `platform/mac/mac_screen_query.cpp` MacScreenQuery 类
- **验收标准**：
  - [ ] MacScreenQuery 实现 IScreenQuery 接口（primaryBoundary），不修改接口签名（CF0 Frozen）
  - [ ] 经 NSScreen / CGDisplay 用户态 API 查询主显示器逻辑几何，禁止依赖内核态或特权操作
  - [ ] 查询延迟 ≤10ms，查询结果可缓存
  - [ ] 单元测试：查询正确性 + ≤10ms 性能基准

### 5.2 CF2-TASK-027：ScreenBoundary 归一化（native → CF0 左上角 (0,0) 语义）

- [ ] **描述**：实现 ScreenBoundary 归一化：macOS native 坐标系差异（NSScreen 左下角原点 / CGDisplay 左上角原点 / 多显示器排列可能负坐标）在 platform/mac/ 适配层消化，归一化为 CF0 Frozen 的左上角 (0,0) 原点语义
- **输入**：spec.md §5.4.1 规则 2（CF2-REQ-R6 修订）+ §6.5 ScreenBoundary + CF0 §5.4.1.4 + 契约⑤ + design.md §2.6.4
- **输出**：`platform/mac/mac_screen_query.cpp` native geometry 归一化逻辑 + `platform/mac/native_coord_normalizer.hpp`
- **验收标准**：
  - [ ] **CF2 是"CF0 Logical Screen Space 的 macOS 实现"**，不是自行发明坐标语义的"主显示器局部坐标适配器"
  - [ ] 查询 macOS native screen geometry 并归一化为 CF0 Frozen 的 ScreenBoundary 语义：原点固定为屏幕左上角 (0,0)，宽高为正整数
  - [ ] **macOS native 坐标系差异在 platform/mac/ 适配层消化**（NSScreen 原点在主显示器左下角、CGDisplay 原点在主显示器左上角、多显示器排列可能产生负坐标）
  - [ ] 适配层负责将 native geometry 转换为 CF0 Frozen 的左上角 (0,0) 原点语义后呈现给 Core 层
  - [ ] **禁止让 CF2 发明坐标语义或假定 native origin 直接等于 (0,0)**
  - [ ] Core 层不出现 NSScreen/CGDisplay native 坐标假设，仅依赖 CF0 ScreenBoundary 语义
  - [ ] 单元测试：NSScreen 左下角原点归一化 + CGDisplay 左上角原点归一化 + 多显示器负坐标归一化

### 5.3 CF2-TASK-028：EdgeDetector 边缘越界检测（坐标域分层 i64/u32 + ≤500us + Capture thread 内）

- [ ] **描述**：实现 EdgeDetector 边缘越界检测：在 Capture/Input thread 内（≤500us）实时比较光标坐标与屏幕逻辑边缘，光标越过左边缘（x < 0）或右边缘（x > width）时产生越界事件
- **输入**：spec.md §5.4.1 规则 6/7/8 + design.md §2.6.4 DC-S04-001 + Design-R1 坐标域分层（cursorX/cursorY 用 i64 native 域，boundary 用 u32 CF0 Logical 域）
- **输出**：`platform/mac/edge_detector.hpp` + `platform/mac/edge_detector.cpp` EdgeDetector 类
- **验收标准**：
  - [ ] **检测在 Capture/Input thread（回调后处理线程）内完成，不在 CGEventTap 回调内完成**（CF2-REQ-R1 修订）
  - [ ] **Design-R1 坐标域分层**：`EdgeDetector.detect(cursorX, cursorY, boundary)` 签名，cursorX/cursorY 为 i64（native 域），boundary 为 u32（CF0 Logical 域），统一 signed domain，消除 u32 永远不 < 0 的逻辑矛盾
  - [ ] 光标 x < 0 → 左边缘越界事件（越界量 = -x）；光标 x > width → 右边缘越界事件（越界量 = x - width）
  - [ ] 上/下边缘不触发（复用 CF0 §5.4.1.2，第一版不含 Up/Down）
  - [ ] 检测延迟 ≤500us
  - [ ] **越界检测不依赖画面**：仅基于光标坐标与声明边界比较，禁止依赖任何屏幕画面、截图、像素数据（复用 CF0 契约⑤）
  - [ ] 越界事件经无锁 SPSC 队列递交 CF0 Handoff FSM 供其消费触发 Handoff（复用 CF0 §4.6.2 FSM 单线程所有权 + 契约⑦）
  - [ ] 越界事件含边缘方向（左/右）+ 越界量（像素）+ 纵向坐标（供 CF0 Coordinate Engine 按比例映射）
  - [ ] **callback 内无 Edge Detection、无 FSM 调用、无越界事件产生**（审查回调代码）
  - [ ] 单元测试：左/右越界检测 + 上/下不触发 + ≤500us 性能基准 + 不依赖画面验证

### 5.4 CF2-TASK-029：分辨率变更适应（≤1s + 通知 CF0 Coordinate Engine）

- [ ] **描述**：实现分辨率变更适应：macOS 分辨率变更、显示器连接/断开、显示器镜像变更时 ≤1s 内重新查询屏幕边界、更新缓存、通知 CF0 Coordinate Engine
- **输入**：spec.md §5.4.1 规则 4 + design.md §2.6.4 DC-S04-002
- **输出**：`platform/mac/mac_screen_query.cpp` 分辨率变更监听 + 缓存失效逻辑
- **验收标准**：
  - [ ] 分辨率变更/显示器连接/断开/镜像变更时 ≤1s 内重新查询屏幕边界、更新缓存、通知 CF0 Coordinate Engine
  - [ ] 进行中的 Handoff 以新边界为准（复用 CF0 §5.4.1.6）
  - [ ] 变更日志含新边界；CF0 收到更新通知
  - [ ] 单元测试：分辨率变更 1080p → 2160p ≤1s 更新 + 通知 CF0

### 5.5 CF2-TASK-030：多显示器合并声明

- [ ] **描述**：实现多显示器合并声明：第一版每个 macOS 端点按单一逻辑屏幕处理，若存在多显示器必须合并声明为单一逻辑边界（取包围盒）或明确声明不支持
- **输入**：spec.md §5.4.1 规则 5 + §4.5.5 多显示器合并声明 + CF0 §5.4.1.7
- **输出**：`platform/mac/mac_screen_query.cpp` 多显示器合并逻辑
- **验收标准**：
  - [ ] 端点存在多显示器 + 声明合并 → 合并为单一逻辑边界，取包围盒
  - [ ] 端点存在多显示器 + 未声明合并 → 拒绝加入拓扑，提示需声明合并或不支持
  - [ ] 未声明合并策略的多显示器端点拒绝加入拓扑
  - [ ] 单元测试：多显示器合并 + 未声明合并拒绝

---

## 6. CF2-S05 修饰键状态追踪

> **本组任务实现修饰键状态追踪**，核心是 std::atomic\<ModifierState\> 无锁存储 + is_lock_free() static_assert。

### 6.1 CF2-TASK-031：ModifierTracker std::atomic\<ModifierState\> 无锁存储 + static_assert(is_lock_free())

- [ ] **描述**：实现 ModifierTracker，使用 std::atomic\<ModifierState\> 无锁存储修饰键状态，编译期 static_assert(is_lock_free())，禁止互斥量
- **输入**：spec.md §5.5.1 规则 3/4/8 + §9.5 CF2-S05-REQ-001 + design.md §2.6.5 DC-S05-001
- **输出**：`platform/mac/modifier_tracker.hpp` + `platform/mac/modifier_tracker.cpp` ModifierTracker 类
- **验收标准**：
  - [ ] **Architecture Requirement（CF0 inherited Gate）**：ModifierState 使用 `std::atomic<ModifierState>` 无锁存储，目标平台（CF0 §4.5.5 编译器矩阵）必须 `is_lock_free() == true`（编译期 `static_assert`，复用 CF0 §8.2.8 + design §2.7.3）
  - [ ] 捕获回调线程写入，FSM 线程与注入路径读取，禁止互斥量（复用 CF0 §4.6.5）
  - [ ] **Performance Evidence（测量型 DFX）**：CI / physical validation benchmark 记录并发读写延迟 P50/P95/P99，目标 P50 读 ≤100ns / 写 ≤200ns（参考硬件测量，非源码可移植硬性 Contract）
  - [ ] ModifierState 为小位图（Shift/Ctrl/Option/Cmd 4 bit），CF0 已冻结 is_lock_free static_assert
  - [ ] 单元测试：并发读写无锁 + static_assert 通过 + CI benchmark P50/P95/P99 报告

### 6.2 CF2-TASK-032：CGEventFlags 解析 + Modifier atomic update（callback 内 ≤200ns）

- [ ] **描述**：实现 CGEventFlags 解析 + Modifier atomic update：callback 内从 CGEventFlags 解析修饰键状态并 std::atomic 写入 ModifierTracker，≤200ns
- **输入**：spec.md §5.5.1 规则 1/2 + design.md §2.1.3.1 Callback Boundary 时序
- **输出**：`platform/mac/modifier_tracker.cpp` CGEventFlags 解析 + atomic update 方法
- **验收标准**：
  - [ ] 从 CGEventFlags 解析 Shift / Control / Option / Command 等修饰键按下状态
  - [ ] **callback 内执行**（MacEventFieldExtractor 的一部分），≤200ns
  - [ ] std::atomic 写入 ModifierTracker（无锁，无互斥量）
  - [ ] 单元测试：CGEventFlags 解析正确性 + ≤200ns 性能基准

### 6.3 CF2-TASK-033：修饰键显式同步注入（Handoff 后构造修饰键按下/释放事件）

- [ ] **描述**：实现修饰键显式同步注入：Handoff 完成后目标端根据源端快照显式构造并注入修饰键按下/释放事件
- **输入**：spec.md §5.5.1 规则 6 + design.md §2.6.5 DC-S05-002
- **输出**：`platform/mac/mac_event_injector.cpp` 修饰键显式同步注入方法
- **验收标准**：
  - [ ] Handoff 完成后目标端根据源端修饰键状态快照显式构造并注入修饰键按下/释放事件
  - [ ] 对齐本端 ModifierState = 源端快照
  - [ ] 禁止依赖隐式状态传递修饰键
  - [ ] 对齐日志含源端快照与目标端对齐结果
  - [ ] 单元测试：源端 Shift 按下 + Handoff 完成 → 目标端 ModifierState.Shift=true

---

## 7. CF2-S06 捕获线程与生命周期管理 + PressedStateSnapshot bitmap ownership

> **本组任务实现捕获线程与生命周期管理 + PressedStateSnapshot bitmap ownership 方案 B**，核心是方案 B SPSC snapshot publication（R5/R11 冻结）+ 不新增线程（≤8 线程）。

### 7.1 CF2-TASK-034：CaptureLifecycleManager 线程复用（CF0 #2 Capture + #3 Injection，不新增线程）

- [ ] **描述**：实现 CaptureLifecycleManager，复用 CF0 8 线程模型中的 Capture 线程与注入线程，不新增线程；CGEventTap 回调运行在 macOS 系统回调线程（不计入 8 线程预算）
- **输入**：spec.md §4.6.1 不新增线程 + §5.6.1 规则 1/10 + design.md §2.6.6 DC-S06-001 + CF0 design §2.7 8 线程模型
- **输出**：`platform/mac/capture_lifecycle_manager.hpp` + `platform/mac/capture_lifecycle_manager.cpp` CaptureLifecycleManager 类
- **验收标准**：
  - [ ] **CF2 复用 CF0 线程模型，不新增线程**；Agent 线程数 ≤8
  - [ ] CaptureLifecycleManager 复用 CF0 #2 Capture + #3 Injection 线程
  - [ ] CGEventTap 回调运行在 macOS 系统回调线程（不计入 8 线程预算，但受 ≤1ms 回调约束）
  - [ ] CF2 无裸 std::thread（使用 CF0 jthread 复用）
  - [ ] 线程统计 ≤8；CF2 新增线程数 = 0
  - [ ] 单元测试：线程数统计 + 无新增线程验证

### 7.2 CF2-TASK-035：KeyCodeBitmap (256-bit) + MouseButtonBitmap (8-bit) fixed-size 位图

- [ ] **描述**：实现 KeyCodeBitmap（固定 256-bit 位图，对应 macOS 虚拟键码 0-255）+ MouseButtonBitmap（固定 8-bit 位图，对应 MouseButton 枚举基数 ≤ 8），编译期确定
- **输入**：spec.md §4.6.4 规则 4（CF2-REQ-R5/R11 修订）+ §6.4 PressedStateSnapshot + design.md §2.7.2 PressedStateSnapshot bitmap extension
- **输出**：`core/common/key_code_bitmap.hpp` + `core/common/mouse_button_bitmap.hpp` fixed-size 位图类
- **验收标准**：
  - [ ] **KeyCodeBitmap 位宽 = 256 bit**（对应 macOS 虚拟键码 0-255，编译期确定，预留扩展）
  - [ ] **MouseButtonBitmap 位宽 = 8 bit**（对应 MouseButton 枚举基数 ≤ 8，编译期确定）
  - [ ] **禁止使用 std::vector<KeyCode> / std::vector<MouseButton>**（动态 vector 无法保证 snapshot 无 data race）
  - [ ] **禁止使用 std::atomic<KeyCodeBitmap> 整体原子化**（256-bit atomic 在目标平台不一定 lock-free，方案 B SPSC snapshot publication 已冻结为唯一 model）
  - [ ] 位图操作：set/clear/test/reset，O(1) 复杂度
  - [ ] 单元测试：位图操作正确性 + 固定大小验证

### 7.3 CF2-TASK-036：PressedStateSnapshot 方案 B SPSC snapshot publication（Capture thread owns mutable bitmap → SPSC → FSM/injection thread owns immutable snapshot）

- [ ] **描述**：实现 PressedStateSnapshot 方案 B SPSC snapshot publication：Capture thread 单线程 owns mutable KeyCodeBitmap / MouseButtonBitmap → snapshot 请求经无锁 SPSC 队列递交 → Capture thread 发布 immutable snapshot 副本 → FSM/injection thread owns immutable snapshot
- **输入**：spec.md §4.6.4 规则 4（CF2-REQ-R11 冻结唯一 ownership model = 方案 B）+ §6.4 + design.md §2.1.3.5 bitmap ownership SPSC snapshot publication 流程（第 759-811 行）+ §2.7.2
- **输出**：`platform/mac/pressed_state_snapshot_publisher.hpp` + `platform/mac/pressed_state_snapshot_publisher.cpp` SnapshotPublisher 类
- **验收标准**：
  - [ ] **方案 B SPSC snapshot publication（唯一冻结 model）**：Capture thread 单线程 owns mutable KeyCodeBitmap / MouseButtonBitmap（单线程写，无竞争）→ snapshot 请求经无锁 SPSC 队列递交 → Capture thread 发布 immutable snapshot 副本（拷贝当前 bitmap 到 snapshot 对象）→ FSM/injection thread owns immutable snapshot（单线程消费，无竞争）
  - [ ] **不依赖 std::atomic<256-bit> 的平台 lock-free 保证**（256-bit atomic 在目标平台不一定 lock-free，方案 B 已冻结为唯一 model）
  - [ ] **ModifierState 保持 std::atomic<ModifierState>（不变）**：ModifierState 为小位图，CF0 已冻结 is_lock_free static_assert
  - [ ] 生成延迟 ≤1ms（SPSC snapshot publication：Capture thread 拷贝 256+8 bit bitmap 到 snapshot 对象，无锁竞争）
  - [ ] **PressedState authoritative source 冻结**：CGEvent callback → SPSC_STATE → Capture thread → mutable bitmap → SPSC snapshot publication → FSM/Injection thread，只有 Capture thread 的 bitmap 是权威状态
  - [ ] 无锁竞争，无 data race
  - [ ] 单元测试：snapshot publication 正确性 + 无锁竞争 + ≤1ms 性能基准

### 7.4 CF2-TASK-037：SnapshotRequest SPSC ownership 明确（Producer = FSM thread 唯一，Consumer = Capture thread 唯一）

- [ ] **描述**：明确 SnapshotRequest SPSC ownership：SnapshotRequest Producer = 唯一线程/唯一逻辑 owner（FSM thread），SnapshotPublisher Consumer = Capture thread，FSM 与 Injection 不能同时为 producer
- **输入**：design.md §2.4.9 SnapshotRequest SPSC ownership 明确（Design-R8 修复，第 2592-2657 行）+ §2.6.8 R8 行
- **输出**：`platform/mac/pressed_state_snapshot_publisher.cpp` SnapshotRequest SPSC ownership 实现
- **验收标准**：
  - [ ] **SnapshotRequest Producer = FSM thread 唯一**（FSM 发起 snapshot 请求）
  - [ ] **SnapshotPublisher Consumer = Capture thread 唯一**（Capture thread 消费请求并发布 snapshot）
  - [ ] **FSM 与 Injection 不能同时为 producer**（避免双生产者破坏 SPSC 语义）
  - [ ] SnapshotRequest 经无锁 SPSC 队列递交（复用 SpscRingBuffer）
  - [ ] 单元测试：FSM 唯一 producer + Capture thread 唯一 consumer + Injection 不直接发起 SnapshotRequest

### 7.5 CF2-TASK-038：捕获与注入路径隔离（独立线程 + 无共享可变状态）

- [ ] **描述**：实现捕获与注入路径隔离：CGEventTap 捕获路径与 CGEventPost 注入路径必须运行在相互独立的线程上，禁止共享可变状态；跨路径数据交换必须经无锁队列或 std::atomic
- **输入**：spec.md §4.6.2 捕获与注入路径隔离 + §5.6.1 规则 2 + design.md §2.6.6 DC-S06-002
- **输出**：`platform/mac/capture_lifecycle_manager.cpp` 路径隔离实现
- **验收标准**：
  - [ ] CGEventTap 捕获路径与 CGEventPost 注入路径运行在相互独立的线程上（Capture thread #2 与 Injection thread #3）
  - [ ] 禁止共享可变状态
  - [ ] 跨路径数据交换必须经无锁队列（DualChannelSpsc / SnapshotPublisher）或 std::atomic（ModifierTracker）
  - [ ] 线程模型文档声明独立；代码无共享可变状态
  - [ ] 单元测试：独立线程验证 + 无共享可变状态验证

---

## 8. 单元测试

> **本组任务为各实现组的单元测试**，与实现同步进行。

### 8.1 CF2-TASK-039：AtomicPayload + SPSC RingBuffer 单元测试

- [ ] **描述**：为 AtomicPayload + SpscRingBuffer 编写单元测试，覆盖 word-atomic store/load + seqlock double-read validation + tryPush/tryPushDropOldest/tryPop + logicalDropBoundary 对齐 + INV1~INV6 + INV7a/INV7b + 区间分解
- **输入**：CF2-TASK-003~007 实现 + design.md §2.4.8 方案 A++
- **输出**：`tests/unit/spsc_ring_buffer_test.cpp` + `tests/unit/atomic_payload_test.cpp`
- **验收标准**：
  - [ ] AtomicPayload store/load 往返一致性 + word 数量正确 + trivially-copyable 校验
  - [ ] SpscRingBuffer tryPush 正常/满 + tryPushDropOldest 覆写 + tryPop 正常/空/覆写检测
  - [ ] seqlock double-read 正常路径 + 覆写检测 + 撕裂 item 丢弃
  - [ ] logicalDropBoundary 对齐 + INV2 成立 + 无逐个 seq 校验失败循环
  - [ ] 不变量在正常/Drop-Oldest/Consumer-pause 各种状态下成立 + cumulativeDropCount 可观测 + pendingDropped 随 Consumer 对齐归零
  - [ ] 全部测试 PASS

### 8.2 CF2-TASK-040：MacEventFieldExtractor + CGEventNormalizer 单元测试

- [ ] **描述**：为 MacEventFieldExtractor + CGEventNormalizer 编写单元测试，覆盖字段提取 + 类型映射 + payload 提取 + 无堆分配 + ≤100us 性能
- **输入**：CF2-TASK-012/015/016 实现 + spec.md §5.2.1
- **输出**：`tests/unit/mac_event_field_extractor_test.cpp` + `tests/unit/cg_event_normalizer_test.cpp`
- **验收标准**：
  - [ ] 字段提取正确性 + 无堆分配验证（malloc hook）+ ≤100us 性能基准
  - [ ] 全部事件类型映射 + payload 提取正确性 + 未映射键码/未知类型告警
  - [ ] CGEventNormalizer 在 Capture thread 内执行验证
  - [ ] 全部测试 PASS

### 8.3 CF2-TASK-041：DualChannelSpsc + AuthoritativeResync 单元测试

- [ ] **描述**：为 DualChannelSpsc + AuthoritativeResync 编写单元测试，覆盖双通道分发 + SPSC_STATE=64 reserved capacity + 饱和检测 + 确定性安全降级 + authoritative resynchronization
- **输入**：CF2-TASK-017/018/019 实现 + spec.md §5.2.1 规则 11
- **输出**：`tests/unit/dual_channel_spsc_test.cpp` + `tests/unit/authoritative_resync_test.cpp`
- **验收标准**：
  - [ ] 双通道分发正确性 + 容量固定 + 无堆分配 + 指标可观测
  - [ ] 正常条件下不满 + 饱和检测 + 确定性降级 + 不阻塞
  - [ ] resynchronization 触发 + 修饰键 ground truth + bitmap best-effort + FSM RECOVERY 通知 + 降级退出
  - [ ] 全部测试 PASS

### 8.4 CF2-TASK-042：MacEventInjector + ReleaseAllPressedExecutor 单元测试

- [ ] **描述**：为 MacEventInjector + ReleaseAllPressedExecutor 编写单元测试，覆盖 CGEventPost 注入 + RelativeDelta/AbsolutePosition 双语义 + releaseAllPressed bounded completion
- **输入**：CF2-TASK-022/023/024 实现 + spec.md §5.3.1
- **输出**：`tests/unit/mac_event_injector_test.cpp` + `tests/unit/release_all_pressed_executor_test.cpp`
- **验收标准**：
  - [ ] 方式 A/B 注入正确性 + AbsolutePosition 一次性定位 + ≤5ms/≤10ms 性能基准
  - [ ] 非主控端拒绝 + 非法参数丢弃 + 主控态拒绝
  - [ ] 成功路径 ≤100ms + 失败路径有界重试 + deadline reached local safety degradation + 无无限重试 + 无乘积超界
  - [ ] 全部测试 PASS

### 8.5 CF2-TASK-043：EdgeDetector + MacScreenQuery 单元测试

- [ ] **描述**：为 EdgeDetector + MacScreenQuery 编写单元测试，覆盖越界检测 + 坐标域分层 + 屏幕边界归一化 + 分辨率变更适应
- **输入**：CF2-TASK-026/027/028/029 实现 + spec.md §5.4.1
- **输出**：`tests/unit/edge_detector_test.cpp` + `tests/unit/mac_screen_query_test.cpp`
- **验收标准**：
  - [ ] 左/右越界检测 + 上/下不触发 + ≤500us 性能基准 + 不依赖画面验证
  - [ ] NSScreen 左下角原点归一化 + CGDisplay 左上角原点归一化 + 多显示器负坐标归一化
  - [ ] 分辨率变更 1080p → 2160p ≤1s 更新 + 通知 CF0
  - [ ] 全部测试 PASS

### 8.6 CF2-TASK-044：ModifierTracker + PressedStateSnapshot 单元测试

- [ ] **描述**：为 ModifierTracker + PressedStateSnapshot 编写单元测试，覆盖无锁状态 + CGEventFlags 解析 + bitmap ownership 方案 B SPSC snapshot publication
- **输入**：CF2-TASK-031/032/036 实现 + spec.md §5.5.1 + §4.6.4
- **输出**：`tests/unit/modifier_tracker_test.cpp` + `tests/unit/pressed_state_snapshot_test.cpp`
- **验收标准**：
  - [ ] 并发读写无锁 + static_assert 通过 + CI benchmark P50/P95/P99 报告
  - [ ] CGEventFlags 解析正确性 + ≤200ns 性能基准
  - [ ] snapshot publication 正确性 + 无锁竞争 + ≤1ms 性能基准
  - [ ] 全部测试 PASS

---

## 9. 集成测试 + 10 项 Contract 验证

> **本组任务为集成测试 + 10 项 Contract 验证**，是 CF2 最危险的 10 项 Contract 的验收证据，最高优先级。

### 9.1 CF2-TASK-045：SPSC Drop-Oldest 并发压力测试 TEST-R14-LIN + TEST-R15-LIFETIME（Contract #1 + #4）

- [ ] **描述**：实现 SPSC Drop-Oldest 并发压力测试 TEST-R14-LIN（60s TSan + Consumer 返回 item 序号严格递增 + droppedOldestCount 一致性）+ TEST-R15-LIFETIME（60s TSan + Consumer 暂停注入 + 覆写校验生效 + item 序号严格递增）
- **输入**：design.md §2.4.8 TEST-R14-LIN（第 1730-1739 行）+ TEST-R15-LIFETIME + Verification Matrix R7/R11/R14/R15 行
- **输出**：`tests/integration/test_r14_lin.cpp` + `tests/integration/test_r15_lifetime.cpp`
- **验收标准**：
  - [ ] **Contract #1 SPSC_DATA=256 / Drop-Oldest**：TEST-R14-LIN 配置 Capacity=256，Producer 线程高频 tryPushDropOldest（每 100µs 一次，持续 60s，共 600k 次写入），Consumer 线程高频 tryPop（每 50µs 一次，持续 60s）
  - [ ] **无 data race**：ThreadSanitizer (TSan) 运行 60s 无 data race 报告（HARD CONTRACT）
  - [ ] **seq 单调**：head / tail / 各 slot seq 单调递增，无回绕（运行时断言）
  - [ ] **Consumer 返回的每个 item 的 seq 严格递增**：Consumer 记录每次成功 tryPop 返回的 item 内嵌的写入序号，验证序号严格递增（无重复、无回退）
  - [ ] **Drop Oldest 语义**：统计 droppedOldestCount，验证 Consumer 返回的 item 数 + droppedOldestCount == Producer 写入数（无遗漏、无重复消费）
  - [ ] **Contract #4 Slot lifetime / R15-LIFETIME**：TEST-R15-LIFETIME 含 Consumer 暂停注入（arbitrary consumer pause）+ 覆写校验生效（s1 != s2 丢弃）+ item 序号严格递增
  - [ ] **J7 关键 interleaving 验证**：s1 读后暂停、Producer 覆写、Consumer 恢复读 payload + s2，s1 != s2 丢弃，无 data race
  - [ ] **J8 payload 读中途暂停验证**：payload 读中途暂停、Producer 覆写 word 1、Consumer 恢复读 word 1，撕裂 item 被 s1 != s2 校验丢弃
  - [ ] CI nightly 集成：TSan + 60s 压力测试（MEASUREMENT REQUIREMENT）

### 9.2 CF2-TASK-046：R16 logicalDropBoundary 测试 TEST-R16-LOGICAL-DROP-BOUNDARY（Contract #2 + #5）

- [ ] **描述**：实现 R16 logicalDropBoundary 测试 TEST-R16-LOGICAL-DROP-BOUNDARY：Consumer 长期暂停 + Producer 高频写入 + 恢复后 logicalDropBoundary 对齐 + INV2 恒成立 + 区间分解
- **输入**：design.md §2.4.8 R16 修复子节 + Verification Matrix R16 行（第 2790 行）
- **输出**：`tests/integration/test_r16_logical_drop_boundary.cpp`
- **验收标准**：
  - [ ] **Contract #2 logicalDropBoundary + Contract #5 R16**：Consumer 长期暂停（Δt → ∞）+ Producer 高频写入（head 持续推进，tail 不变，head - tail → ∞）
  - [ ] **INV2 `head - logicalDropBoundary ≤ Capacity` 在 Consumer 任意暂停期间恒成立**（logicalDropBoundary 跟随 head 推进）
  - [ ] **恢复后 R16 对齐**：Consumer 恢复后读 head（acquire），计算 logicalDropBoundary = head - Capacity，对齐跳过所有已 drop slot，从最旧有效 slot 开始读
  - [ ] **五种 Consumer-pause 状态 Q1~Q5 穷举完备**：Q1 Full / Q2 Non-Full / Q3 Consumer Paused / Q4 Consumer Validating / Q5 Consumer After Payload Copy
  - [ ] **区间分解验证**：tail ≤ logicalDropBoundary ≤ head + head - logicalDropBoundary ≤ Capacity + [logicalDropBoundary, tail) 为空
  - [ ] **R16 INV2 运行时断言**（DEBUG build）
  - [ ] **旧 INV7/INV8 已从 Verification Matrix 中彻底删除**（v1.7）
  - [ ] CI nightly 集成

### 9.3 CF2-TASK-047：R17 概念分离测试 TEST-R17-CONCEPT-SEPARATION + TEST-R17-INTERVAL-DECOMPOSITION（Contract #5）

- [ ] **描述**：实现 R17 概念分离测试 TEST-R17-CONCEPT-SEPARATION（pendingDropped 瞬时派生量 vs cumulativeDropCount 累计统计量）+ TEST-R17-INTERVAL-DECOMPOSITION（区间分解验证）
- **输入**：design.md §2.4.8 R17 修复子节 + 区间分解证明（第 2149-2172 行）+ Verification Matrix R17 行（第 2793 行）
- **输出**：`tests/integration/test_r17_concept_separation.cpp` + `tests/integration/test_r17_interval_decomposition.cpp`
- **验收标准**：
  - [ ] **Contract #5 R17**：TEST-R17-CONCEPT-SEPARATION 反例验证——Producer 写 300 → pendingDropped=44, cumulativeDropCount=44；Consumer 消费 100 → pendingDropped=0, cumulativeDropCount=44 不变
  - [ ] **pendingDropped 为派生量（非存储）** + **cumulativeDropCount 为 std::atomic\<uint64_t\>** + 两者不混用
  - [ ] **cumulativeDropCount 不参与 correctness 证明**（observational metric，v1.7 Design-R17-OBSERVATIONAL）
  - [ ] TEST-R17-INTERVAL-DECOMPOSITION 区间分解验证：tail ≤ logicalDropBoundary ≤ head + head - logicalDropBoundary ≤ Capacity + [logicalDropBoundary, tail) 为空 + 区间不重叠且覆盖 [0, head)
  - [ ] **旧 INV7 `droppedCount = effectiveTail - tail` 已从 Contract/不变量列表/Verification Matrix 中彻底删除**
  - [ ] **旧 INV8 `head = droppedCount + tail + (head - effectiveTail)` 已从 Contract/不变量列表/Verification Matrix 中彻底删除**
  - [ ] CI nightly 集成

### 9.4 CF2-TASK-048：R18 线性化测试 TEST-R18-LINEARIZATION + TEST-R18-DROP-DEFINITION（Contract #6）

- [ ] **描述**：实现 R18 线性化测试 TEST-R18-LINEARIZATION（60s TSan + Consumer 暂停注入触发 P5/P6/P7 + Lp < Lc 旧 item 不返回 + Lc < Lp 旧 item 可返回 + item 序号严格递增）+ TEST-R18-DROP-DEFINITION（定义 A 与机制 B/C 一致性验证）
- **输入**：design.md §2.4.8 R18 修复子节 + Verification Matrix R18 行（第 2794 行）
- **输出**：`tests/integration/test_r18_linearization.cpp` + `tests/integration/test_r18_drop_definition.cpp`
- **验收标准**：
  - [ ] **Contract #6 R18 P1-P8 interleavings**：TEST-R18-LINEARIZATION 60s TSan + Consumer 暂停注入触发 P5/P6/P7
  - [ ] **核心结论验证**：`Lp < Lc` 旧 item 不返回（Producer 在 Consumer s2 读之前已 seq.store(new_epoch)，s2 > s1 → s1 != s2 → 丢弃）+ `Lc < Lp` 旧 item 可返回 legitimately（线性化为 Consumer Pop < Producer Push）
  - [ ] **item 序号严格递增**
  - [ ] **P1-P8 每种情况明确四要素**：Producer LP / Consumer LP / Drop LP（语义发生点）/ Consumer 最终接受/拒绝原因
  - [ ] **Epoch Validity Linearization Theorem 验证**：(1) 已失效 epoch 不得被 Consumer 在 Pop LP 后返回；(2) 仍有效 epoch 可合法返回
  - [ ] TEST-R18-DROP-DEFINITION 定义 A（Producer-side logical drop）与机制 B/C（Consumer 发现 drop 的机制）一致性验证
  - [ ] CI nightly 集成

### 9.5 CF2-TASK-049：AtomicPayload 平台契约测试 TEST-ATOMIC-PAYLOAD-MATRIX（Contract #3）

- [ ] **描述**：实现 AtomicPayload 平台契约测试 TEST-ATOMIC-PAYLOAD-MATRIX：x86_64/arm64 双架构 build 通过 + 非 x86_64/arm64 build 失败 + is_lock_free() 检查 + negative test
- **输入**：design.md §2.4.8 AtomicPayload 平台契约子节 + Verification Matrix AtomicPayload 行（第 2791/2795 行）
- **输出**：`tests/integration/test_atomic_payload_matrix.cpp` + CMake build matrix 配置
- **验收标准**：
  - [ ] **Contract #3 AtomicPayload 平台契约**：CI x86_64/arm64 双架构 build 通过
  - [ ] 非 x86_64/arm64 build 失败 + 错误信息含 `CFX-E-CAP-ARCH-UNSUPPORTED`
  - [ ] 编译期 architecture guard `#if !defined(__x86_64__) && !defined(__aarch64__) #error` 验证
  - [ ] 编译期 `static_assert(std::atomic<uint64_t>{}.is_lock_free())`（使用 `is_lock_free()` 非 `is_always_lock_free`）验证
  - [ ] **不 fallback 到 mutex-based atomic**（违反 Input Plane 非阻塞设计原则）
  - [ ] **不声称"所有 C++20 平台支持"**
  - [ ] negative test：32-bit ARM 等平台 build 失败
  - [ ] CI build matrix 集成

### 9.6 CF2-TASK-050：Callback Boundary 集成测试（Contract #8 callback → enqueue → Capture thread 严格边界）

- [ ] **描述**：实现 Callback Boundary 集成测试：验证 callback 仅做 minimal field extraction + Modifier atomic update + RawInputEvent enqueue，不调用 onEvent/FSM/Edge Detection/downstream dispatch；CGEventNormalizer 仅在 Capture thread 内
- **输入**：design.md §2.1.3.1 Callback Boundary 时序 + Verification Matrix R5 行（第 2781 行）+ spec.md §5.1.1 规则 2/9
- **输出**：`tests/integration/test_callback_boundary.cpp`
- **验收标准**：
  - [ ] **Contract #8 callback → enqueue → Capture thread 严格边界**：callback 内仅 MacEventFieldExtractor（minimal field extraction + Modifier atomic update + RawInputEvent enqueue）
  - [ ] **CGEventNormalizer 不出现在 callback 边界内**（Design-R5 严格命名）
  - [ ] **callback MUST NOT**：call onEvent / call FSM / perform edge detection / perform handoff decision / perform downstream dispatch / perform blocking operation
  - [ ] 回调耗时测量 ≤1ms
  - [ ] 回调代码无锁/IO/重处理/onEvent/FSM/Edge Detection
  - [ ] onEvent 由 Capture/Input thread 触发，不由 callback 触发
  - [ ] 越界事件由 Capture/Input thread 后处理经 SPSC 队列递交 FSM
  - [ ] 全部测试 PASS

### 9.7 CF2-TASK-051：SPSC_STATE 饱和安全降级集成测试（Contract #7 饱和 → authoritative resynchronization → FSM RECOVERY）

- [ ] **描述**：实现 SPSC_STATE 饱和安全降级集成测试：异常过载触发 SPSC_STATE 饱和 → stateChannelSaturated=true + 告警 → Capture thread authoritative resynchronization（修饰键 ground truth + bitmap best-effort + FSM RECOVERY）→ 用户松手后系统回到 P1 ∧ P2
- **输入**：spec.md §9.2 CF2-S02-REQ-003 + design.md §2.1.3.3 SPSC_STATE 饱和安全降级状态机 + §2.4.6 recovery 时序 + §2.6.8 R6 行
- **输出**：`tests/integration/test_state_channel_saturation.cpp`
- **验收标准**：
  - [ ] **Contract #7 SPSC_STATE=64 saturation**：异常过载 + SPSC_STATE 队列满 + callback enqueue Key/Button Press/Release → 确定性安全降级：stateChannelSaturated=true + stateChannelSaturatedCount++ + 告警 CFX-E-CAP-STATE-CHANNEL-SATURATED，callback 不阻塞
  - [ ] Capture thread 检测饱和 → authoritative resynchronization：修饰键 = CGEventSourceFlagsState ground truth，按键/按钮 = bitmap best-effort + stale，FSM 进 RECOVERY
  - [ ] **resynchronization 后**：ModifierState = macOS HID ground truth；KeyCodeBitmap/MouseButtonBitmap = best-effort + stale；FSM 在 RECOVERY 态停止捕获，用户松手后系统回到 P1 ∧ P2，P3 Recoverable 可证，无无限卡死，无虚假控制权
  - [ ] **不依赖 gap counter 神奇恢复**，不依赖被丢弃的单个事件
  - [ ] **PressedState authoritative source 链路审查可追溯**：CGEvent callback → SPSC_STATE → Capture thread → mutable bitmap → SPSC snapshot → FSM/Injection thread，只有 Capture thread bitmap 是权威状态
  - [ ] **System Safety Recovery ≤1.1ms（DESIGN TARGET）/ ≤12ms（T_system_recovery_test HARD CONTRACT）**内系统进入 RECOVERY 态
  - [ ] 全部测试 PASS

### 9.8 CF2-TASK-052：releaseAllPressed bounded completion 集成测试（Contract #9 total deadline ≤100ms + 失败路径 local safety degradation）

- [ ] **描述**：实现 releaseAllPressed bounded completion 集成测试：成功路径 ≤100ms total budget 内全部释放 + 失败路径在剩余 total budget 内有界重试 + deadline reached local safety degradation
- **输入**：spec.md §9.3 CF2-S03-REQ-002 + design.md §2.1.3.4 releaseAllPressed bounded completion 流程 + §2.6.3 DC-S03-002
- **输出**：`tests/integration/test_release_all_pressed_bounded.cpp`
- **验收标准**：
  - [ ] **Contract #9 releaseAllPressed ≤100ms**：成功路径——被控态链路断开 + 存在按下键 A、B + 按下鼠标左键 + 注入全部成功 → ≤100ms total budget 内注入 A 释放、B 释放、左键释放，本端无残留，状态 CLOSED
  - [ ] 失败路径——CGEventPost 失败或释放后仍有残留 → 在剩余 total budget ≤100ms 内有界重试，不无限重试，不超出 100ms total deadline
  - [ ] deadline reached——100ms total deadline reached 仍失败 → 强制清空内部按下状态 + 告警 CFX-E-INJ-RELEASE-FAILED + 通知 CF0 FSM 进 RECOVERY，P3 Recoverable 保持
  - [ ] **无无限重试循环，total deadline ≤100ms bounded completion 可验证，无"次数 × 每次时延"乘积超界风险**
  - [ ] 断线释放日志含释放清单、total elapsed 耗时、最终处置（CLOSED / degraded）；失败路径含剩余预算内重试计数与 CFX-E-INJ-RELEASE-FAILED 告警；total elapsed ≤ 100ms 可验证
  - [ ] 全部测试 PASS

### 9.9 CF2-TASK-053：CF2-S01~S06 Design Contract 架构测试

- [ ] **描述**：实现 CF2-S01~S06 Design Contract 架构测试，验证全部 Design Contract 在实现中成立
- **输入**：design.md §2.6.1~§2.6.6 Verification Matrix 回映 + §2.6.8 Design-R1~R18 修订回映
- **输出**：`tests/architecture/cf2_design_contract_test.cpp`
- **验收标准**：
  - [ ] DC-S01-001：platform/mac/ 仅使用 CGEventTap 等 Core Graphics 用户态 API
  - [ ] DC-S01-002：callback 仅 MacEventFieldExtractor + Modifier atomic update + RawInputEvent enqueue，≤1ms 返回
  - [ ] DC-S01-003：A11yPermissionGuard 检测 Accessibility + Input Monitoring
  - [ ] DC-S02-001：platform/mac/ 消化 macOS 平台逻辑；Core 层零平台头文件
  - [ ] DC-S02-002：CGEventNormalizer 映射表覆盖全部 CGEventType
  - [ ] DC-S02-003：DualChannelSpsc 双通道 SPSC + AuthoritativeResync 确定性安全降级
  - [ ] DC-S03-001：MacEventInjector 校验 sourceNodeId == 当前主控端
  - [ ] DC-S03-002：ReleaseAllPressedExecutor total deadline ≤100ms bounded completion
  - [ ] DC-S04-001：EdgeDetector 仅基于光标坐标与 ScreenBoundary 比较（Design-R1 坐标域分层）
  - [ ] DC-S04-002：MacScreenQuery 分辨率变更 ≤1s 更新 + 通知 CF0
  - [ ] DC-S05-001：ModifierTracker 用 std::atomic\<ModifierState\> + static_assert(is_lock_free())
  - [ ] DC-S05-002：MacEventInjector Handoff 后显式对齐 ModifierState
  - [ ] DC-S06-001：CF2 复用 CF0 Capture + Injection 线程，不新增
  - [ ] DC-S06-002：捕获与注入独立线程，无共享可变状态
  - [ ] 全部测试 PASS

---

## 10. CF0 Safety Invariant 保持验证 + DFX 红线验证 + 架构冻结审查

> **本组任务为 CF0 Safety Invariant 保持验证 + DFX 红线验证 + 架构冻结审查**，是 CF2 冻结的最终 Gate。

### 10.1 CF2-TASK-054：CF0 Safety Invariant P1/P2/P3 保持验证（Contract #10 P3 只证明 bounded safety degradation）

- [ ] **描述**：验证 CF2 全部机制不破坏 CF0 Architecture Safety Invariant P1（No Split-Brain）/ P2（No Void-Owner）/ P3（Recoverable），特别验证 P3 只证明 bounded safety degradation，不证明 deterministic RECOVERY→NORMAL
- **输入**：spec.md §7.5 CF0 Architecture Safety Invariant 延续 + design.md §2.5 CF0 Safety Invariant 保障 + §2.5.3 P3 Recoverable + §2.6.7 Safety Invariant 对齐回映
- **输出**：`tests/architecture/cf0_safety_invariant_test.cpp`
- **验收标准**：
  - [ ] **P1 No Split-Brain**：CF2 的捕获失败、注入失败、权限缺失不得产生虚假控制权；主控态捕获失败时本端保持控制权，不产生双主控
  - [ ] **P2 No Void-Owner**：CF2 的捕获失败、CGEventTap 异常、权限缺失不得导致本地键鼠永久失效；降级态下本端物理键鼠仍作用于本机
  - [ ] **Contract #10 P3 只证明 bounded safety degradation**：**CF2 proves bounded safety degradation and preservation of P1∧P2. It does not prove deterministic bounded return to NORMAL.**
  - [ ] **System Safety Recovery**：系统侧 ≤1.1ms（DESIGN TARGET）/ ≤12ms（T_system_recovery_test）内进入 RECOVERY 态，保证 P1∧P2 不破坏，此为系统可控 bounded completion
  - [ ] **User-dependent convergence**：依赖 T_user_release（用户松手，无界），不宣称 deterministic bounded upper bound；系统在 RECOVERY 态期间保持 P1∧P2，不无限卡死，不产生虚假控制权
  - [ ] CF0-ARCH-SAFETY-001~006 在 CF2 阶段继续可验证通过
  - [ ] CF2 新增机制不得引入新的 Safety Invariant 破坏路径
  - [ ] 全部测试 PASS

### 10.2 CF2-TASK-055：P3 表述严格限定验证（不证明 deterministic RECOVERY→NORMAL，Contract #10）

- [ ] **描述**：验证 P3 表述严格限定：明确区分"系统侧安全降级 bounded"（可证）与"return to NORMAL"（不证 deterministic bounded，依赖 T_user_release eventual convergence），防止 Coding Agent 把 RECOVERY→NORMAL 实现成"必须在 X ms 内完成"的错误硬约束
- **输入**：design.md §2.4.6 P3 严格表述（第 1484-1487 行）+ §2.5.3 措施 6 + §2.6.8 P3 表述严格限定行（第 2798 行）
- **输出**：`tests/architecture/p3_strict_statement_test.cpp` + 代码审查清单
- **验收标准**：
  - [ ] **Contract #10 P3 只证明 bounded safety degradation，不证明 deterministic RECOVERY→NORMAL**
  - [ ] 文档统一采用 "**CF2 proves bounded safety degradation and preservation of P1∧P2. It does not prove deterministic bounded return to NORMAL.**"
  - [ ] 不写 "P3 可证" 避免歧义
  - [ ] 明确区分 "系统侧安全降级 bounded"（可证）与 "return to NORMAL"（不证 deterministic bounded，依赖 T_user_release eventual convergence）
  - [ ] **防止 Coding Agent 把 RECOVERY→NORMAL 实现成 "必须在 X ms 内完成" 的错误硬约束**（代码审查：RECOVERY→NORMAL 转移不包含硬超时约束）
  - [ ] T_user_release 标注无界（用户可控，不属系统可控时间上界）
  - [ ] 全部测试 PASS + 代码审查清单闭合

### 10.3 CF2-TASK-056：DFX 红线验证（回调 ≤1ms + 无锁热路径 + 用户态约束 + ≤8 线程）

- [ ] **描述**：验证 CF2 DFX 红线：回调 ≤1ms + 无锁热路径 + 用户态约束 + ≤8 线程 + CGEvent 到 RawInputEvent 转换 ≤100us + CGEventPost 注入 ≤5ms + 屏幕边界查询 ≤10ms + 边缘越界检测 ≤500us
- **输入**：spec.md §4.1 性能 + §4.5 兼容性 + §4.6 并发与线程安全 + design.md §2.4 工程边界量化
- **输出**：`tests/architecture/dfx_redline_test.cpp` + CI 性能基准报告
- **验收标准**：
  - [ ] **回调 ≤1ms**（复用 CF0 §4.6.4），回调内禁止锁等待/IO/内存分配/跨平面调用/同步日志写入
  - [ ] **CGEvent 到 RawInputEvent 转换 ≤100us**（仅做类型映射与字段提取，禁止重处理）
  - [ ] **CGEventPost 注入 ≤5ms**，批量注入（injectBatch）单帧延迟 ≤10ms
  - [ ] **屏幕边界查询 ≤10ms**，查询结果可缓存
  - [ ] **边缘越界检测 ≤500us**（在 Capture/Input thread 内完成）
  - [ ] **修饰键状态读取 P50 ≤100ns / 写 ≤200ns**（CI benchmark，测量型 DFX）
  - [ ] **按下状态快照生成 ≤1ms**
  - [ ] **捕获启停 ≤50ms**
  - [ ] **无锁热路径**：std::atomic 修饰键状态，无互斥量
  - [ ] **用户态约束红线**：CF2 全部实现禁止依赖内核扩展、驱动安装、root 特权或 SIP 禁用
  - [ ] **≤8 线程**：CF2 复用 CF0 8 线程模型，不新增线程
  - [ ] 全部测试 PASS + CI 性能基准报告

### 10.4 CF2-TASK-057：CF0 Frozen Boundary Amendment 验证（additive extension + 不修改 CF0/CF1 Frozen）

- [ ] **描述**：验证 CF0 Frozen Boundary Amendment：CF2 的全部 CF0/Core 边界变化（onEdgeOverflow() + PressedStateSnapshot bitmap extension）均为 additive extension，在 CF0 design 预留扩展空间内，不修改 CF0/CF1 Frozen 文档，不重新设计 Handoff FSM，不改变 NodeID/Topology Authority 语义，不破坏 Safety Invariant
- **输入**：design.md §2.7 CF0 Frozen Boundary Amendment / Compatibility Contract（第 2800-2859 行）+ §2.6.8 R10 行
- **输出**：`tests/architecture/cf0_frozen_boundary_test.cpp` + git diff 验证脚本
- **验收标准**：
  - [ ] **CF0/CF1 Frozen 文档 diff 验证**：CF2 不修改 CF0/CF1 任何 Frozen 文档（HARD CONTRACT，git diff 验证）
  - [ ] **IHandoffOrchestrator.onEdgeOverflow() 边界扩展**：CF2 新增 `onEdgeOverflow(const EdgeOverflowEvent& event)` 方法，仅为越界事件提供消费入口，不改变 FSM 状态转移逻辑；触发 FSM 既有 ARMED→PENDING 转移，不引入新状态、不修改转移条件、不改变冷却/驻留/熔断参数
  - [ ] **PressedStateSnapshot bitmap extension 边界扩展**：内部承载从 vector 改为 bitmap 是性能/并发安全增强，不改变语义；stale flag 是新增字段（默认 false，向后兼容）
  - [ ] **CF2 additive extension 审查**：onEdgeOverflow() + PressedStateSnapshot bitmap extension 均为 additive，无既有方法签名修改（HARD CONTRACT，源码审查 + diff 验证）
  - [ ] **CF2 不重新设计 FSM**：onEdgeOverflow() 不能演化成 CF2 自己重新设计 Handoff FSM；FSM 驱动权属 CF0-S03，CF2 仅提供信号
  - [ ] **不改变 NodeID/Topology Authority 语义**：CF2 复用 CF1 NodeID 作为源端标识，不修改身份与发现
  - [ ] **CF0 Safety Invariant P1/P2/P3 在 CF2 additive extension 后继续可验证通过**
  - [ ] 全部测试 PASS + git diff 验证闭合

### 10.5 CF2-TASK-058：CF2 架构冻结审查与交付

- [ ] **描述**：CF2 架构冻结审查与交付，汇总全部任务状态 + 10 项 Contract 覆盖情况 + CF2-S01~S06 六模块覆盖情况 + Gate Review 准备
- **输入**：CF2-TASK-001~057 全部任务状态 + 10 项 Contract 验证结果 + CF2-S01~S06 Design Contract 架构测试结果
- **输出**：CF2 架构冻结审查报告 + tasks.md 最终状态更新 + Gate Review 交付包
- **验收标准**：
  - [ ] 全部 58 个任务 Implementation ✅ + Acceptance/Evidence 🟢（除 TASK-058 本身为 N/GATE）
  - [ ] **10 项 Contract 全部覆盖并验证通过**：
    - Contract #1 SPSC_DATA=256 / Drop-Oldest → TASK-005 + TASK-045 ✅
    - Contract #2 logicalDropBoundary → TASK-006 + TASK-046 ✅
    - Contract #3 AtomicPayload word-atomic + lock-free → TASK-003 + TASK-008 + TASK-049 ✅
    - Contract #4 Slot lifetime / R15-LIFETIME → TASK-004 + TASK-045 ✅
    - Contract #5 R16 logical-drop boundary → TASK-006 + TASK-007 + TASK-046 ✅
    - Contract #6 R18 P1-P8 interleavings → TASK-009 + TASK-048 ✅
    - Contract #7 SPSC_STATE=64 saturation → TASK-018 + TASK-051 ✅
    - Contract #8 callback → enqueue → Capture thread 严格边界 → TASK-012 + TASK-015 + TASK-050 ✅
    - Contract #9 releaseAllPressed ≤100ms → TASK-024 + TASK-052 ✅
    - Contract #10 P3 只证明 bounded safety degradation → TASK-054 + TASK-055 ✅
  - [ ] **CF2-S01~S06 六模块全部覆盖并验证通过**：
    - CF2-S01 macOS 用户态输入捕获 → TASK-010~014 ✅
    - CF2-S02 CGEvent 规范化适配 → TASK-015~020 ✅
    - CF2-S03 macOS 用户态输入注入 → TASK-021~025 ✅
    - CF2-S04 屏幕边界查询与边缘越界检测 → TASK-026~030 ✅
    - CF2-S05 修饰键状态追踪 → TASK-031~033 ✅
    - CF2-S06 捕获线程与生命周期管理 → TASK-034~038 ✅
  - [ ] CF0 Safety Invariant P1/P2/P3 保持验证通过
  - [ ] DFX 红线验证通过
  - [ ] CF0 Frozen Boundary Amendment 验证通过
  - [ ] **CF2 最终状态**：FINAL PASS / FROZEN / CLOSED（待 Final Gate Review 裁决）

---

> **文档结束**
> 本任务规划定义 CF2-S01~CF2-S06 六个 macOS 输入捕获地基的编码任务，共 10 组 58 个任务，严格遵循 CF0/CF1 冻结的全部架构基线（C++20、Driverless User-Mode、Handoff 六态 FSM、7 契约、Safety Invariant P1/P2/P3、8 线程模型、双平面隔离、Coordinate Space 双语义、无锁 SPSC 队列），落地"Driverless User-Mode Architecture —— macOS 输入捕获与注入必须完全在用户态完成"第一原则。
> **10 项 Contract 覆盖**：全部 10 项 Contract 均有对应实现任务 + 验证任务，详见各任务验收标准中的 Contract 引用。
> **执行纪律**：Design PASS ≠ Coding Authorization。本任务规划完成后需经 Task Gate Review 裁决后再进入 Coding Authorization → C++ Implementation。
> **保持不变**：不修改 CF0/CF1 Frozen 文档；不引入 Coordinator election / Raft / Paxos；不改变 NodeID/Topology Authority 语义；不修改 Handoff FSM；不引入内核扩展或驱动；不采集屏幕画面；不直接 Coding。