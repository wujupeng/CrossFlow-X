# CrossFlow-X · CF0 架构规格冻结编码任务规划

> **阶段标记**：CF0 — Architecture Specification / Freeze
> **对应需求规格**：`.codeartsdoer/specs/cf0_arch_freeze/spec.md`（v2，892 行，CF0-S01～S05 + 7 核心契约 + C++20 约束）
> **对应实现方案**：`.codeartsdoer/specs/cf0_arch_freeze/design.md`（v3，3449 行，六模块总体架构 + 防抖动 + 并发 + ACK 事务 + Safety Invariant）
> **技术栈**：C++20 + CMake + CTest + platform conditional compile（macOS Apple Clang≥15 / Windows MSVC≥19.3x & Clang≥17）
> **架构原则**：Driverless User-Mode Architecture（第一版不碰驱动）
> **任务编号格式**：`CF0-TASK-XXX`（大写连字符）
> **文件命名规范**：snake_case（如 `handoff_fsm.cpp`、`spsc_queue.hpp`）
> **任务粒度**：每个任务可在单次编码会话内完成（≤4 小时）
> **v3 更新摘要**：C++17→C++20 升级；Handoff FSM 三态→六态 + 12 条转移路径；防抖动四重契约；8 线程并发模型；ACK 事务语义；Coordinate Space 双语义；7 核心契约架构测试 + Safety Invariant

---

## 任务依赖总览

```
[Group 1 基础设施(C++20)] ──┐
                             ├──> [Group 2 领域模型] ──┬──> [Group 3 跨平台抽象层 F]
                             │                          ├──> [Group 4 CF0-S01]
                             │                          ├──> [Group 5 CF0-S02]
                             │                          ├──> [Group 6 CF0-S04]
                             │                          └──> [Group 7 CF0-S05]
                             │                                    │
                             │                                    v
                             │                          [Group 8 防抖动机制]
                             │                                    │
                             │                          [Group 9 并发模型]
                             │                                    │
                             v                                    v
                             └────────────> [Group 10 CF0-S03 FSM] (依赖 S01/S02/S04/S05/防抖动/并发)
                                                │
                             [Group 11 C++20方案] │
                                                v
[Group 12 单元测试] <─────────── 各实现组并行触发 ──────────────────
                             │
                             v
[Group 13 集成测试] ──> [Group 14 架构契约测试] ──> [Group 15 验证与冻结]
```

**关键依赖链**：
- C++20 基础设施（Group 1）必须最先完成，所有上层依赖 C++20 设施
- 模块 F（跨平台抽象层）必须先于 S01（捕获/注入依赖抽象接口）
- S04（坐标映射）必须先于 S03（边缘检测依赖边界）
- S05（传输层）必须先于 S03（握手依赖 Control Plane）
- S02（拓扑）必须先于 S03（邻居查询）与 S01（源端 NodeID 标注）
- 防抖动机制（Group 8）必须先于 S03（FSM 依赖四重契约）
- 并发模型（Group 9）必须先于 S03（FSM 依赖线程模型与无锁队列）
- S03（Handoff 状态机）依赖 S01+S02+S04+S05+防抖动+并发，置于最后实现
- 架构契约测试（Group 14）依赖全部实现组完成

---

## 1. 项目骨架与基础设施（C++20 升级）

> 本组任务建立 C++20 项目可编译骨架与全局基础设施。v3 关键变更：C++17→C++20 升级，编译器版本检测，coroutine 启用。

### 1.1 CF0-TASK-001 C++20 项目骨架与 CMake 构建系统

- **任务标题**：建立 C++20 项目骨架与 CMake 多模块构建系统
- **任务描述**：创建 CrossFlow-X Agent 的 C++20 项目骨架，按六模块划分目录结构（`core/s01_event`、`core/s02_topology`、`core/s03_handoff`、`core/s04_coord`、`core/s05_transport`、`core/common`、`platform/mac`、`platform/win`、`platform/common`），配置 CMake 顶层 `CMakeLists.txt` 与各模块子 CMake，设置 `CMAKE_CXX_STANDARD 20` + `CMAKE_CXX_STANDARD_REQUIRED ON` + `CMAKE_CXX_EXTENSIONS OFF`，启用 CTest 测试框架，配置编译选项（Clang/GCC: `-Wall -Wextra -Werror -Wpedantic -std=c++20`；MSVC: `/W4 /WX /permissive- /utf-8 /Zc:__cplusplus /std:c++20`），输出可编译的空骨架（含 `main.cpp` 占位）
- **依赖任务**：无
- **输入**：design.md §2.1.2 服务/组件总体架构、§2.4.3 平台编译期选择策略、§2.8.1 编译基线与编译器矩阵、§2.8.4 CMake 构建方案升级
- **输出**：
  - `CMakeLists.txt`（顶层，`CXX_STANDARD 20`）
  - `cmake/Platform.cmake`（平台检测 + 编译器版本检测）
  - `cmake/CompilerCheck.cmake`（编译器版本检测）
  - `core/s0{1..5}_*/CMakeLists.txt`、`core/common/CMakeLists.txt`
  - `platform/{mac,win,common}/CMakeLists.txt`
  - `tests/CMakeLists.txt`
  - `src/main.cpp`（占位）
- **验收标准**：
  1. macOS Apple Clang≥15 上 `cmake -B build && cmake --build build` 成功
  2. Windows MSVC≥19.3x 与 Clang≥17 均可构建成功
  3. `CXX_STANDARD` 严格为 20，CI 检查禁降级
  4. `ctest --test-dir build` 可执行（无测试时输出 0 tests）
  5. 目录结构对齐 design.md 六模块划分
- **预估复杂度**：中
- **平台归属**：跨平台共享

### 1.2 CF0-TASK-002 跨平台编译期选择与编译器版本检测

- **任务标题**：实现 CMake 平台条件编译、编译器版本检测与接口绑定机制
- **任务描述**：在 `cmake/Platform.cmake` 中实现 `if(APPLE)` / `if(WIN32)` 平台检测与目标绑定逻辑，定义 `CFX_PLATFORM_MAC` / `CFX_PLATFORM_WIN` 编译宏；在 `cmake/CompilerCheck.cmake` 中实现编译器版本检测（Apple Clang≥15 / MSVC≥19.3 / Clang≥17），不达标 `message(FATAL_ERROR)`；配置 coroutine 启用选项（按编译器需要添加 `-fcoroutines`）；上层模块仅依赖抽象接口目标 `cfx_platform_common`，不直接依赖平台实现目标
- **依赖任务**：CF0-TASK-001
- **输入**：design.md §2.4.3 平台编译期选择策略、§2.8.1 编译器矩阵、§2.8.4 CMake 升级
- **输出**：`cmake/Platform.cmake` 更新、`cmake/CompilerCheck.cmake` 新增、各平台 CMake 目标定义
- **验收标准**：
  1. 上层模块 CMakeLists 不出现平台实现目标名
  2. macOS 构建产物不含 Windows 实现目标文件，反之亦然
  3. 编译器版本不达标时 CMake 配置阶段即 FATAL_ERROR
  4. 不使用运行时 dlopen/插件机制
  5. coroutine 编译选项按平台正确启用
- **预估复杂度**：低
- **平台归属**：跨平台共享

### 1.3 CF0-TASK-003 错误码体系扩展（v3 新增 12 个错误码）

- **任务标题**：扩展结构化错误码体系，新增 v3 全部错误码
- **任务描述**：在 `core/common/error_code.hpp` 中扩展错误码体系，新增 v3 全部 12 个错误码：`CFX-E-TOPO-TWOPOINT-DEGEN`（两点拓扑退化）、`CFX-E-HANDOFF-ILLEGAL-TRANS`（非法状态转移）、`CFX-E-HANDOFF-ILLEGAL-STATE`（非法状态调用）、`CFX-W-HANDOFF-REENTRY`（FSM 重入告警）、`CFX-W-HANDOFF-JITTER`（抖动告警）、`CFX-E-HANDOFF-JITTER-ESCALATE`（连续熔断升级）、`CFX-E-THREAD-OVER-LIMIT`（线程超限）、`CFX-E-THREAD-SHUTDOWN-TIMEOUT`（终止清理超时）、`CFX-E-HANDOFF-COMMIT-FAIL`（COMMIT 失败）、`CFX-E-HANDOFF-COMMIT-LOST`（COMMIT 丢失）、`CFX-W-HANDOFF-VOID-OWNER-RECOVERED`（void-owner 已修复）、`CFX-E-HANDOFF-SPLIT-BRAIN`（split-brain 检测）；错误码格式 `CFX-<level>-<module>-<reason>`，使用 `enum class ErrorLevel` / `enum class ErrorModule` / `enum class ErrorCode` 强类型枚举
- **依赖任务**：CF0-TASK-001
- **输入**：design.md §2.13.1 错误码体系、§2.5.6.7 错误码补充、§2.6.4 熔断器错误码
- **输出**：`core/common/error_code.hpp` 更新（含全部 v3 错误码）
- **验收标准**：
  1. 全部 12 个 v3 新增错误码定义完整，格式为 `CFX-<level>-<module>-<reason>`
  2. `ErrorLevel` / `ErrorModule` / `ErrorCode` 均为 `enum class`
  3. 错误码可转为结构化 JSON Lines 日志输出
  4. 无错误码重复定义
- **预估复杂度**：低
- **平台归属**：跨平台共享

### 1.4 CF0-TASK-004 JSON Lines 日志器与 std::expected 错误处理范式

- **任务标题**：实现 JSON Lines 结构化日志器与 C++20 std::expected 错误处理范式
- **任务描述**：在 `core/common/logger.hpp` 中实现 JSON Lines 结构化日志器（线程安全、异步 flush、支持 `std::source_location`），在 `core/common/cfx_error.hpp` 中定义 `CfxError` 结构体与 `std::expected<T, CfxError>` 错误处理范式；全部可失败操作返回 `std::expected<T, CfxError>`，热路径禁异常（无 throw/try/catch）；日志器支持 `ErrorLevel` / `ErrorModule` / `ErrorCode` 结构化字段
- **依赖任务**：CF0-TASK-001、CF0-TASK-003
- **输入**：design.md §2.13.2 错误处理范式、§2.2.2 存量日志器分析
- **输出**：`core/common/logger.hpp` 更新、`core/common/cfx_error.hpp` 新增
- **验收标准**：
  1. 日志器输出 JSON Lines 格式，含 timestamp/level/module/code/message 字段
  2. `CfxError` 含 ErrorLevel/ErrorModule/ErrorCode/message/trace_id 字段
  3. `std::expected<T, CfxError>` 作为可失败操作的标准返回类型
  4. 热路径代码无 throw/try/catch
- **预估复杂度**：中
- **平台归属**：跨平台共享

---

## 2. 领域模型与核心数据结构

> 本组任务定义平台无关的领域模型。v3 关键变更：CoordinateSpace 双语义（RelativeDelta + AbsolutePosition）、MouseMotionPayload 使用 std::variant、HandoffState 六态枚举。

### 2.1 CF0-TASK-005 核心领域对象与 CoordinateSpace 双语义

- **任务标题**：定义核心领域对象，含 CoordinateSpace 双语义与 std::variant 负载
- **任务描述**：在 `core/common/domain.hpp` 中定义核心领域对象：`NodeID`、`Endpoint`、`EndpointState`、`ScreenBoundary`、`EdgeDirection`、`NeighborState`、`LinkState`、`ModifierState`、`TraceId`；新增 v3 CoordinateSpace 双语义：`CoordinateSpace` enum class（`LocalScreen`）、`RelativeDelta` 结构体（int32_t deltaX/deltaY）、`AbsolutePosition` 结构体（uint32_t x/y + CoordinateSpace space）、`MouseMotionPayload` = `std::variant<RelativeDelta, AbsolutePosition>`；全部枚举使用 `enum class`，全部时间使用 `std::chrono::duration` 强类型
- **依赖任务**：CF0-TASK-001、CF0-TASK-003
- **输入**：design.md §2.3.2 模型实现、§2.10.0 Coordinate Space 设计、spec §6 数据约束
- **输出**：`core/common/domain.hpp` 更新
- **验收标准**：
  1. `RelativeDelta` / `AbsolutePosition` 结构体定义完整
  2. `MouseMotionPayload` 为 `std::variant<RelativeDelta, AbsolutePosition>`
  3. 全部枚举为 `enum class`，无裸 enum
  4. 全部时间字段为 `std::chrono::duration` 强类型，无裸整数时间
  5. `NodeID` 与 IP 解耦，IP 变化不改变 NodeID
- **预估复杂度**：中
- **平台归属**：跨平台共享

### 2.2 CF0-TASK-006 CanonicalInputEvent 规范化事件模型

- **任务标题**：定义 CanonicalInputEvent 规范化事件模型与 EventPayload variant
- **任务描述**：在 `core/s01_event/canonical_event.hpp` 中定义 `CanonicalInputEvent` 结构体，含 `EventType` enum class（MouseMove/MouseButton/MouseWheel/KeyDown/KeyUp）、`EventPayload` = `std::variant<MouseMotionPayload, MouseButtonPayload, MouseWheelPayload, KeyboardPayload>`、`Timestamp`（`std::chrono::steady_clock::time_point`）、`SequenceNumber`（`std::atomic<u64>` 或 uint64_t）、`SourceNodeId`（NodeID）；`MouseButtonPayload` 含 `MouseButton` enum + `isPressed`；`MouseWheelPayload` 含 `WheelAxis` enum + `delta`（RelativeDelta 语义）；`KeyboardPayload` 含 `keyCode` + `isPressed` + `ModifierState`
- **依赖任务**：CF0-TASK-005
- **输入**：design.md §2.3.2 模型实现、§2.10.0.1 坐标语义扩展、spec §5.1 规范输入事件模型、§6.1 数据约束
- **输出**：`core/s01_event/canonical_event.hpp` 新增
- **验收标准**：
  1. `CanonicalInputEvent` 为平台无关结构体，无任何平台头文件依赖
  2. `EventPayload` 为 `std::variant`，无裸 union
  3. 鼠标移动负载为 `MouseMotionPayload`（支持 RelativeDelta + AbsolutePosition）
  4. 滚轮负载使用 RelativeDelta 语义
  5. 序号单调递增，`std::atomic<u64>` 保证线程安全
- **预估复杂度**：中
- **平台归属**：跨平台共享

### 2.3 CF0-TASK-007 HandoffState 六态枚举与 HandoffContext

- **任务标题**：定义 HandoffState 六态枚举、HandoffContext 与 HandoffOutcome
- **任务描述**：在 `core/s03_handoff/handoff_types.hpp` 中定义 `HandoffState` enum class（六态：`ARMED`/`PENDING`/`ACK`/`ACTIVE`/`COOLDOWN`/`RECOVERY`）、`HandoffContext` 结构体（含 `TraceId`、`SourceNode`、`TargetNode`、`EdgeDirection`、`Overflow`、`EntryCoord`（AbsolutePosition）、`ModifierSnapshot`、`StartTime`）、`HandoffOutcome` = `std::variant<Success, Rejected, Timeout, Failed, Recovered>`、`RejectReason` enum class、`RollbackReason` enum class；定义各态不变量注释（ARMED: 主控态≤1 / PENDING: Source 持 LOCAL ownership / ACK: Source 持 LOCAL ownership / ACTIVE: 仅接受当前主控端输入 / COOLDOWN: ≥80ms / RECOVERY: 释放≤100ms）
- **依赖任务**：CF0-TASK-005
- **输入**：design.md §2.5.1 状态集合与不变量、§2.5.6.2 事务状态机映射、spec §5.3 切换状态机、§6.4 切换上下文
- **输出**：`core/s03_handoff/handoff_types.hpp` 新增
- **验收标准**：
  1. `HandoffState` 含且仅含六态（ARMED/PENDING/ACK/ACTIVE/COOLDOWN/RECOVERY）
  2. 各态不变量以注释或 concept 形式标注
  3. `HandoffOutcome` 为 `std::variant`，覆盖全部结果类型
  4. `HandoffContext` 含 TraceId/SourceNode/TargetNode/EdgeDirection/Overflow/EntryCoord/ModifierSnapshot/StartTime
- **预估复杂度**：中
- **平台归属**：跨平台共享

---

## 3. 跨平台抽象层 F

> 本组任务实现平台无关接口与平台特定适配器。v3 关键变更：坐标转换逻辑隔离在适配层，Core 层仅处理 RelativeDelta/AbsolutePosition。

### 3.1 CF0-TASK-008 平台抽象接口定义（IInputCapture / IInputInjector / IEventNormalizer）

- **任务标题**：定义平台抽象接口，对齐 C++20 设施（std::expected / std::span / concepts）
- **任务描述**：在 `platform/common/platform_ports.hpp` 中定义平台抽象接口：`IInputCapture`（`start(callback)` / `stop()` / `captureEnabled()`，返回 `std::expected`）、`IInputInjector`（`inject(event)` / `injectBatch(std::span<const CanonicalInputEvent>)` / `releaseAll()`，返回 `std::expected`）、`IEventNormalizer`（`normalize(rawEvent) -> std::expected<CanonicalInputEvent, CfxError>`）；回调参数使用 concepts 约束（`std::invocable<const RawInputEvent&>`）；接口签名使用 `std::span` 传批量数据，无裸指针+长度
- **依赖任务**：CF0-TASK-005、CF0-TASK-006
- **输入**：design.md §2.2.2.1-2.2.2.3 接口清单、§2.4.1 抽象层结构、§2.8.2.2 C++20 设施使用
- **输出**：`platform/common/platform_ports.hpp` 更新
- **验收标准**：
  1. 全部可失败操作返回 `std::expected<T, CfxError>`
  2. 批量传参使用 `std::span<const CanonicalInputEvent>`，无裸指针+长度、无 vector 拷贝
  3. 回调参数使用 concepts 约束
  4. 接口无平台头文件依赖（无 CGEvent、无 Windows.h）
- **预估复杂度**：中
- **平台归属**：跨平台共享

### 3.2 CF0-TASK-009 macOS 输入适配器实现

- **任务标题**：实现 macOS 用户态输入捕获/注入适配器
- **任务描述**：在 `platform/mac/mac_input_adapter.hpp/cpp` 中实现 `MacInputAdapter`（实现 `IInputCapture` + `IInputInjector` + `IEventNormalizer`）；捕获使用 CGEventTap（用户态，不装驱动）；鼠标移动捕获 `CGEventGetDoubleValueField(kCGMouseEventDeltaX/Y)` → `RelativeDelta`；绝对位置捕获 `CGEventGetLocation` → `AbsolutePosition`；注入使用 `CGEventPost`；修饰键状态使用 `CGEventGetFlags`；全部平台相关调用隔离在本文件内，Core 层不直接调用 CGEvent API
- **依赖任务**：CF0-TASK-008
- **输入**：design.md §2.4.2 平台实现选型、§2.12.1 macOS 用户态方案、§2.10.0.5 平台差异隔离
- **输出**：`platform/mac/mac_input_adapter.hpp`、`platform/mac/mac_input_adapter.cpp`
- **验收标准**：
  1. CGEventTap 注册成功，捕获回调 ≤1ms 返回
  2. 鼠标移动捕获为 `RelativeDelta`（deltaX/deltaY）
  3. 绝对位置捕获为 `AbsolutePosition`（本端屏幕坐标系）
  4. 注入使用 `CGEventPost`，相对运动与绝对位置均支持
  5. Core 层无 `CGEvent*` 直接引用（契约① 保障）
- **预估复杂度**：高
- **平台归属**：macOS

### 3.3 CF0-TASK-010 Windows 输入适配器实现

- **任务标题**：实现 Windows 用户态输入捕获/注入适配器
- **任务描述**：在 `platform/win/win_input_adapter.hpp/cpp` 中实现 `WinInputAdapter`（实现 `IInputCapture` + `IInputInjector` + `IEventNormalizer`）；捕获使用低级钩子 `SetWindowsHookEx(WH_MOUSE_LL/WH_KEYBOARD_LL)`（用户态，不装驱动）；鼠标移动捕获 `MSLLHOOKSTRUCT.pt` 差分（适配层维护上次位置）→ `RelativeDelta`；绝对位置 `MSLLHOOKSTRUCT.pt` → `AbsolutePosition`；注入使用 `SendInput`（相对运动 `MOUSEEVENTF_MOVE`，绝对位置 `MOUSEEVENTF_ABSOLUTE`）；全部平台相关调用隔离在本文件内
- **依赖任务**：CF0-TASK-008
- **输入**：design.md §2.4.2 平台实现选型、§2.12.2 Windows 用户态方案、§2.10.0.5 平台差异隔离
- **输出**：`platform/win/win_input_adapter.hpp`、`platform/win/win_input_adapter.cpp`
- **验收标准**：
  1. 低级钩子安装成功，捕获回调 ≤1ms 返回
  2. 鼠标移动差分正确生成 `RelativeDelta`
  3. 绝对位置正确生成 `AbsolutePosition`
  4. 注入使用 `SendInput`，相对运动与绝对位置均支持
  5. Core 层无 `MSLLHOOKSTRUCT` / `SendInput` 直接引用（契约① 保障）
- **预估复杂度**：高
- **平台归属**：Windows

### 3.4 CF0-TASK-011 平台坐标转换隔离与 DPI 归一化

- **任务标题**：实现平台坐标转换隔离与 DPI Scaling 归一化
- **任务描述**：在 `platform/mac/mac_coord.hpp` 与 `platform/win/win_coord.hpp` 中实现平台坐标转换与 DPI 归一化逻辑；macOS: Retina scaling 归一化（`CGDisplayBounds` 逻辑像素）；Windows: DPI Scaling 归一化（`GetDpiForWindow` / `SetProcessDpiAwareness`）；Core 层仅处理 `RelativeDelta` / `AbsolutePosition` 平台无关语义，平台差异在适配层消化；相对运动不受 DPI/分辨率/多屏排列影响
- **依赖任务**：CF0-TASK-009、CF0-TASK-010
- **输入**：design.md §2.10.0.5 平台差异隔离、§2.10.0.2 核心控制路径
- **输出**：`platform/mac/mac_coord.hpp`、`platform/win/win_coord.hpp`
- **验收标准**：
  1. macOS Retina scaling 归一化正确（逻辑像素）
  2. Windows DPI Scaling 归一化正确
  3. `RelativeDelta` 跨平台一致（macOS delta → Windows delta 注入，运动视觉连续）
  4. Core 层无平台坐标 API 直接引用（CF0-COORD-005）
- **预估复杂度**：中
- **平台归属**：跨平台共享

---

## 4. CF0-S01 规范输入事件模型

> 本组任务实现事件规范化与二进制协议编解码。v3 关键变更：MouseMotionPayload variant 编解码、std::span 零拷贝。

### 4.1 CF0-TASK-012 IEventNormalizer 实现与事件规范化

- **任务标题**：实现事件规范化器，将平台原始事件转为 CanonicalInputEvent
- **任务描述**：在 `core/s01_event/event_normalizer.hpp/cpp` 中实现 `EventNormalizer`（实现 `IEventNormalizer`）；将平台原始事件（macOS CGEvent / Windows MSLLHOOKSTRUCT）转为 `CanonicalInputEvent`；鼠标移动转为 `MouseMotionPayload`（`RelativeDelta` 用于核心控制路径）；修饰键状态快照；序号 `std::atomic<u64>` 单调递增；时间戳 `std::chrono::steady_clock::now()`；全部可失败操作返回 `std::expected`
- **依赖任务**：CF0-TASK-006、CF0-TASK-008
- **输入**：design.md §2.2.2.3 事件规范化接口、spec §5.1 规范输入事件模型
- **输出**：`core/s01_event/event_normalizer.hpp`、`core/s01_event/event_normalizer.cpp`
- **验收标准**：
  1. 平台原始事件正确转为 `CanonicalInputEvent`
  2. 鼠标移动负载为 `RelativeDelta`（核心控制路径）
  3. 序号单调递增，线程安全
  4. 时间戳为 `std::chrono::steady_clock::time_point`
  5. 返回 `std::expected<CanonicalInputEvent, CfxError>`
- **预估复杂度**：中
- **平台归属**：跨平台共享

### 4.2 CF0-TASK-013 FrameCodec 二进制协议编解码

- **任务标题**：实现 FrameCodec 二进制协议编解码，支持 MouseMotionPayload variant
- **任务描述**：在 `core/s01_event/frame_codec.hpp/cpp` 中实现 `FrameCodec`；将 `CanonicalInputEvent` 编码为二进制帧（紧凑格式：type(1B) + seq(8B) + timestamp(8B) + sourceNodeId(16B) + payload(variable)）；`MouseMotionPayload` variant 编解码（tag byte 区分 RelativeDelta/AbsolutePosition）；批量编解码使用 `std::span<const CanonicalInputEvent>` 输入、`std::span<std::byte>` 输出（零拷贝）；解码失败返回 `std::expected<CanonicalInputEvent, CfxError>`；`FrameType` enum class（InputEvent/HandoffRequest/HandoffAck/HandoffComplete/HandoffReject/Heartbeat/TopologySync）
- **依赖任务**：CF0-TASK-006
- **输入**：design.md §2.3.2 模型实现、spec §5.1.1 业务规则
- **输出**：`core/s01_event/frame_codec.hpp`、`core/s01_event/frame_codec.cpp`
- **验收标准**：
  1. 编解码往返一致（encode → decode == 原始事件）
  2. `MouseMotionPayload` variant 正确编解码（RelativeDelta + AbsolutePosition）
  3. 批量编解码使用 `std::span`，无 vector 拷贝
  4. 解码失败返回 `std::expected` 错误，不抛异常
  5. 帧格式紧凑，鼠标移动帧 ≤32 字节
- **预估复杂度**：中
- **平台归属**：跨平台共享

### 4.3 CF0-TASK-014 事件序号与去重机制

- **任务标题**：实现事件序号单调递增与去重机制
- **任务描述**：在 `core/s01_event/sequence_tracker.hpp/cpp` 中实现 `SequenceTracker`；维护 `std::atomic<uint64_t>` 单调递增序号；接收端去重（序号 ≤ 已处理序号 → 丢弃）；乱序检测（序号跳跃 → 告警）；序号回绕处理（uint64_t 空间足够，不需特殊处理但需注释说明）
- **依赖任务**：CF0-TASK-013
- **输入**：spec §5.1.1 业务规则
- **输出**：`core/s01_event/sequence_tracker.hpp`、`core/s01_event/sequence_tracker.cpp`
- **验收标准**：
  1. 序号单调递增，`std::atomic` 线程安全
  2. 重复序号正确丢弃
  3. 乱序跳跃正确告警
  4. `is_lock_free()` 为 true
- **预估复杂度**：低
- **平台归属**：跨平台共享

---

## 5. CF0-S02 端点身份与拓扑模型

> 本组任务实现端点身份与拓扑管理。v3 关键变更：两点退化拒绝 + CFX-E-TOPO-TWOPOINT-DEGEN、循环切换支持。

### 5.1 CF0-TASK-015 ITopologyManager 接口与实现

- **任务标题**：实现 ITopologyManager 拓扑管理器
- **任务描述**：在 `core/s02_topology/topology_manager.hpp/cpp` 中实现 `TopologyManager`（实现 `ITopologyManager`）；`loadTopology(config) -> std::expected<void, CfxError>` 加载端点清单与邻居关系；`neighbor(nodeId, direction) -> std::optional<NodeID>` 查询邻居（支持循环映射）；`isCircular() -> bool` 查询是否循环拓扑；`segmentCount() -> size_t` 查询端点数量；`addEndpoint` / `removeEndpoint` / `updateNeighbor` 运行时修改；校验 `segmentCount() >= 2`，两点退化拒绝并告警 `CFX-E-TOPO-TWOPOINT-DEGEN`
- **依赖任务**：CF0-TASK-005
- **输入**：design.md §2.2.2.4 拓扑管理接口、§2.5.5 循环切换实现、spec §5.2 端点身份与拓扑模型
- **输出**：`core/s02_topology/topology_manager.hpp`、`core/s02_topology/topology_manager.cpp`
- **验收标准**：
  1. 拓扑加载成功，邻居关系正确建立
  2. `segmentCount() < 2` 时返回 `CFX-E-TOPO-TWOPOINT-DEGEN` 错误
  3. 循环拓扑 `isCircular()` 返回 true，最左端左邻居 = 最右端
  4. 全部可失败操作返回 `std::expected`
  5. `neighbor()` 查询无死路（循环拓扑）
- **预估复杂度**：中
- **平台归属**：跨平台共享

### 5.2 CF0-TASK-016 拓扑配置加载与校验

- **任务标题**：实现拓扑配置加载与完整性校验
- **任务描述**：在 `core/s02_topology/topology_config.hpp/cpp` 中实现拓扑配置加载；从 JSON/TOML 配置文件加载端点清单（NodeID、ScreenBoundary、邻居关系）；校验配置完整性（每个端点最多两个邻居、邻居关系对称、屏幕边界合法）；配置格式错误返回 `std::expected` 错误；支持运行时热更新拓扑
- **依赖任务**：CF0-TASK-015
- **输入**：spec §5.2.1 业务规则、§5.2.2 交互流程
- **输出**：`core/s02_topology/topology_config.hpp`、`core/s02_topology/topology_config.cpp`
- **验收标准**：
  1. JSON/TOML 配置正确解析
  2. 配置校验覆盖：邻居数量≤2、邻居对称性、屏幕边界合法
  3. 配置错误返回结构化 `CfxError`
  4. 运行时热更新不中断正在进行的 Handoff
- **预估复杂度**：中
- **平台归属**：跨平台共享

### 5.3 CF0-TASK-017 邻居查询与循环映射

- **任务标题**：实现邻居查询与循环映射逻辑
- **任务描述**：在 `TopologyManager` 中实现 `neighbor()` 查询逻辑；线性序列拓扑：最左端左邻居 = 最右端，最右端右邻居 = 最左端（循环映射）；非循环拓扑：端点无对应方向邻居时返回 `std::nullopt`；`HandoffOrchestrator` 不感知回环，仅按 `neighbor()` 结果发起握手
- **依赖任务**：CF0-TASK-015
- **输入**：design.md §2.5.5 循环切换实现、spec §5.2.1.4 循环切换
- **输出**：`TopologyManager::neighbor()` 实现更新
- **验收标准**：
  1. 循环拓扑最右端右越界 → `neighbor(Right)` 返回最左端
  2. 非循环拓扑端点无邻居 → 返回 `std::nullopt`
  3. `HandoffOrchestrator` 不含回环特判逻辑
  4. 契约③ 保障：循环拓扑无死路
- **预估复杂度**：低
- **平台归属**：跨平台共享
---

## 6. CF0-S04 坐标空间与屏幕映射

> 本组任务实现坐标映射算法。v3 关键变更：RelativeDelta 核心控制路径 + AbsolutePosition 边缘 Handoff 路径、行为契约 CF0-COORD-001~005。

### 6.1 CF0-TASK-018 ICoordMapper 接口与越界量换算

- **任务标题**：实现 ICoordMapper 坐标映射器与越界量换算算法
- **任务描述**：在 `core/s04_coord/coord_mapper.hpp/cpp` 中实现 `CoordMapper`（实现 `ICoordMapper`）；`mapOverflow(overflow, sourceBoundary, targetBoundary, edgeDirection) -> std::expected<AbsolutePosition, CfxError>` 越界量换算为目标端入射坐标；右边缘越界：`entry.x = overflow`，`entry.y = y_s * H_t / H_s`（纵向按高度比例映射）；左边缘越界：`entry.x = W_t - overflow`，`entry.y` 同理；全部整数运算，无浮点（避免精度问题）；分辨率变更不影响 RelativeDelta 转发
- **依赖任务**：CF0-TASK-005
- **输入**：design.md §2.10.1 越界量换算算法、§2.10.3 整数运算约束、spec §5.4 坐标空间与屏幕映射
- **输出**：`core/s04_coord/coord_mapper.hpp`、`core/s04_coord/coord_mapper.cpp`
- **验收标准**：
  1. 越界量换算正确（右边缘/左边缘均覆盖）
  2. 全部整数运算，无浮点
  3. 纵向按高度比例映射正确
  4. 返回 `std::expected<AbsolutePosition, CfxError>`
  5. 分辨率变更不影响 RelativeDelta 转发（CF0-COORD-004）
- **预估复杂度**：中
- **平台归属**：跨平台共享

### 6.2 CF0-TASK-019 EdgeDetector 边缘越界检测

- **任务标题**：实现 EdgeDetector 边缘越界检测器
- **任务描述**：在 `core/s04_coord/edge_detector.hpp/cpp` 中实现 `EdgeDetector`；监听鼠标绝对位置（AbsolutePosition），判定是否越过屏幕边缘（`absoluteX > screenWidth` → 右边缘越界，`absoluteX < 0` → 左边缘越界）；越界时生成 `EdgeOverflowEvent`（含 EdgeDirection、Overflow 量、越界点纵向坐标）；查询 `TopologyManager.neighbor()` 判定是否有对应方向邻居，有邻居才触发 Handoff
- **依赖任务**：CF0-TASK-015、CF0-TASK-018
- **输入**：design.md §2.10.0.3 边缘 Handoff 路径、spec §5.4.1 业务规则
- **输出**：`core/s04_coord/edge_detector.hpp`、`core/s04_coord/edge_detector.cpp`
- **验收标准**：
  1. 鼠标越过右边缘 → 生成右越界事件
  2. 鼠标越过左边缘 → 生成左越界事件
  3. 无对应方向邻居时不触发 Handoff
  4. 越界量计算正确（光标超出边缘的像素距离）
- **预估复杂度**：中
- **平台归属**：跨平台共享

### 6.3 CF0-TASK-020 Coordinate Space 行为契约实现

- **任务标题**：实现 Coordinate Space 双语义行为契约（CF0-COORD-001~005）
- **任务描述**：确保核心控制路径使用 RelativeDelta（鼠标移动转发不传绝对坐标），边缘 Handoff 路径使用 AbsolutePosition（越界检测与入射定位）；ACTIVE 进入后一次性绝对定位光标至入射点，后续切换回 RelativeDelta；平台坐标转换逻辑隔离在适配层（Core 层无 CGEvent/MSLLHOOKSTRUCT 直接引用）；相对运动跨平台一致（macOS delta → Windows delta 注入，DPI Scaling 由适配层归一化）
- **依赖任务**：CF0-TASK-011、CF0-TASK-018、CF0-TASK-019
- **输入**：design.md §2.10.0 Coordinate Space 设计、§2.10.0.6 行为契约
- **输出**：`coord_mapper` / `edge_detector` / 适配层坐标逻辑更新
- **验收标准**：
  1. CF0-COORD-001: 核心控制路径使用 RelativeDelta，Input Plane 鼠标移动帧 payload 为 RelativeDelta
  2. CF0-COORD-002: 边缘 Handoff 使用 AbsolutePosition，ACTIVE 后切换回 RelativeDelta
  3. CF0-COORD-003: macOS 移动 100px → Windows 注入后光标移动 100px（DPI 归一化）
  4. CF0-COORD-004: 分辨率变更不影响相对运动转发
  5. CF0-COORD-005: Core 层无平台坐标 API 直接引用
- **预估复杂度**：中
- **平台归属**：跨平台共享

---

## 7. CF0-S05 低延迟传输协议

> 本组任务实现双平面传输层。v3 关键变更：coroutine 限 Transport 层、双平面隔离、TCP 可靠有序 + UDP 低延迟。

### 7.1 CF0-TASK-021 IControlPlaneChannel / IInputPlaneChannel 接口定义

- **任务标题**：定义 Control Plane 与 Input Plane 通道接口
- **任务描述**：在 `core/s05_transport/transport_ports.hpp` 中定义 `IControlPlaneChannel`（`send(msg) -> std::expected<void, CfxError>` / `setOnReceive(callback)` / `connect` / `disconnect` / `isConnected -> bool`）与 `IInputPlaneChannel`（`sendFrame(std::span<const std::byte>) -> std::expected<void, CfxError>` / `setOnFrame(callback)` / 最新优先丢帧策略）；ControlMessage = `std::variant<HandoffRequest, HandoffAck, HandoffComplete, HandoffReject, Heartbeat, TopologySync>`；接口签名使用 `std::span` + `std::expected`
- **依赖任务**：CF0-TASK-005、CF0-TASK-007
- **输入**：design.md §2.2.2.8-2.2.2.9 通道接口、§2.11.1 双平面隔离
- **输出**：`core/s05_transport/transport_ports.hpp` 新增
- **验收标准**：
  1. Control Plane 与 Input Plane 接口完全隔离（无共享状态）
  2. `ControlMessage` 为 `std::variant`，覆盖全部控制报文类型
  3. 批量传参使用 `std::span`
  4. 全部可失败操作返回 `std::expected`
- **预估复杂度**：中
- **平台归属**：跨平台共享

### 7.2 CF0-TASK-022 Control Plane TCP 可靠有序实现

- **任务标题**：实现 Control Plane TCP 可靠有序传输
- **任务描述**：在 `core/s05_transport/control_plane_channel.hpp/cpp` 中实现 `ControlPlaneChannel`（实现 `IControlPlaneChannel`）；基于 TCP + 序号/ACK/重传保证可靠有序；重传超限（5 次）判定断线；mutex 仅用于非热路径重传管理（持锁 ≤100us）；coroutine 异步收发（不阻塞 FSM/Capture 线程）；协议版本协商 + 配对门控
- **依赖任务**：CF0-TASK-021
- **输入**：design.md §2.11.2 Control Plane 可靠有序实现、§2.11.5 协议版本协商、§2.11.6 配对门控
- **输出**：`core/s05_transport/control_plane_channel.hpp`、`core/s05_transport/control_plane_channel.cpp`
- **验收标准**：
  1. TCP 连接建立成功，报文可靠有序送达
  2. 重传超限（5 次）判定断线并告警
  3. coroutine 异步收发，无阻塞 socket 调用
  4. mutex 持锁 ≤100us（非热路径）
  5. 协议版本协商 + 配对门控正确
- **预估复杂度**：高
- **平台归属**：跨平台共享

### 7.3 CF0-TASK-023 Input Plane UDP 低延迟队列实现

- **任务标题**：实现 Input Plane UDP 低延迟传输与最新优先丢帧
- **任务描述**：在 `core/s05_transport/input_plane_channel.hpp/cpp` 中实现 `InputPlaneChannel`（实现 `IInputPlaneChannel`）；基于 UDP + coroutine 异步收发；最新优先策略（队列满时丢弃最旧帧）；无锁队列连接 Input Plane 线程与 Injection 线程；首帧传输 ≤5ms；无阻塞 I/O、无锁竞争、无 sleep
- **依赖任务**：CF0-TASK-021
- **输入**：design.md §2.11.3 Input Plane 低延迟队列、§2.14.3 背压与丢帧策略
- **输出**：`core/s05_transport/input_plane_channel.hpp`、`core/s05_transport/input_plane_channel.cpp`
- **验收标准**：
  1. UDP 异步收发，无阻塞 socket
  2. 队列满时丢弃最旧帧（最新优先）
  3. 首帧传输 ≤5ms
  4. 无锁队列连接 Input Plane 与 Injection
  5. 热路径无锁竞争、无 sleep
- **预估复杂度**：高
- **平台归属**：跨平台共享

### 7.4 CF0-TASK-024 Coroutine 异步 I/O 封装（限 Transport 层）

- **任务标题**：实现 C++20 coroutine 异步 I/O 封装（仅限 Transport 层）
- **任务描述**：在 `core/s05_transport/coroutine_io.hpp` 中实现 coroutine awaitable 封装；macOS: 基于 kqueue/ev 的 coroutine awaitable；Windows: 基于 IOCP 的 coroutine awaitable；coroutine 调度由独立 Scheduler Thread 负责，不阻塞 FSM/Capture 线程；**FSM 层无 coroutine**（CF0-CPP20-001）；coroutine 不渗透到 Handoff FSM 状态转移
- **依赖任务**：CF0-TASK-022、CF0-TASK-023
- **输入**：design.md §2.7.4 coroutine 异步传输、§2.8.2.0 C++20 特性分级
- **输出**：`core/s05_transport/coroutine_io.hpp` 新增
- **验收标准**：
  1. macOS/Windows coroutine awaitable 实现正确
  2. coroutine 调度独立于 FSM/Capture 线程
  3. FSM 层源码无 `co_await` / `co_yield` / `co_return`（CF0-CPP20-001）
  4. Transport 层无阻塞 socket 调用
- **预估复杂度**：高
- **平台归属**：跨平台共享

### 7.5 CF0-TASK-025 心跳与断线判定 + 自动重连

- **任务标题**：实现心跳检测、断线判定与自动重连
- **任务描述**：在 `core/s05_transport/link_monitor.hpp/cpp` 中实现 `LinkMonitor`；周期性心跳发送（`std::chrono` 周期）；心跳超时判定断线（触发 LinkDown 事件）；断线后自动重连（指数退避：1s → 2s → 4s → 8s → 16s，上限 30s）；断线事件通知 FSM（ACTIVE → RECOVERY）；链路状态 `std::atomic<LinkState>` 快照供跨线程读
- **依赖任务**：CF0-TASK-022
- **输入**：design.md §2.11.4 心跳与断线判定、§2.11.7 自动重连
- **输出**：`core/s05_transport/link_monitor.hpp`、`core/s05_transport/link_monitor.cpp`
- **验收标准**：
  1. 心跳周期性发送，超时正确判定断线
  2. 自动重连指数退避正确（1s → 2s → 4s → 8s → 16s，上限 30s）
  3. 断线事件正确通知 FSM
  4. `std::atomic<LinkState>` `is_lock_free()` 为 true
- **预估复杂度**：中
- **平台归属**：跨平台共享

---

## 8. 防抖动机制（v3 新增）

> 本组任务实现防抖动四重契约。v3 关键变更：CooldownTimer + DwellTimeGuard + JitterDetector + JitterCircuitBreaker + FSM 不可重入守卫 + 行为契约 CF0-DEBOUNCE-001~005。

### 8.1 CF0-TASK-026 ICooldownTimer 接口与 CooldownTimer 实现

- **任务标题**：实现 CooldownTimer 冷却期定时器（第一道防线）
- **任务描述**：在 `core/s03_handoff/cooldown_timer.hpp/cpp` 中定义 `ICooldownTimer` 接口并实现 `CooldownTimer`；`start(duration)` 启动计时（基于 `std::chrono::steady_clock`）；`remaining() -> std::chrono::milliseconds` 查询剩余时间；`onExpired` 回调（FSM 执行 COOLDOWN → ARMED 转移）；默认冷却期 80ms，熔断冷却期 300ms；冷却期内越界事件丢弃 + 钳制鼠标回边缘内侧（通过 `IInputInjector`）
- **依赖任务**：CF0-TASK-007
- **输入**：design.md §2.6.1 冷却期定时器、spec §4.2.6 / §5.3.1.11
- **输出**：`core/s03_handoff/cooldown_timer.hpp`、`core/s03_handoff/cooldown_timer.cpp`
- **验收标准**：
  1. `start(80ms)` 后 `remaining()` 正确递减
  2. 到期触发 `onExpired` 回调
  3. 冷却期内越界事件被丢弃
  4. 钳制鼠标回边缘内侧正确
  5. 熔断时 `start(300ms)` 提升冷却期
- **预估复杂度**：中
- **平台归属**：跨平台共享

### 8.2 CF0-TASK-027 IDwellTimeGuard 接口与 DwellTimeGuard 实现

- **任务标题**：实现 DwellTimeGuard 最小停留时间检测（第二道防线）
- **任务描述**：在 `core/s03_handoff/dwell_time_guard.hpp/cpp` 中定义 `IDwellTimeGuard` 接口并实现 `DwellTimeGuard`；`start(50ms)` 启动计时（FSM 进入 ACTIVE 时）；`onCursorPosition(pos, boundary)` 更新位置（鼠标离开屏幕 → 重置计时）；`reached() -> bool` 查询是否达标；未达标时越界事件被忽略（不发起 Handoff，不钳制鼠标）；达标后放行至 EdgeDetector
- **依赖任务**：CF0-TASK-007
- **输入**：design.md §2.6.2 最小停留时间检测、spec §4.2.7 / §5.3.1.12
- **输出**：`core/s03_handoff/dwell_time_guard.hpp`、`core/s03_handoff/dwell_time_guard.cpp`
- **验收标准**：
  1. `start(50ms)` 后 `reached()` 在 50ms 后返回 true
  2. 鼠标离开屏幕 → 重置计时
  3. 未达标时越界事件被忽略
  4. 达标后放行至 EdgeDetector
- **预估复杂度**：中
- **平台归属**：跨平台共享

### 8.3 CF0-TASK-028 JitterDetector 抖动频率检测器实现

- **任务标题**：实现 JitterDetector 抖动频率检测器（第三道防线）
- **任务描述**：在 `core/s03_handoff/jitter_detector.hpp/cpp` 中实现 `JitterDetector`；维护以 `EndpointPair`（规范化 NodeId 对，小者为 a）为键的滑动窗口计数器；环形缓冲，1 秒窗口，每次 Handoff 完成时递增计数并淘汰过期记录；窗口内计数 >5 时触发熔断信号通知 `JitterCircuitBreaker`
- **依赖任务**：CF0-TASK-007
- **输入**：design.md §2.6.3 抖动频率检测器、spec §4.1.8 / §5.3.1.13
- **输出**：`core/s03_handoff/jitter_detector.hpp`、`core/s03_handoff/jitter_detector.cpp`
- **验收标准**：
  1. 滑动窗口 1 秒，计数正确
  2. `EndpointPair` 规范化（小者为 a）
  3. 窗口内计数 >5 时触发熔断信号
  4. 过期记录正确淘汰
- **预估复杂度**：中
- **平台归属**：跨平台共享

### 8.4 CF0-TASK-029 IJitterCircuitBreaker 接口与 JitterCircuitBreaker 实现

- **任务标题**：实现 JitterCircuitBreaker 抖动熔断器（第四道防线）
- **任务描述**：在 `core/s03_handoff/jitter_circuit_breaker.hpp/cpp` 中定义 `IJitterCircuitBreaker` 接口并实现 `JitterCircuitBreaker`；维护 `EndpointPair` 为键的熔断状态机：`Normal → Tripped → Escalated`；收到熔断信号时：状态转 `Tripped`，调用 `CooldownTimer.start(300ms)`，记录告警 `CFX-W-HANDOFF-JITTER`，递增 `consecutiveTrips`；连续 3 次熔断：状态转 `Escalated`，记录告警 `CFX-E-HANDOFF-JITTER-ESCALATE`，向运维告警；冷却期正常结束后 `consecutiveTrips` 清零
- **依赖任务**：CF0-TASK-026、CF0-TASK-028
- **输入**：design.md §2.6.4 抖动熔断器、spec §4.2.8 / §5.3.1.13
- **输出**：`core/s03_handoff/jitter_circuit_breaker.hpp`、`core/s03_handoff/jitter_circuit_breaker.cpp`
- **验收标准**：
  1. 熔断状态机 `Normal → Tripped → Escalated` 正确
  2. 熔断时冷却期提升至 300ms
  3. 连续 3 次熔断触发 `CFX-E-HANDOFF-JITTER-ESCALATE` 告警
  4. 冷却期正常结束后 `consecutiveTrips` 清零
- **预估复杂度**：中
- **平台归属**：跨平台共享

### 8.5 CF0-TASK-030 FSM 不可重入守卫

- **任务标题**：实现 FSM 不可重入守卫与待处理事件队列
- **任务描述**：在 `HandoffFsm.submit(event)` 入口实现不可重入守卫；当前态为 ARMED/ACTIVE → 正常处理；当前态为 PENDING/ACK/COOLDOWN/RECOVERY → 推入待处理队列（容量 16）；队列满时丢弃事件并告警 `CFX-W-HANDOFF-REENTRY`；FSM 转回 ARMED/ACTIVE 时消费待处理队列中的事件
- **依赖任务**：CF0-TASK-007
- **输入**：design.md §2.6.5 FSM 不可重入守卫、spec §4.2.10 / §5.3.1.18
- **输出**：`HandoffFsm` 不可重入守卫实现
- **验收标准**：
  1. PENDING/ACK/COOLDOWN/RECOVERY 期间新事件排队
  2. 队列容量 16，满时丢弃 + 告警 `CFX-W-HANDOFF-REENTRY`
  3. FSM 转回 ARMED/ACTIVE 时消费队列
  4. 禁止重入打断进行中的转移
- **预估复杂度**：中
- **平台归属**：跨平台共享

### 8.6 CF0-TASK-031 防抖动行为契约验证准备

- **任务标题**：准备防抖动行为契约 CF0-DEBOUNCE-001~005 的可验证接口
- **任务描述**：确保防抖动四重契约协同工作，暴露可观察证据接口供集成测试验证；FSM 状态序列可查询（COOLDOWN 态不直接转 PENDING）；Handoff 次数可统计（1 秒内 ≤5 次）；ACTIVE 进入时间戳与下一次 PENDING 时间戳可比较（≥50ms）；COOLDOWN 进入时间戳与 ARMED 恢复时间戳可比较（≥80ms）；熔断后 COOLDOWN 时长可查询（≥300ms）
- **依赖任务**：CF0-TASK-026、CF0-TASK-027、CF0-TASK-028、CF0-TASK-029、CF0-TASK-030
- **输入**：design.md §2.6.0 防抖动行为契约、§2.6.6 四重契约协同时序
- **输出**：防抖动组件可观察证据接口
- **验收标准**：
  1. CF0-DEBOUNCE-001: COOLDOWN 期间 0 次 Handoff 触发可验证
  2. CF0-DEBOUNCE-002: 1 秒内 Handoff 次数 ≤5 可验证
  3. CF0-DEBOUNCE-003: ACTIVE 后 ≥50ms 才允许越界可验证
  4. CF0-DEBOUNCE-004: COOLDOWN ≥80ms 可验证
  5. CF0-DEBOUNCE-005: 熔断后 ≥300ms + 连续 3 次告警可验证
- **预估复杂度**：中
- **平台归属**：跨平台共享

---

## 9. 并发与线程安全模型（v3 新增）

> 本组任务实现 8 线程架构与无锁队列。v3 关键变更：std::jthread + 无锁 SPSC/MPMC + 原子状态快照 + 终止清理 ≤200ms。

### 9.1 CF0-TASK-032 IThreadModel 接口与 8 线程架构

- **任务标题**：实现 IThreadModel 接口与 8 线程架构
- **任务描述**：在 `core/common/thread_model.hpp/cpp` 中定义 `IThreadModel` 接口并实现 `ThreadModel`；8 线程均用 `std::jthread`（自动 join + 可中断）：Main、Capture、Injection、InputPlane、ControlPlane、FSM、Logger、Scheduler；`start() -> std::expected<void, CfxError>` 启动全部线程；`requestStop()` 通过 `std::jthread::request_stop()` 通知所有线程；`joinAll(timeout) -> std::expected<void, CfxError>` 等待退出；线程数上界 ≤8，超限告警 `CFX-E-THREAD-OVER-LIMIT`；`ThreadRole` enum class 标识各线程
- **依赖任务**：CF0-TASK-001
- **输入**：design.md §2.7.1-2.7.2 8 线程架构、§2.2.2.11 并发模型接口、spec §4.6 并发与线程安全
- **输出**：`core/common/thread_model.hpp`、`core/common/thread_model.cpp`
- **验收标准**：
  1. 8 线程均用 `std::jthread`，无裸 `std::thread`
  2. 线程数 ≤8，超限告警 `CFX-E-THREAD-OVER-LIMIT`
  3. `requestStop()` 正确通知所有线程
  4. `joinAll()` 等待退出，超时告警 `CFX-E-THREAD-SHUTDOWN-TIMEOUT`
  5. `std::jthread` 析构自动 join，无泄漏
- **预估复杂度**：高
- **平台归属**：跨平台共享

### 9.2 CF0-TASK-033 无锁 SPSC 队列实现

- **任务标题**：实现无锁 SPSC（单生产者单消费者）环形队列
- **任务描述**：在 `core/common/spsc_queue.hpp` 中实现 `SpscQueue<T, Capacity>`；环形缓冲 + `std::atomic<size_t>` head/tail（无锁无竞争）；`push(item) -> bool`（满时返回 false）；`pop() -> std::optional<T>`；`empty() / full() / size()`；默认容量 256；`is_lock_free()` 保证；用途：Capture→FSM、FSM→Injection、FSM→Input Plane、FSM→Control Plane、Input Plane→Injection、Control Plane→FSM
- **依赖任务**：CF0-TASK-001
- **输入**：design.md §2.7.3 无锁队列与原子状态
- **输出**：`core/common/spsc_queue.hpp` 新增
- **验收标准**：
  1. 无锁无竞争，`std::atomic` head/tail
  2. `push` / `pop` 正确，满时返回 false
  3. 容量固定 256，无动态分配
  4. `is_lock_free()` 为 true
  5. 单生产者单消费者场景无竞争
- **预估复杂度**：中
- **平台归属**：跨平台共享

### 9.3 CF0-TASK-034 无锁 MPMC 队列实现（日志专用）

- **任务标题**：实现无锁 MPMC（多生产者单消费者）分片队列
- **任务描述**：在 `core/common/mpmc_queue.hpp` 中实现 `MpmcQueue<T, Capacity>`；分片 SPSC 队列（按线程数分片）+ `std::atomic` 索引；所有线程 → Logger Thread；分片降低竞争；溢出丢弃并计数
- **依赖任务**：CF0-TASK-033
- **输入**：design.md §2.7.3 无锁队列
- **输出**：`core/common/mpmc_queue.hpp` 新增
- **验收标准**：
  1. 分片 SPSC + `std::atomic` 索引
  2. 多生产者无竞争（分片隔离）
  3. 溢出丢弃并计数
  4. `is_lock_free()` 为 true
- **预估复杂度**：中
- **平台归属**：跨平台共享

### 9.4 CF0-TASK-035 原子状态快照实现

- **任务标题**：实现跨线程原子状态快照
- **任务描述**：在 `core/common/atomic_state.hpp` 中实现原子状态快照；`std::atomic<HandoffState>` FSM 状态快照（FSM 线程写，其他线程读）；`std::atomic<LinkState>` 链路状态（Control Plane 线程写，其他线程读）；`std::atomic<ModifierState>` 修饰键状态（Capture 线程写，其他线程读）；全部 `is_lock_free() == true`（编译期 `static_assert`）
- **依赖任务**：CF0-TASK-007
- **输入**：design.md §2.7.3 原子状态快照、spec §8.2.8
- **输出**：`core/common/atomic_state.hpp` 新增
- **验收标准**：
  1. `std::atomic<HandoffState/LinkState/ModifierState>` 全部 `is_lock_free()` 为 true
  2. 编译期 `static_assert` 验证
  3. 写线程与读线程正确分离
- **预估复杂度**：低
- **平台归属**：跨平台共享

### 9.5 CF0-TASK-036 终止与清理契约实现

- **任务标题**：实现进程终止与清理契约（≤200ms）
- **任务描述**：在 `ThreadModel` 中实现终止清理逻辑；Main Thread 收到 SIGTERM/SIGINT → `requestStop()`；各线程检查 stop_token 并执行清理：Capture 释放 CGEventTap/低级钩子、Injection flush 待注入队列、Input/Control Plane 关闭 socket + 取消 coroutine、FSM 若 ACTIVE/PENDING/ACK 转 RECOVERY 释放键鼠、Logger flush 队列、Scheduler 取消 coroutine 任务；`joinAll(200ms)` 等待退出，超时告警 `CFX-E-THREAD-SHUTDOWN-TIMEOUT`
- **依赖任务**：CF0-TASK-032
- **输入**：design.md §2.7.6 终止与清理契约、spec §4.6.9
- **输出**：`ThreadModel` 终止清理实现更新
- **验收标准**：
  1. 全部线程 ≤200ms 完成清理并退出
  2. SIGTERM/SIGINT 正确触发 `requestStop()`
  3. FSM 若处于 ACTIVE/PENDING/ACK 转 RECOVERY 释放键鼠
  4. 超时告警 `CFX-E-THREAD-SHUTDOWN-TIMEOUT`
  5. 无线程泄漏或悬挂
- **预估复杂度**：中
- **平台归属**：跨平台共享

---

## 10. CF0-S03 Handoff 六态状态机

> 本组任务实现 Handoff FSM 核心逻辑。v3 关键变更：六态 + 12 条转移路径 + ACK 事务语义 + 双向冲突裁决 + 单线程所有权。

### 10.1 CF0-TASK-037 IHandoffFsm 接口定义

- **任务标题**：定义 IHandoffFsm 接口（六态 + 12 条转移 + 单线程所有权）
- **任务描述**：在 `core/s03_handoff/i_handoff_fsm.hpp` 中定义 `IHandoffFsm` 接口；`submit(event)` 提交事件（经无锁 SPSC 队列，非 FSM 线程调用）；`currentState() -> HandoffState` 查询当前态（`std::atomic` 快照读）；`onTransition(callback)` 状态转移回调；`FsmEvent` = `std::variant<EdgeOverflow, HandoffAck, HandoffReject, HandoffComplete, HandoffFail, HandoffTimeout, AckTimeout, InvalidAck, ReleaseRequest, CooldownExpired, RecoveryDone, LinkDown, Exception, TopologyMismatch>`；接口注释标注单线程所有权语义
- **依赖任务**：CF0-TASK-007、CF0-TASK-033
- **输入**：design.md §2.2.2.5 Handoff FSM 接口、§2.5.3 FSM 单线程所有权
- **输出**：`core/s03_handoff/i_handoff_fsm.hpp` 新增
- **验收标准**：
  1. `HandoffState` 六态完整
  2. `FsmEvent` variant 覆盖全部 12 条转移路径的事件
  3. `submit` 经无锁 SPSC 队列，非 FSM 线程可安全调用
  4. `currentState` 经 `std::atomic` 快照读
- **预估复杂度**：中
- **平台归属**：跨平台共享

### 10.2 CF0-TASK-038 HandoffFsm 实现与 12 条状态转移

- **任务标题**：实现 HandoffFsm 六态状态机与 12 条转移路径
- **任务描述**：在 `core/s03_handoff/handoff_fsm.hpp/cpp` 中实现 `HandoffFsm`（实现 `IHandoffFsm`）；FSM 实例由 FSM 专属 `std::jthread` 独占；其他线程经 `submit(event)` 推入无锁 SPSC 队列；FSM 线程循环 pop 事件并串行处理；12 条转移路径：(1) ARMED+EdgeOverflow→PENDING (2) PENDING+HandoffAck→ACK (3) PENDING+Reject/LinkDown→COOLDOWN (4) PENDING+Timeout→RECOVERY (5) ACK+Complete→ACTIVE (6) ACK+Fail→COOLDOWN (7) ACK+AckTimeout/InvalidAck→RECOVERY (8) ACTIVE+Release→COOLDOWN (9) ACTIVE+LinkDown→RECOVERY (10) COOLDOWN+Expired→ARMED (11) RECOVERY+Done→ARMED (12) 任意态+LinkDown/Exception/TopologyMismatch→RECOVERY；非法转移 → 告警 `CFX-E-HANDOFF-ILLEGAL-TRANS`，FSM 保持原态；转移路径内禁 I/O、禁锁等待、禁异常（≤1ms）
- **依赖任务**：CF0-TASK-037、CF0-TASK-030、CF0-TASK-035
- **输入**：design.md §2.5.1-2.5.3 FSM 设计、§2.5.2 状态转移表
- **输出**：`core/s03_handoff/handoff_fsm.hpp`、`core/s03_handoff/handoff_fsm.cpp`
- **验收标准**：
  1. 六态 12 条转移路径全部实现
  2. 非法转移告警 `CFX-E-HANDOFF-ILLEGAL-TRANS` 且 FSM 保持原态
  3. 禁止跳过 COOLDOWN（PENDING→ARMED、ACK→ARMED、ACTIVE→ARMED 均非法）
  4. 禁止跳过 RECOVERY（PENDING timeout→ARMED、ACK invalid→ARMED、ACTIVE disconnect→ARMED 均非法）
  5. FSM 单线程所有权，状态转移无锁 ≤1ms
  6. 转移路径内无 I/O、无锁等待、无异常
- **预估复杂度**：高
- **平台归属**：跨平台共享

### 10.3 CF0-TASK-039 ACK 事务语义实现（PREPARE → ACK → COMMIT → ACTIVE）

- **任务标题**：实现 ACK 事务语义与 commitOwnership/onCommitReceived 接口
- **任务描述**：在 `IHandoffOrchestrator` 中新增 v3 COMMIT 阶段接口：`commitOwnership(traceId) -> std::expected<CommitResult, CfxError>`（Source 端 COMMIT，仅在 ACK 态可调用，调用后 Source 释放 LOCAL ownership，发送 HandoffComplete）、`onCommitReceived(traceId) -> std::expected<void, CfxError>`（Target 端接收 COMMIT，仅在 ACK 态可调用，进入 ACTIVE）；其他态调用返回 `CFX-E-HANDOFF-ILLEGAL-STATE`；COMMIT 后不可回滚（仅 disconnect → RECOVERY 释放）；COMMIT 失败告警 `CFX-E-HANDOFF-COMMIT-FAIL`，COMMIT 丢失告警 `CFX-E-HANDOFF-COMMIT-LOST`，void-owner 修复后告警 `CFX-W-HANDOFF-VOID-OWNER-RECOVERED`
- **依赖任务**：CF0-TASK-038
- **输入**：design.md §2.5.6 ACK 事务语义设计、§2.5.6.6 接口契约补充
- **输出**：`IHandoffOrchestrator` COMMIT 接口新增、`handoff_fsm` 事务逻辑实现
- **验收标准**：
  1. PREPARE→ACK→COMMIT→ACTIVE 事务模型正确
  2. COMMIT 前 Source 恒持 LOCAL ownership
  3. `commitOwnership` 仅在 ACK 态可调用，其他态返回 `CFX-E-HANDOFF-ILLEGAL-STATE`
  4. COMMIT 后不可回滚
  5. COMMIT 丢失触发 void-owner 修复路径
  6. 错误码 `CFX-E-HANDOFF-COMMIT-FAIL` / `CFX-E-HANDOFF-COMMIT-LOST` / `CFX-W-HANDOFF-VOID-OWNER-RECOVERED` 正确使用
- **预估复杂度**：高
- **平台归属**：跨平台共享

### 10.4 CF0-TASK-040 IHandoffOrchestrator 编排实现

- **任务标题**：实现 IHandoffOrchestrator Handoff 编排器
- **任务描述**：在 `core/s03_handoff/handoff_orchestrator.hpp/cpp` 中实现 `HandoffOrchestrator`（实现 `IHandoffOrchestrator`）；`initiate(edgeOverflow) -> std::expected<TraceId, CfxError>` 发起 Handoff（ARMED→PENDING，生成 TraceId，发送 HandoffRequest，启动 20ms 超时）；`onAckReceived` / `onRejectReceived` / `onCompleteReceived` 处理对端响应；协调 `TopologyManager.neighbor()` 查询目标端；协调 `CoordMapper.mapOverflow()` 换算入射坐标；协调 `CooldownTimer` / `DwellTimeGuard` / `JitterDetector` / `JitterCircuitBreaker` 四重契约
- **依赖任务**：CF0-TASK-038、CF0-TASK-039、CF0-TASK-015、CF0-TASK-018
- **输入**：design.md §2.2.2.6 Handoff 编排接口、§2.5.4 握手时序
- **输出**：`core/s03_handoff/handoff_orchestrator.hpp`、`core/s03_handoff/handoff_orchestrator.cpp`
- **验收标准**：
  1. `initiate` 正确生成 TraceId 并发送 HandoffRequest
  2. 20ms 超时正确启动
  3. 对端响应正确处理（Ack/Reject/Complete）
  4. 四重防抖动契约正确协调
  5. 拓扑邻居查询与坐标换算正确
- **预估复杂度**：高
- **平台归属**：跨平台共享

### 10.5 CF0-TASK-041 双向并发冲突裁决

- **任务标题**：实现双向并发 Handoff 冲突裁决（TraceId 较小者胜）
- **任务描述**：在 `HandoffOrchestrator` 中实现双向并发冲突裁决；A→B 与 B→A 同时发起时，TraceId 较小者胜；败方在 PREPARE 阶段回滚（PENDING → COOLDOWN），未进入 COMMIT，ownership 仍由胜方持有；胜方正常完成 Handoff；无 split-brain
- **依赖任务**：CF0-TASK-040
- **输入**：design.md §2.1.3.4 双向同时触发裁决流程、§2.5.6.4 Split-Brain 抑制证明
- **输出**：`HandoffOrchestrator` 冲突裁决实现
- **验收标准**：
  1. 双向并发时 TraceId 较小者胜
  2. 败方回滚至 COOLDOWN，未进入 COMMIT
  3. 胜方正常完成 Handoff
  4. 裁决后拓扑中 Control Owner 数量 = 1（无 split-brain）
- **预估复杂度**：中
- **平台归属**：跨平台共享

### 10.6 CF0-TASK-042 握手时序与延迟预算保障

- **任务标题**：实现握手时序与端到端延迟预算保障（≤30ms）
- **任务描述**：确保 Handoff 端到端时序满足 DFX 红线 ≤30ms；预算分配：边缘检测 ≤1ms + 网络传输 ≤5ms + 目标校验 ≤1ms + 网络传输 ≤5ms + 源端准备 ≤1ms + 首帧传输 ≤5ms + 注入启动 ≤1ms + COOLDOWN 启动 ≤1ms = 20ms（余量 10ms）；握手超时阈值 20ms；FSM 转移延迟 ≤1ms；冷却期 80ms；最小停留 50ms
- **依赖任务**：CF0-TASK-040
- **输入**：design.md §2.5.4 握手时序与延迟预算、spec §4.1 性能
- **输出**：时序保障实现与验证
- **验收标准**：
  1. 端到端 Handoff ≤30ms（DFX 红线）
  2. FSM 转移延迟 ≤1ms
  3. 握手超时阈值 20ms
  4. 冷却期 ≥80ms，最小停留 ≥50ms
- **预估复杂度**：中
- **平台归属**：跨平台共享
---

## 11. C++20 技术栈使用方案（v3 新增）

> 本组任务实现 C++20 特性分级与禁止项保障。v3 关键变更：MUST/SHOULD/OPTIONAL/FORBIDDEN 四级分级、coroutine 限 Transport 层、CI 矩阵。

### 11.1 CF0-TASK-043 C++20 特性分级强制保障

- **任务标题**：实现 C++20 特性分级 MUST/SHOULD/OPTIONAL/FORBIDDEN 保障
- **任务描述**：创建 C++20 特性分级检查脚本与 CI 配置；MUST 级特性使用证据检查（`std::expected` / `std::span` / `std::chrono` / `std::atomic` / `std::jthread` 在代码中有使用）；SHOULD 级特性推荐使用但不强制（coroutine 限 Transport 层、`std::variant` + `std::visit`、concepts）；FORBIDDEN 级特性静态审查（复杂模板元编程、协程状态机、提案特性、编译器扩展）；CF0-CPP20-001~006 验收约束
- **依赖任务**：CF0-TASK-001、CF0-TASK-002
- **输入**：design.md §2.8.2.0 C++20 特性分级、§2.8.2.1 分级关键约束、§2.8.3 禁止项保障
- **输出**：`cmake/cpp20_check.cmake`、`.github/workflows/cpp20_audit.yml`（或等效 CI 配置）
- **验收标准**：
  1. CF0-CPP20-001: FSM 层源码无 `co_await` / `co_yield` / `co_return`（grep 检查）
  2. CF0-CPP20-002: FSM 单元测试可纯同步驱动，无 coroutine 调度器依赖
  3. CF0-CPP20-005: MUST 级特性使用证据存在（CI 检查）
  4. CF0-CPP20-006: FORBIDDEN 级特性未出现（CI 静态审查 + 模板深度检查）
  5. `CXX_STANDARD` 严格为 20，禁降级
- **预估复杂度**：中
- **平台归属**：跨平台共享

### 11.2 CF0-TASK-044 CI 编译器矩阵配置

- **任务标题**：配置 CI 编译器矩阵（Apple Clang≥15 / MSVC≥19.3 / Clang≥17）
- **任务描述**：创建 CI 矩阵配置文件；macOS: Apple Clang 15+（Xcode 15+）；Windows: MSVC 19.3x+（VS 2022）+ Clang 17+；全组合编译 + 测试通过；CI 检查 `CXX_STANDARD` 严格为 20
- **依赖任务**：CF0-TASK-043
- **输入**：design.md §2.8.1 编译器矩阵、§2.8.4 CMake 升级
- **输出**：`.github/workflows/matrix.yml`（或等效 CI 配置）
- **验收标准**：
  1. macOS Apple Clang 15+ 编译 + 测试通过
  2. Windows MSVC 19.3x+ 编译 + 测试通过
  3. Windows Clang 17+ 编译 + 测试通过
  4. CI 检查 `CXX_STANDARD` 严格为 20
- **预估复杂度**：低
- **平台归属**：跨平台共享

---

## 12. 单元测试

> 本组任务实现核心模块单元测试。v3 关键变更：六态 FSM 12 条转移测试、防抖动四重契约测试、并发模型测试。

### 12.1 CF0-TASK-045 领域模型单元测试

- **任务标题**：编写核心领域对象单元测试
- **任务描述**：在 `tests/test_domain.cpp` 中编写领域模型单元测试；`CanonicalInputEvent` 创建/序列化/反序列化；`MouseMotionPayload` variant（RelativeDelta + AbsolutePosition）正确性；`HandoffState` 六态枚举；`NodeID` 唯一性与 IP 解耦；`ScreenBoundary` 合法性校验；`TraceId` 生成唯一性
- **依赖任务**：CF0-TASK-005、CF0-TASK-006、CF0-TASK-007
- **输入**：spec §6 数据约束
- **输出**：`tests/test_domain.cpp` 新增
- **验收标准**：
  1. 全部领域对象创建/序列化/反序列化正确
  2. `MouseMotionPayload` variant 两种语义均覆盖
  3. `HandoffState` 六态完整
  4. `NodeID` 与 IP 解耦验证
- **预估复杂度**：中
- **平台归属**：跨平台共享

### 12.2 CF0-TASK-046 Handoff FSM 单元测试（六态 12 条转移）

- **任务标题**：编写 Handoff FSM 六态状态机单元测试
- **任务描述**：在 `tests/test_handoff_fsm.cpp` 中编写 FSM 单元测试；12 条合法转移路径全部覆盖；非法转移测试（PENDING→ARMED、ACK→ARMED、ACTIVE→ARMED、PENDING timeout→ARMED、ACK invalid→ARMED、ACTIVE disconnect→ARMED 均告警 `CFX-E-HANDOFF-ILLEGAL-TRANS` 且保持原态）；FSM 单线程所有权验证（纯同步驱动，无 coroutine 依赖）；状态不变量验证（各态进入条件与守卫）
- **依赖任务**：CF0-TASK-038
- **输入**：design.md §2.5.1-2.5.2 FSM 设计
- **输出**：`tests/test_handoff_fsm.cpp` 新增
- **验收标准**：
  1. 12 条合法转移路径全部测试通过
  2. 非法转移正确告警且保持原态
  3. FSM 可纯同步驱动（CF0-CPP20-002）
  4. 各态不变量验证通过
- **预估复杂度**：高
- **平台归属**：跨平台共享

### 12.3 CF0-TASK-047 防抖动机制单元测试

- **任务标题**：编写防抖动四重契约单元测试
- **任务描述**：在 `tests/test_debounce.cpp` 中编写防抖动单元测试；CooldownTimer: 80ms/300ms 计时正确、到期回调触发；DwellTimeGuard: 50ms 计时正确、鼠标离开重置；JitterDetector: 1 秒滑动窗口计数、>5 触发熔断信号；JitterCircuitBreaker: Normal→Tripped→Escalated 状态机、连续 3 次告警；FSM 不可重入守卫: 队列容量 16、满时丢弃+告警
- **依赖任务**：CF0-TASK-026、CF0-TASK-027、CF0-TASK-028、CF0-TASK-029、CF0-TASK-030
- **输入**：design.md §2.6 防抖动机制详细设计
- **输出**：`tests/test_debounce.cpp` 新增
- **验收标准**：
  1. CooldownTimer 80ms/300ms 计时正确
  2. DwellTimeGuard 50ms 计时与重置正确
  3. JitterDetector 滑动窗口与熔断信号正确
  4. JitterCircuitBreaker 状态机与连续告警正确
  5. FSM 不可重入守卫队列与丢弃正确
- **预估复杂度**：中
- **平台归属**：跨平台共享

### 12.4 CF0-TASK-048 坐标映射单元测试

- **任务标题**：编写坐标映射与边缘检测单元测试
- **任务描述**：在 `tests/test_coord.cpp` 中编写坐标映射单元测试；越界量换算（右边缘/左边缘）、纵向比例映射、整数运算精度；EdgeDetector 边缘越界检测（右越界/左越界/无邻居）；RelativeDelta 跨平台一致性；AbsolutePosition 边缘 Handoff 路径
- **依赖任务**：CF0-TASK-018、CF0-TASK-019
- **输入**：design.md §2.10 坐标映射算法设计
- **输出**：`tests/test_coord.cpp` 新增
- **验收标准**：
  1. 越界量换算正确（右/左边缘）
  2. 纵向比例映射正确
  3. 全部整数运算，无浮点精度问题
  4. 边缘越界检测正确
- **预估复杂度**：中
- **平台归属**：跨平台共享

### 12.5 CF0-TASK-049 拓扑管理单元测试

- **任务标题**：编写拓扑管理单元测试（循环 + 退化）
- **任务描述**：在 `tests/test_topology.cpp` 中编写拓扑管理单元测试；拓扑加载与校验、邻居查询（循环映射）、两点退化拒绝（`CFX-E-TOPO-TWOPOINT-DEGEN`）、循环拓扑无死路、运行时拓扑修改
- **依赖任务**：CF0-TASK-015、CF0-TASK-017
- **输入**：design.md §2.5.5 循环切换实现、spec §5.2 端点身份与拓扑模型
- **输出**：`tests/test_topology.cpp` 新增
- **验收标准**：
  1. 拓扑加载与校验正确
  2. 循环映射正确（最左端左邻居 = 最右端）
  3. 两点退化正确拒绝
  4. 循环拓扑无死路
- **预估复杂度**：中
- **平台归属**：跨平台共享

### 12.6 CF0-TASK-050 传输层单元测试

- **任务标题**：编写传输层单元测试（双平面隔离 + 协议编解码）
- **任务描述**：在 `tests/test_transport.cpp` 中编写传输层单元测试；FrameCodec 编解码往返、ControlMessage variant 编解码、Control Plane TCP 可靠有序（重传/序号）、Input Plane UDP 最新优先丢帧、协议版本协商、配对门控
- **依赖任务**：CF0-TASK-013、CF0-TASK-022、CF0-TASK-023
- **输入**：design.md §2.11 传输层设计
- **输出**：`tests/test_transport.cpp` 新增
- **验收标准**：
  1. FrameCodec 编解码往返一致
  2. Control Plane 可靠有序
  3. Input Plane 最新优先丢帧
  4. 协议版本协商与配对门控正确
- **预估复杂度**：中
- **平台归属**：跨平台共享

### 12.7 CF0-TASK-051 并发模型单元测试

- **任务标题**：编写并发模型单元测试（无锁队列 + 原子快照 + 线程模型）
- **任务描述**：在 `tests/test_concurrency.cpp` 中编写并发模型单元测试；SpscQueue 无锁无竞争 push/pop、满时返回 false；MpmcQueue 分片隔离、溢出丢弃计数；原子状态快照 `is_lock_free()` 验证；ThreadModel 8 线程启动/停止、线程数 ≤8、终止清理 ≤200ms
- **依赖任务**：CF0-TASK-032、CF0-TASK-033、CF0-TASK-034、CF0-TASK-035
- **输入**：design.md §2.7 并发与线程安全模型
- **输出**：`tests/test_concurrency.cpp` 新增
- **验收标准**：
  1. SpscQueue 无锁正确，满时返回 false
  2. MpmcQueue 分片隔离正确
  3. 原子状态 `is_lock_free()` 全部为 true
  4. 8 线程启动/停止正确
  5. 终止清理 ≤200ms
- **预估复杂度**：中
- **平台归属**：跨平台共享

### 12.8 CF0-TASK-052 ACK 事务语义单元测试

- **任务标题**：编写 ACK 事务语义单元测试（PREPARE→ACK→COMMIT→ACTIVE）
- **任务描述**：在 `tests/test_ack_transaction.cpp` 中编写 ACK 事务单元测试；PREPARE 阶段 Source 持 LOCAL ownership；ACK 阶段 Source 仍持 LOCAL ownership；COMMIT 阶段 Source 释放 ownership；ACTIVE 阶段 Target 成为 LOCAL owner；`commitOwnership` 仅 ACK 态可调用（其他态返回 `CFX-E-HANDOFF-ILLEGAL-STATE`）；COMMIT 不可回滚；void-owner 修复路径
- **依赖任务**：CF0-TASK-039
- **输入**：design.md §2.5.6 ACK 事务语义设计
- **输出**：`tests/test_ack_transaction.cpp` 新增
- **验收标准**：
  1. PREPARE→ACK→COMMIT→ACTIVE 事务阶段正确
  2. COMMIT 前 Source 恒持 LOCAL ownership
  3. `commitOwnership` 非法态调用返回错误
  4. COMMIT 不可回滚
  5. void-owner 修复路径正确
- **预估复杂度**：中
- **平台归属**：跨平台共享

---

## 13. 集成测试

> 本组任务实现跨模块集成测试。v3 关键变更：防抖动行为契约、ACK 事务 split-brain/void-owner、并发终止。

### 13.1 CF0-TASK-053 Handoff 端到端集成测试

- **任务标题**：编写 Handoff 端到端集成测试（Mac→Win 切换全流程）
- **任务描述**：在 `tests/integration/test_handoff_e2e.cpp` 中编写端到端集成测试；模拟 Mac→Win Handoff 全流程：边缘越界 → PENDING → ACK → COMMIT → ACTIVE → COOLDOWN → ARMED；验证端到端 ≤30ms；验证控制权正确转移；验证 Input Plane 事件流正确转发
- **依赖任务**：CF0-TASK-040、CF0-TASK-042
- **输入**：design.md §2.5.4 握手时序
- **输出**：`tests/integration/test_handoff_e2e.cpp` 新增
- **验收标准**：
  1. Mac→Win Handoff 全流程正确
  2. 端到端 ≤30ms
  3. 控制权正确转移
  4. Input Plane 事件流正确转发
- **预估复杂度**：高
- **平台归属**：跨平台共享

### 13.2 CF0-TASK-054 防抖动行为契约集成测试

- **任务标题**：编写防抖动行为契约集成测试（CF0-DEBOUNCE-001~005）
- **任务描述**：在 `tests/integration/test_debounce_behavior.cpp` 中编写防抖动行为契约集成测试；CF0-DEBOUNCE-001: 模拟鼠标边缘反复进出 10 次/秒，COOLDOWN 期间 0 次 Handoff；CF0-DEBOUNCE-002: 鼠标 Mac↔Win 边缘抖动 1 秒，Handoff 次数 ≤5；CF0-DEBOUNCE-003: ACTIVE 后 30ms 越界被忽略，60ms 越界被接受；CF0-DEBOUNCE-004: 完成 Handoff 后 70ms 越界被丢弃，90ms 被接受；CF0-DEBOUNCE-005: 1 秒内 6 次 Handoff → 熔断，冷却期 ≥300ms
- **依赖任务**：CF0-TASK-031、CF0-TASK-047
- **输入**：design.md §2.6.0 防抖动行为契约
- **输出**：`tests/integration/test_debounce_behavior.cpp` 新增
- **验收标准**：
  1. CF0-DEBOUNCE-001: COOLDOWN 期间 0 次 Handoff
  2. CF0-DEBOUNCE-002: 1 秒内 Handoff ≤5
  3. CF0-DEBOUNCE-003: ACTIVE 后 ≥50ms 才允许越界
  4. CF0-DEBOUNCE-004: COOLDOWN ≥80ms
  5. CF0-DEBOUNCE-005: 熔断后 ≥300ms + 连续 3 次告警
- **预估复杂度**：高
- **平台归属**：跨平台共享

### 13.3 CF0-TASK-055 ACK 事务与 Safety Invariant 集成测试

- **任务标题**：编写 ACK 事务集成测试（split-brain 抑制 + void-owner 修复）
- **任务描述**：在 `tests/integration/test_ack_safety.cpp` 中编写 ACK 事务安全集成测试；模拟 PENDING 超时 → Source 保持控制权；模拟 ACK 超时 → Source 保持控制权；模拟 COMMIT 丢失 → void-owner 修复（Source RECOVERY 后重新获得 ownership）；模拟双向并发 Handoff → 裁决后 Control Owner = 1；模拟 ACTIVE disconnect → RECOVERY → 本地不粘键/不双控
- **依赖任务**：CF0-TASK-039、CF0-TASK-041
- **输入**：design.md §2.5.6.4 Split-Brain 抑制证明、§2.5.6.5 Void-Owner 修复路径
- **输出**：`tests/integration/test_ack_safety.cpp` 新增
- **验收标准**：
  1. PENDING 超时后 Source 保持控制权
  2. ACK 超时后 Source 保持控制权
  3. COMMIT 丢失后 void-owner 正确修复
  4. 双向并发裁决后 Control Owner = 1
  5. ACTIVE disconnect 后本地不粘键/不双控
- **预估复杂度**：高
- **平台归属**：跨平台共享

### 13.4 CF0-TASK-056 并发模型集成测试

- **任务标题**：编写并发模型集成测试（8 线程 + 终止 ≤200ms）
- **任务描述**：在 `tests/integration/test_concurrency_e2e.cpp` 中编写并发模型集成测试；8 线程同时运行，无竞争/无死锁；热路径禁锁/禁阻塞验证；终止信号 → 全部线程 ≤200ms 清理退出；FSM 若 ACTIVE/PENDING/ACK → RECOVERY 释放键鼠；无线程泄漏
- **依赖任务**：CF0-TASK-036、CF0-TASK-051
- **输入**：design.md §2.7 并发与线程安全模型
- **输出**：`tests/integration/test_concurrency_e2e.cpp` 新增
- **验收标准**：
  1. 8 线程无竞争/无死锁运行
  2. 热路径无锁/无阻塞
  3. 终止 ≤200ms 全部退出
  4. FSM 清理正确
  5. 无线程泄漏
- **预估复杂度**：高
- **平台归属**：跨平台共享

### 13.5 CF0-TASK-057 Coordinate Space 集成测试

- **任务标题**：编写 Coordinate Space 集成测试（CF0-COORD-001~005）
- **任务描述**：在 `tests/integration/test_coord_behavior.cpp` 中编写坐标空间行为集成测试；CF0-COORD-001: 抓包观察 Input Plane 鼠标移动帧 payload 为 RelativeDelta；CF0-COORD-002: Handoff 请求携带 entry_coord (AbsolutePosition)，ACTIVE 后切换回 RelativeDelta；CF0-COORD-003: macOS 移动 100px → Windows 注入 100px；CF0-COORD-004: 分辨率变更后转发正常；CF0-COORD-005: Core 层无平台坐标 API 引用
- **依赖任务**：CF0-TASK-020
- **输入**：design.md §2.10.0.6 行为契约
- **输出**：`tests/integration/test_coord_behavior.cpp` 新增
- **验收标准**：
  1. CF0-COORD-001~005 全部验证通过
- **预估复杂度**：中
- **平台归属**：跨平台共享

---

## 14. 架构契约测试（v3 新增）

> 本组任务实现 7 个核心契约架构测试 + Safety Invariant 测试。v3 关键变更：统一可验证格式、架构测试 ID、Safety Invariant 最高优先级。

### 14.1 CF0-TASK-058 契约① 架构测试：CF0-ARCH-NORM-001

- **任务标题**：实现契约① CanonicalInputEvent 规范化隔离架构测试
- **任务描述**：在 `tests/architecture/test_arch_norm.cpp` 中实现契约① 架构测试；验证 Core 层无平台头文件依赖（无 CGEvent、无 Windows.h）；验证 `CanonicalInputEvent` 平台无关；验证平台差异全部隔离在适配层（模块 F）
- **依赖任务**：CF0-TASK-008、CF0-TASK-009、CF0-TASK-010
- **输入**：design.md §2.9.1 契约① 保障、spec §7.1
- **输出**：`tests/architecture/test_arch_norm.cpp` 新增
- **验收标准**：
  1. Core 层无平台头文件依赖
  2. `CanonicalInputEvent` 平台无关
  3. 平台差异隔离在适配层
- **预估复杂度**：中
- **平台归属**：跨平台共享

### 14.2 CF0-TASK-059 契约② 架构测试：CF0-ARCH-FSM-001

- **任务标题**：实现契约② Handoff FSM 完整状态架构测试
- **任务描述**：在 `tests/architecture/test_arch_fsm.cpp` 中实现契约② 架构测试；验证六态完整（ARMED/PENDING/ACK/ACTIVE/COOLDOWN/RECOVERY）；验证 12 条转移路径覆盖；验证非法转移拒绝；验证失败路径必经 RECOVERY；验证 COOLDOWN 不可跳过
- **依赖任务**：CF0-TASK-038、CF0-TASK-046
- **输入**：design.md §2.9.2 契约② 保障、spec §7.2
- **输出**：`tests/architecture/test_arch_fsm.cpp` 新增
- **验收标准**：
  1. 六态完整
  2. 12 条转移路径覆盖
  3. 非法转移拒绝
  4. 失败路径必经 RECOVERY
  5. COOLDOWN 不可跳过
- **预估复杂度**：中
- **平台归属**：跨平台共享

### 14.3 CF0-TASK-060 契约③ 架构测试：CF0-ARCH-TOPO-001

- **任务标题**：实现契约③ Topology 循环支持架构测试
- **任务描述**：在 `tests/architecture/test_arch_topo.cpp` 中实现契约③ 架构测试；验证循环拓扑无死路；验证最右端右越界 → 最左端；验证两点退化拒绝；验证 `HandoffOrchestrator` 不含回环特判
- **依赖任务**：CF0-TASK-017、CF0-TASK-049
- **输入**：design.md §2.9.3 契约③ 保障、spec §7.3
- **输出**：`tests/architecture/test_arch_topo.cpp` 新增
- **验收标准**：
  1. 循环拓扑无死路
  2. 两点退化拒绝
  3. `HandoffOrchestrator` 无回环特判
- **预估复杂度**：低
- **平台归属**：跨平台共享

### 14.4 CF0-TASK-061 契约④ 架构测试：CF0-ARCH-INPUT-001

- **任务标题**：实现契约④ Input Plane 直通架构测试
- **任务描述**：在 `tests/architecture/test_arch_input.cpp` 中实现契约④ 架构测试；验证 Input Plane 事件流直通（捕获 → 规范化 → 传输 → 注入，无中间状态机阻断）；验证最新优先丢帧；验证首帧 ≤5ms
- **依赖任务**：CF0-TASK-012、CF0-TASK-023
- **输入**：design.md §2.9.4 契约④ 保障、spec §7.4
- **输出**：`tests/architecture/test_arch_input.cpp` 新增
- **验收标准**：
  1. Input Plane 事件流直通
  2. 最新优先丢帧
  3. 首帧 ≤5ms
- **预估复杂度**：中
- **平台归属**：跨平台共享

### 14.5 CF0-TASK-062 契约⑤ 架构测试：CF0-ARCH-COORD-001

- **任务标题**：实现契约⑤ Coordinate Space 坐标空间架构测试
- **任务描述**：在 `tests/architecture/test_arch_coord.cpp` 中实现契约⑤ 架构测试；验证核心控制路径使用 RelativeDelta；验证边缘 Handoff 使用 AbsolutePosition；验证跨平台运动一致；验证分辨率变更不影响；验证 Core 层无平台坐标 API
- **依赖任务**：CF0-TASK-020、CF0-TASK-057
- **输入**：design.md §2.9.5 契约⑤ 保障、spec §7.5
- **输出**：`tests/architecture/test_arch_coord.cpp` 新增
- **验收标准**：
  1. CF0-COORD-001~005 全部架构测试通过
- **预估复杂度**：中
- **平台归属**：跨平台共享

### 14.6 CF0-TASK-063 契约⑥ 架构测试：CF0-ARCH-TRANSPORT-001

- **任务标题**：实现契约⑥ Transport 双平面隔离架构测试
- **任务描述**：在 `tests/architecture/test_arch_transport.cpp` 中实现契约⑥ 架构测试；验证 Control Plane（TCP 可靠有序）与 Input Plane（UDP 低延迟）完全隔离；验证无共享状态；验证 Control Plane TCP 可靠送达（COMMIT 送达保障 P1）
- **依赖任务**：CF0-TASK-022、CF0-TASK-023
- **输入**：design.md §2.9.6 契约⑥ 保障、spec §7.6
- **输出**：`tests/architecture/test_arch_transport.cpp` 新增
- **验收标准**：
  1. 双平面完全隔离
  2. Control Plane TCP 可靠有序
  3. Input Plane UDP 低延迟
  4. COMMIT 送达保障
- **预估复杂度**：中
- **平台归属**：跨平台共享

### 14.7 CF0-TASK-064 契约⑦ 架构测试：CF0-ARCH-CONCURRENCY-001

- **任务标题**：实现契约⑦ Concurrency/Threading 并发模型架构测试
- **任务描述**：在 `tests/architecture/test_arch_concurrency.cpp` 中实现契约⑦ 架构测试；验证 8 线程架构（`std::jthread`）；验证无锁队列 `is_lock_free()`；验证 FSM 单线程所有权；验证热路径禁锁/禁阻塞；验证终止 ≤200ms
- **依赖任务**：CF0-TASK-032、CF0-TASK-036、CF0-TASK-051
- **输入**：design.md §2.9.7 契约⑦ 保障、spec §7.7
- **输出**：`tests/architecture/test_arch_concurrency.cpp` 新增
- **验收标准**：
  1. 8 线程 `std::jthread`
  2. 无锁队列 `is_lock_free()`
  3. FSM 单线程所有权
  4. 热路径禁锁/禁阻塞
  5. 终止 ≤200ms
- **预估复杂度**：中
- **平台归属**：跨平台共享

### 14.8 CF0-TASK-065 Safety Invariant 架构测试：CF0-ARCH-SAFETY-001~006

- **任务标题**：实现 CF0 Architecture Safety Invariant 架构测试（最高优先级）
- **任务描述**：在 `tests/architecture/test_arch_safety.cpp` 中实现 Safety Invariant 全部 6 项验收测试；CF0-ARCH-SAFETY-001: 周期性查询全拓扑 Control Owner 数量 ≤1；CF0-ARCH-SAFETY-002: 模拟 PENDING 超时，Mac 控制权保留 + 键鼠可用；CF0-ARCH-SAFETY-003: 注入各类故障，≤300ms 恢复 P1∧P2；CF0-ARCH-SAFETY-004: 双向并发 Handoff，裁决后 Control Owner = 1；CF0-ARCH-SAFETY-005: 模拟 COMMIT 丢失，Source RECOVERY 后重新获得 ownership；CF0-ARCH-SAFETY-006: 长时 24 小时 + 故障注入，无 split-brain/无永久丢失
- **依赖任务**：CF0-TASK-055、CF0-TASK-058～064
- **输入**：design.md §2.17 CF0 Architecture Safety Invariant
- **输出**：`tests/architecture/test_arch_safety.cpp` 新增
- **验收标准**：
  1. CF0-ARCH-SAFETY-001: Control Owner ≤1
  2. CF0-ARCH-SAFETY-002: PENDING 超时后控制权保留
  3. CF0-ARCH-SAFETY-003: ≤300ms 恢复 P1∧P2
  4. CF0-ARCH-SAFETY-004: 双向并发裁决后 Owner = 1
  5. CF0-ARCH-SAFETY-005: COMMIT 丢失后 void-owner 修复
  6. CF0-ARCH-SAFETY-006: 24 小时无 split-brain/无永久丢失
  7. 本测试为 CF0 冻结必要条件，任一失败 CF0 不得冻结
- **预估复杂度**：高
- **平台归属**：跨平台共享

---

## 15. 验证与冻结

> 本组任务实现最终验证与架构冻结。v3 关键变更：C++20 分级验证、Safety Invariant 冻结条件、长时稳定性测试。

### 15.1 CF0-TASK-066 C++20 特性分级验证

- **任务标题**：验证 C++20 特性分级 CF0-CPP20-001~006
- **任务描述**：运行 C++20 特性分级检查；CF0-CPP20-001: grep FSM 层无 coroutine；CF0-CPP20-002: FSM 测试纯同步驱动；CF0-CPP20-003: enum class 不作重点验收；CF0-CPP20-004: concepts 不过度使用；CF0-CPP20-005: MUST 级特性使用证据；CF0-CPP20-006: FORBIDDEN 级特性未出现
- **依赖任务**：CF0-TASK-043、CF0-TASK-044
- **输入**：design.md §2.8.2.1 分级关键约束
- **输出**：C++20 特性分级验证报告
- **验收标准**：
  1. CF0-CPP20-001~006 全部验证通过
  2. CI 矩阵全组合编译 + 测试通过
- **预估复杂度**：低
- **平台归属**：跨平台共享

### 15.2 CF0-TASK-067 性能 DFX 红线验证

- **任务标题**：验证性能 DFX 红线达标
- **任务描述**：验证全部 DFX 红线：端到端 Handoff ≤30ms；FSM 转移 ≤1ms；首帧传输 ≤5ms；捕获回调 ≤1ms；冷却期 ≥80ms；最小停留 ≥50ms；终止清理 ≤200ms；RECOVERY 清理 ≤100ms；热路径无锁/无阻塞/无异常
- **依赖任务**：CF0-TASK-042、CF0-TASK-036
- **输入**：design.md §2.14 性能保障措施、spec §4.1 性能
- **输出**：性能 DFX 红线验证报告
- **验收标准**：
  1. 端到端 Handoff ≤30ms
  2. FSM 转移 ≤1ms
  3. 首帧 ≤5ms
  4. 捕获回调 ≤1ms
  5. 终止清理 ≤200ms
  6. RECOVERY ≤100ms
- **预估复杂度**：中
- **平台归属**：跨平台共享

### 15.3 CF0-TASK-068 长时稳定性测试（24 小时 + 故障注入）

- **任务标题**：执行 24 小时长时稳定性测试与故障注入
- **任务描述**：执行 24 小时连续运行测试；周期性注入故障（网络断开、超时、双向并发、COMMIT 丢失）；监控 split-brain / void-owner / 线程泄漏 / 内存泄漏；验证 Safety Invariant 全时成立；验证无永久丢失
- **依赖任务**：CF0-TASK-065
- **输入**：design.md §2.17.4 Safety Invariant 可验证性
- **输出**：长时稳定性测试报告
- **验收标准**：
  1. 24 小时连续运行无崩溃
  2. 故障注入后全部正确恢复
  3. 无 split-brain / 无永久丢失
  4. 无线程泄漏 / 无内存泄漏
  5. CF0-ARCH-SAFETY-006 通过
- **预估复杂度**：高
- **平台归属**：跨平台共享

### 15.4 CF0-TASK-069 架构冻结审查与交付

- **任务标题**：执行 CF0 架构冻结审查与交付
- **任务描述**：汇总全部验收结果；确认 7 个核心契约（CF0-ARCH-NORM/FSM/TOPO/INPUT/COORD/TRANSPORT/CONCURRENCY-001）全部通过；确认 Safety Invariant（CF0-ARCH-SAFETY-001~006）全部通过；确认 C++20 特性分级（CF0-CPP20-001~006）全部通过；确认防抖动行为契约（CF0-DEBOUNCE-001~005）全部通过；确认坐标空间行为契约（CF0-COORD-001~005）全部通过；确认 DFX 红线全部达标；任一验收项失败 → CF0 不得冻结，回退修正
- **依赖任务**：CF0-TASK-058～065、CF0-TASK-066、CF0-TASK-067、CF0-TASK-068
- **输入**：全部验收报告
- **输出**：CF0 架构冻结审查报告
- **验收标准**：
  1. 7 个核心契约全部通过
  2. Safety Invariant 全部通过（最高优先级）
  3. C++20 特性分级全部通过
  4. 防抖动行为契约全部通过
  5. 坐标空间行为契约全部通过
  6. DFX 红线全部达标
  7. 全部通过 → CF0 架构冻结，交付后续阶段
  8. 任一失败 → CF0 不得冻结，回退修正
- **预估复杂度**：中
- **平台归属**：跨平台共享

---

> **任务规划结束**
> 本编码任务规划基于 `.codeartsdoer/specs/cf0_arch_freeze/spec.md`（v2，892 行）与 `.codeartsdoer/specs/cf0_arch_freeze/design.md`（v3，3449 行）生成。
> **任务统计**：共 15 组 69 个任务（Group 1: 4 + Group 2: 3 + Group 3: 4 + Group 4: 3 + Group 5: 3 + Group 6: 3 + Group 7: 5 + Group 8: 6 + Group 9: 5 + Group 10: 6 + Group 11: 2 + Group 12: 8 + Group 13: 5 + Group 14: 8 + Group 15: 4）。
> **v3 更新覆盖**：C++20 升级（TASK-001~004, 043~044, 066）；六态 FSM（TASK-007, 037~042, 046, 053）；防抖动四重契约（TASK-026~031, 047, 054）；并发模型（TASK-032~036, 051, 056）；ACK 事务语义（TASK-039, 052, 055）；Coordinate Space（TASK-005, 006, 018~020, 057, 062）；7 核心契约架构测试（TASK-058~064）；Safety Invariant（TASK-065, 068, 069）；12 个新错误码（TASK-003）；5 个新接口（TASK-026, 027, 029, 032, 037）。
> 待用户审查确认后，交付编码实现阶段。