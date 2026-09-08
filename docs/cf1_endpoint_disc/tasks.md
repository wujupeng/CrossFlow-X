# CrossFlow-X · CF1 端点身份与发现编码任务规划

> **阶段标记**：CF1 — Endpoint Identity & Discovery
> **对应需求规格**：`.codeartsdoer/specs/cf1_endpoint_disc/spec.md`（v2，1092 行，CF1-S01～S05 五个身份与发现地基 + 4 个 Amendment + Verification Matrix，已 FROZEN）
> **对应实现方案**：`.codeartsdoer/specs/cf1_endpoint_disc/design.md`（v4，~2997 行，9 个接口 + 组件分解 + 协议状态机 + Design Contract 回映 Verification Matrix，已 FROZEN）
> **CF0 冻结基线引用**：`.codeartsdoer/specs/cf0_arch_freeze/spec.md`（v2，892 行）+ `.codeartsdoer/specs/cf0_arch_freeze/design.md`（v3，3449 行）+ `.codeartsdoer/specs/cf0_arch_freeze/tasks.md`（69 个任务，CF0-TASK-001~069 已实现）
> **技术栈**：C++20 + CMake + CTest（复用 CF0 基础设施，不引入新线程、不修改 CF0 Frozen 接口签名）
> **第一原则**：NodeID 是身份，IP 只是当前可达地址
> **任务编号格式**：`CF1-TASK-XXX`（大写连字符，延续 CF0 编号风格）
> **文件命名规范**：snake_case（如 `node_identity_manager.cpp`、`session_fence.hpp`）
> **任务粒度**：每个任务可在 1-4 小时内完成，包含明确输入/输出/验收标准
> **执行纪律**：严格遵守大G项目经理 10 条执行纪律（不修改 CF0 Frozen / 不重新设计 Handoff FSM / 4 Amendment 转 Design Contract / 三元 Fence / TopologyVersion 原子提交 / Pairing 状态机实现边界 / Input/Control 双平面隔离 / Design Contract 回映 Verification Matrix / Gate Review 后再编码）

---

## 任务依赖总览

```
[Group 1 基础设施扩展(错误码+领域模型+消息)] ──┬──> [Group 2 CF1-S01 NodeIdentity]
                                                 ├──> [Group 3 CF1-S02 Discovery]
                                                 ├──> [Group 4 CF1-S03 Pairing/Registration]
                                                 ├──> [Group 5 CF1-S04 Topology Membership]
                                                 └──> [Group 6 CF1-S05 Session/Reconnection]
                                                          │
                                                          v
[Group 7 单元测试] <─────────── 各实现组并行触发 ──────────────────
                                                          │
                                                          v
[Group 8 集成测试] ──> [Group 9 Design Contract 验证] ──> [Group 10 验证与冻结]
```

**关键依赖链**：
- 基础设施扩展（Group 1）必须最先完成，所有上层依赖 CF1 错误码与领域模型
- NodeIdentity（Group 2）必须先于 Discovery（Group 3），发现需发布 NodeIdentity 声明
- Discovery（Group 3）必须先于 Pairing/Registration（Group 4），注册前置发现校验
- Pairing/Registration（Group 4）必须先于 Topology Membership（Group 5），成员加入前置注册成功
- SessionFence（Group 6）依赖 NodeIdentity（Session Epoch/InstanceID），与 Pairing/Registration 并行
- TopologyVersion Atomic Commit（Group 5）依赖 SessionFence（消息准入检查）
- 单元测试（Group 7）与实现同步，集成测试（Group 8）在组件完成后
- Design Contract 验证（Group 9）依赖全部实现组完成
- CF0 Safety Invariant 保持验证（Group 9）为最高优先级，任一失败 CF1 不得冻结

---

## 任务总览表

| 任务 ID | 任务标题 | 优先级 | 依赖任务 | 状态 |
|---------|---------|--------|---------|------|
| CF1-TASK-001 | CF1 错误码体系扩展（30 个新错误码） | P0 | CF0-TASK-003 | ⬜ |
| CF1-TASK-002 | NodeIdentity 七要素领域模型定义 | P0 | CF0-TASK-005 | ⬜ |
| CF1-TASK-003 | Discovery/Session/Topology 领域对象定义 | P0 | CF1-TASK-002 | ⬜ |
| CF1-TASK-004 | ControlMessage variant 扩展（6 类新子类型） | P0 | CF0-TASK-021 | ⬜ |
| CF1-TASK-005 | CF1 消息序列化与 TXT 记录编解码 | P1 | CF1-TASK-003, CF1-TASK-004 | ⬜ |
| CF1-TASK-006 | INodeIdentityManager 接口定义 | P0 | CF1-TASK-002 | ⬜ |
| CF1-TASK-007 | NodeIdentityManager 实现（七要素+NodeID 持久化） | P0 | CF1-TASK-006 | ⬜ |
| CF1-TASK-008 | PublicKey 字段位预留与生命周期解耦（AMEND-001） | P0 | CF1-TASK-007 | ⬜ |
| CF1-TASK-009 | Capabilities 声明与 Endpoint Addresses 动态管理 | P1 | CF1-TASK-007 | ⬜ |
| CF1-TASK-010 | Session Epoch 生成与持久化 | P0 | CF1-TASK-007 | ⬜ |
| CF1-TASK-011 | NodeID 持久化完整性恢复 | P1 | CF1-TASK-007 | ⬜ |
| CF1-TASK-012 | IDiscoveryService 接口定义 | P0 | CF1-TASK-003 | ⬜ |
| CF1-TASK-013 | MdnsAnnouncer mDNS 声明发布 | P0 | CF1-TASK-012 | ⬜ |
| CF1-TASK-014 | MdnsListener mDNS 声明监听 + DiscoveryTable | P0 | CF1-TASK-013 | ⬜ |
| CF1-TASK-015 | LAN Broadcast / Manual Config fallback | P1 | CF1-TASK-013 | ⬜ |
| CF1-TASK-016 | Discovery Digest 构建与 TXT 记录 | P1 | CF1-TASK-005, CF1-TASK-013 | ⬜ |
| CF1-TASK-017 | 动态加入/离开+幂等+跨拓扑隔离+冲突检测 | P1 | CF1-TASK-014 | ⬜ |
| CF1-TASK-018 | IPairingManager/IRegistrationManager/ITrustedNodeList 接口定义 | P0 | CF1-TASK-003 | ⬜ |
| CF1-TASK-019 | PairingFsm 7 状态 FSM 实现 | P0 | CF1-TASK-018 | ⬜ |
| CF1-TASK-020 | 五层分层不可跃迁守卫 | P0 | CF1-TASK-019 | ⬜ |
| CF1-TASK-021 | TrustedNodeList 持久化实现 | P0 | CF1-TASK-018 | ⬜ |
| CF1-TASK-022 | 配对码经 Control Plane 传输（禁广播） | P0 | CF1-TASK-021 | ⬜ |
| CF1-TASK-023 | HandshakeOrchestrator 注册握手编排 | P0 | CF1-TASK-019, CF1-TASK-022 | ⬜ |
| CF1-TASK-024 | 注册原子性+幂等+失败回滚 | P1 | CF1-TASK-023 | ⬜ |
| CF1-TASK-025 | IMembershipManager/ITopologyVersionManager 接口定义 | P0 | CF1-TASK-003 | ⬜ |
| CF1-TASK-026 | MembershipManager 动态成员管理 | P0 | CF1-TASK-025 | ⬜ |
| CF1-TASK-027 | NodeCountGuard+DEGRADED 判定+循环拓扑维护 | P1 | CF1-TASK-026 | ⬜ |
| CF1-TASK-028 | TopologyVersionManager 版本覆盖规则 | P0 | CF1-TASK-025 | ⬜ |
| CF1-TASK-029 | Topology Atomic Commit 严格协议状态机（D-TOPO-ATOMIC-005） | P0 | CF1-TASK-028 | ⬜ |
| CF1-TASK-030 | Coordinator Identity Lifecycle（D-TOPO-COORD-005） | P0 | CF1-TASK-029 | ⬜ |
| CF1-TASK-031 | 成员变更通知广播 | P1 | CF1-TASK-026 | ⬜ |
| CF1-TASK-032 | ISessionFence/IIdentityRecoveryManager 接口定义 | P0 | CF1-TASK-003 | ⬜ |
| CF1-TASK-033 | SessionFence 三元栅栏实现 | P0 | CF1-TASK-032 | ⬜ |
| CF1-TASK-034 | EPOCH-001~006 六条测试契约实现 | P0 | CF1-TASK-033 | ⬜ |
| CF1-TASK-035 | Bootstrap/Established Session 两级 Message Admission | P0 | CF1-TASK-033 | ⬜ |
| CF1-TASK-036 | IdentityRecoveryManager 断线重连身份恢复 | P1 | CF1-TASK-033, CF1-TASK-035 | ⬜ |
| CF1-TASK-037 | NodeIdentity 领域模型单元测试 | P1 | CF1-TASK-007 | ⬜ |
| CF1-TASK-038 | Discovery 协议单元测试 | P1 | CF1-TASK-014 | ⬜ |
| CF1-TASK-039 | Pairing/Registration FSM 单元测试 | P1 | CF1-TASK-019, CF1-TASK-023 | ⬜ |
| CF1-TASK-040 | TopologyVersion Atomic Commit 单元测试 | P1 | CF1-TASK-029, CF1-TASK-030 | ⬜ |
| CF1-TASK-041 | SessionFence 三元栅栏单元测试 | P1 | CF1-TASK-034 | ⬜ |
| CF1-TASK-042 | 端到端身份建立集成测试 | P1 | CF1-TASK-017, CF1-TASK-024, CF1-TASK-031 | ⬜ |
| CF1-TASK-043 | Topology Atomic Commit 集成测试 | P1 | CF1-TASK-040 | ⬜ |
| CF1-TASK-044 | Session Fencing 集成测试 | P1 | CF1-TASK-041 | ⬜ |
| CF1-TASK-045 | 断线重连身份恢复集成测试 | P1 | CF1-TASK-036 | ⬜ |
| CF1-TASK-046 | S01~S05 Design Contract 架构测试 | P1 | CF1-TASK-037~041 | ⬜ |
| CF1-TASK-047 | 4 个 Amendment Design Contract 验证 | P1 | CF1-TASK-046 | ⬜ |
| CF1-TASK-048 | CF0 Safety Invariant 保持验证 | P0 | CF1-TASK-047 | ⬜ |
| CF1-TASK-049 | DFX 红线验证 | P1 | CF1-TASK-048 | ⬜ |
| CF1-TASK-050 | CF1 架构冻结审查与交付 | P0 | CF1-TASK-049 | ⬜ |

**任务统计**：共 10 组 50 个任务（Group 1: 5 + Group 2: 6 + Group 3: 6 + Group 4: 7 + Group 5: 7 + Group 6: 5 + Group 7: 5 + Group 8: 4 + Group 9: 3 + Group 10: 2）

---

## 1. 基础设施扩展（错误码 + 领域模型 + 消息扩展）

> 本组任务扩展 CF0 基础设施以支撑 CF1 身份与发现功能。复用 CF0 error_code/logger/domain/messages，不修改 CF0 冻结的接口签名。

### 1.1 CF1-TASK-001 CF1 错误码体系扩展（30 个新错误码）

- **任务标题**：扩展结构化错误码体系，新增 CF1 全部 30 个错误码
- **任务描述**：在 `core/common/error_code.hpp` 中扩展错误码体系，新增 CF1 全部 30 个错误码，覆盖 SESS/DISC/PAIR/REG/TOPO/RECOVERY/HANDOFF 七个模块：`CFX-E-SESS-UUID-GEN-FAIL`、`CFX-W-SESS-PERSIST-CORRUPT`、`CFX-E-SESS-NODEID-FORGE`、`CFX-W-SESSION-STALE-EPOCH`、`CFX-E-SESSION-INSTANCE-CONFLICT`、`CFX-W-SESSION-UNKNOWN-NODE`、`CFX-W-DISC-MDNS-UNAVAILABLE`、`CFX-W-DISC-CROSS-SUBNET`、`CFX-W-DISC-MANUAL-FALLBACK`、`CFX-W-DISC-ANNOUNCE-LOST`、`CFX-E-PAIR-CODE-MISMATCH`、`CFX-E-PAIR-UNPAIRED`、`CFX-E-PAIR-ILLEGAL-TRANS`、`CFX-E-PAIR-CODE-BROADCAST`、`CFX-E-PAIR-TRUST-PERSIST-FAIL`、`CFX-W-PAIR-TRUST-CORRUPT`、`CFX-E-REG-UNDISCOVERED`、`CFX-E-REG-CAP-INCOMPAT`、`CFX-E-REG-TOPOID-MISMATCH`、`CFX-E-REG-LINK-BROKEN`、`CFX-W-TOPO-STALE-VERSION`、`CFX-W-TOPO-VERSION-CONFLICT`、`CFX-E-TOPO-VALIDATION-FAIL`、`CFX-E-TOPO-COMMIT-FAIL`、`CFX-E-TOPO-MULTIPATH`、`CFX-W-TOPO-DEGRADED-NON-CIRCULAR`、`CFX-W-TOPO-DEGRADED-SINGLE`、`CFX-E-RECOVERY-UNTRUSTED`、`CFX-E-RECOVERY-UNREACHABLE`、`CFX-E-RECOVERY-TIMEOUT`、`CFX-W-HANDOFF-UNREGISTERED-PEER`、`CFX-E-TOPO-COORD-DUPLICATE-CLAIM`；遵循 `CFX-<level>-<module>-<reason>` 格式，扩展 `ErrorModule` enum class 新增 `Session/Discovery/Pairing/Registration/Recovery` 模块
- **依赖任务**：CF0-TASK-003
- **输入**：design.md §2.9.1 CF1 新增错误码定义、spec §4.4 错误码体系
- **输出**：`core/common/error_code.hpp` 更新（含全部 30 个 CF1 新增错误码）
- **验收标准**：
  1. 全部 30 个 CF1 新增错误码定义完整，格式为 `CFX-<level>-<module>-<reason>`
  2. `ErrorModule` enum class 新增 Session/Discovery/Pairing/Registration/Recovery 模块
  3. 错误码可转为结构化 JSON Lines 日志输出
  4. 无错误码重复定义，与 CF0 18 个错误码无冲突
- **Design Contract**：支撑全部 DC-S01~S05 错误映射
- **Acceptance Test**：错误码解析单元测试
- **优先级**：P0
- **预估工作量**：1-2 小时
- **平台归属**：跨平台共享

### 1.2 CF1-TASK-002 NodeIdentity 七要素领域模型定义

- **任务标题**：定义 NodeIdentity 七要素领域模型与扩展领域对象
- **任务描述**：在 `core/common/domain.hpp` 中扩展领域模型，新增 CF1 核心领域对象：`NodeIdentity`（七要素：nodeId + publicKey + platform + capabilities + endpointAddresses + topologyMembership + sessionEpoch）、`PublicKey`（变长字节串 + isPresent + empty()）、`Capabilities`（supportedInputTypes + screenBoundary + supportsCircular + protocolVersion）、`EndpointAddress`（addressType + value + port + priority）、`TopologyMembership`（topologyId + leftNeighbor + rightNeighbor + segmentIndex）、`SessionEpoch`（value:u64 + increment() + isMonotonicAfter()）、`SessionInstanceId`（value:NodeId + createdAt + isActive）、`SessionFence`（nodeId + epoch + instanceId + newSession()）；复用 CF0 冻结的 NodeId/Platform/ScreenBoundary/NeighborRelation/TopologyView，不修改已有字段；全部枚举使用 `enum class`，全部时间使用 `std::chrono::duration` 强类型
- **依赖任务**：CF0-TASK-005
- **输入**：design.md §2.3.2 模型实现、spec §5.1 Node Identity 七要素、§2 领域术语
- **输出**：`core/common/domain.hpp` 更新（含 NodeIdentity 七要素 + 全部 CF1 新增领域对象）
- **验收标准**：
  1. `NodeIdentity` 含七要素完整（nodeId/publicKey/platform/capabilities/endpointAddresses/topologyMembership/sessionEpoch）
  2. `PublicKey` 字段位预留，`empty()` 工厂方法返回空 PublicKey
  3. `SessionEpoch` 为 u64 单调递增，`isMonotonicAfter()` 校验方法
  4. `SessionFence` 为三元组（NodeID + Epoch + InstanceID）
  5. 全部枚举为 `enum class`，全部时间为 `std::chrono` 强类型
  6. 复用 CF0 NodeId/Platform/ScreenBoundary，不修改已有定义
- **Design Contract**：DC-S01-001（Stable NodeID 不变性）
- **Acceptance Test**：领域对象创建与校验测试
- **优先级**：P0
- **预估工作量**：2-3 小时
- **平台归属**：跨平台共享

### 1.3 CF1-TASK-003 Discovery/Session/Topology 领域对象定义

- **任务标题**：定义 Discovery/Session/Topology/Pairing 相关领域对象
- **任务描述**：在 `core/common/domain.hpp` 中新增 CF1 其余领域对象：`DiscoveryRecord`（nodeId + platform + capabilitiesFingerprint + sessionEpoch + protocolVersion + topologyId + endpointAddresses + lastSeenAt）、`DiscoveryDigest`（nodeId + platform + capabilitiesFingerprint + sessionEpoch + protocolVersion + topologyId + toTxtRecord()）、`PairingState` enum class（DISCOVERED/UNTRUSTED/PAIRING/TRUSTED/REGISTERING/REGISTERED/MEMBER/REJECTED/ROLLBACK 九态）、`TrustedNodeEntry`（nodeId + pairedAt + lastSeenEpoch）、`RegistrationRecord`（peerNodeId + peerIdentity + currentState + previousState + enteredAt + failureReason）、`MembershipEntry`（nodeId + topologyMembership + joinedAt + isOnline）、`TopologyVersion`（value:u64 + topologyId + lastCommitAt + commitState）、`TopologyCommitState` enum class（Active/Proposed/Validated/Prepared/ActivationAuthorized/Committing/RolledBack/Locked 八态）、`TopologyChangeProposal`（proposedVersion + membershipChange + proposedBy + proposedAt）、`SessionFenceVerdict` enum class（Accept/Reject/StaleEpoch/InstanceConflict/UnknownNode）；全部结构体平台无关，无平台头文件依赖
- **依赖任务**：CF1-TASK-002
- **输入**：design.md §2.3.2 模型实现、spec §5.2~§5.5 各地基领域术语
- **输出**：`core/common/domain.hpp` 更新（含全部 CF1 新增领域对象）
- **验收标准**：
  1. `PairingState` 含九态完整（7 成功态 + 2 失败态）
  2. `TopologyCommitState` 含八态完整（含 v3 新增 Prepared/ActivationAuthorized/Locked）
  3. `DiscoveryDigest.toTxtRecord()` 返回 `vector<pair<string,string>>` 供 mDNS TXT 记录
  4. 全部结构体平台无关，无 CGEvent/Windows.h 依赖
  5. 全部枚举为 `enum class`
- **Design Contract**：DC-S02-002、DC-S03-003、DC-S04-008
- **Acceptance Test**：领域对象创建与序列化测试
- **优先级**：P0
- **预估工作量**：2-3 小时
- **平台归属**：跨平台共享

### 1.4 CF1-TASK-004 ControlMessage variant 扩展（6 类新子类型）

- **任务标题**：扩展 ControlMessage variant，新增 CF1 六类控制报文子类型
- **任务描述**：在 `core/common/messages.hpp` 中扩展 `ControlMessage` variant，新增 CF1 六类控制报文子类型：`DiscoveryAnnouncement`（携带 DiscoveryDigest）、`PairingRequest`/`PairingResponse`（携带 NodeID + 配对码）、`RegistrationRequest`/`RegistrationResponse`（携带完整 NodeIdentity）、`MembershipChangeNotification`（携带成员变更类型 + NodeID + TopologyMembership）、`IdentityRecoveryRequest`/`IdentityRecoveryResponse`（携带 NodeID + SessionEpoch + SessionInstanceId）、`GoodbyeAnnouncement`（携带 NodeID）；每类报文含 `SessionFence` 三元组字段供消息准入检查；遵循 CF0 §5.5.1.8 协议向前兼容（旧版本安全忽略未知报文）
- **依赖任务**：CF0-TASK-021
- **输入**：design.md §1.1.3 ControlMessage variant 扩展、spec §7.3 CF1 新增 Control Message 子类型
- **输出**：`core/common/messages.hpp` 更新（含全部 CF1 新增 6 类控制报文）
- **验收标准**：
  1. `ControlMessage` variant 追加 6 类子类型，不修改 CF0 已有子类型
  2. 每类报文含 `SessionFence` 三元组字段
  3. 旧版本安全忽略未知报文（向前兼容）
  4. 配对码报文标记为 Bootstrap 类（不经三元 Fence NodeID Check）
  5. 全部报文平台无关
- **Design Contract**：DC-S05-004（Bootstrap/Established Session 两级 Admission）
- **Acceptance Test**：报文编解码往返测试
- **优先级**：P0
- **预估工作量**：1-2 小时
- **平台归属**：跨平台共享

### 1.5 CF1-TASK-005 CF1 消息序列化与 TXT 记录编解码

- **任务标题**：实现 CF1 控制报文序列化与 mDNS TXT 记录编解码
- **任务描述**：在 `core/s05_transport/cf1_message_codec.hpp/cpp` 中实现 CF1 控制报文序列化；扩展 CF0 FrameCodec 支持 CF1 新增 6 类控制报文编解码；`DiscoveryDigest.toTxtRecord()` 编码为 mDNS TXT 记录键值对（紧凑格式，NodeID 为 hex 字符串）；`DiscoveryDigest::fromTxtRecord()` 从 TXT 记录解码；全部编解码使用固定宽度整数（u8/u16/u32/u64），字节序 Little-Endian；解码失败返回 `std::expected<T, CfxError>`，不抛异常
- **依赖任务**：CF1-TASK-003、CF1-TASK-004
- **输入**：design.md §2.3.2 模型实现、spec §6.7 Discovery Digest mDNS TXT 记录
- **输出**：`core/s05_transport/cf1_message_codec.hpp`、`core/s05_transport/cf1_message_codec.cpp`
- **验收标准**：
  1. CF1 六类控制报文编解码往返一致（encode → decode == 原始报文）
  2. `DiscoveryDigest` TXT 记录编解码往返一致
  3. 全部使用固定宽度整数，Little-Endian 字节序
  4. 解码失败返回 `std::expected` 错误，不抛异常
  5. mDNS TXT 记录紧凑，DiscoveryDigest ≤256 字节
- **Design Contract**：DC-S02-001（不依赖固定 IP 发现）
- **Acceptance Test**：编解码往返单元测试
- **优先级**：P1
- **预估工作量**：2-3 小时
- **平台归属**：跨平台共享

---

## 2. CF1-S01 Node Identity Model

> 本组任务实现 NodeIdentity 七要素身份模型。AMEND-001 PublicKey 生命周期解耦。复用 CF0 NodeId/Platform/ScreenBoundary，扩展为 NodeIdentity 七要素。

### 2.1 CF1-TASK-006 INodeIdentityManager 接口定义

- **任务标题**：定义 INodeIdentityManager 接口（七要素身份管理）
- **任务描述**：在 `core/s01_event/i_node_identity_manager.hpp` 中定义 `INodeIdentityManager` 接口；`initialize() -> std::expected<NodeIdentity, CfxError>` 初始化本端身份（首次生成/后续加载）；`currentIdentity() const -> NodeIdentity` 查询七要素；`updateEndpointAddresses(std::span<const EndpointAddress>)` 更新地址列表；`updateCapabilities(const Capabilities&)` 更新能力；`updateTopologyMembership(const TopologyMembership&)` 更新拓扑成员；`incrementSessionEpoch() -> SessionEpoch` 递增 Epoch（EPOCH-001）；`newSessionInstanceId() -> SessionInstanceId` 生成新 InstanceID（AMEND-004）；`updatePublicKey(std::span<const u8>)` 更换 PublicKey（AMEND-001，不改变 NodeID）；`recoverFromCorruption() -> std::expected<void, CfxError>` 持久化完整性恢复；全部可失败操作返回 `std::expected`，回调使用 concepts 约束
- **依赖任务**：CF1-TASK-002
- **输入**：design.md §2.2.2.1 Node Identity 管理接口、spec §5.1 Node Identity 规则
- **输出**：`core/s01_event/i_node_identity_manager.hpp` 新增
- **验收标准**：
  1. 接口含全部 9 个方法，签名对齐 C++20 设施（std::expected/std::span/concepts）
  2. `initialize()` 返回 `std::expected<NodeIdentity, CfxError>`
  3. `updatePublicKey()` 注释标注 AMEND-001 契约（不改变 NodeID）
  4. 接口无平台头文件依赖
- **Design Contract**：DC-S01-001、DC-S01-002、DC-S01-003
- **Acceptance Test**：接口契约测试
- **优先级**：P0
- **预估工作量**：1 小时
- **平台归属**：跨平台共享

### 2.2 CF1-TASK-007 NodeIdentityManager 实现（七要素+NodeID 持久化）

- **任务标题**：实现 NodeIdentityManager（七要素聚合 + NodeID 生成持久化）
- **任务描述**：在 `core/s01_event/node_identity_manager.hpp/cpp` 中实现 `NodeIdentityManager`（实现 `INodeIdentityManager`）；`initialize()` 首次启动调用 `NodeId.generate()` 生成 UUIDv4（仅本地随机源，不消费 PublicKey，AMEND-001 规则 3）并持久化到本地 JSON 文件；后续启动从本地加载已有 NodeID，禁止重复生成；聚合七要素返回 `NodeIdentity`；NodeID 写入路径仅含 {本地生成, 本地持久化加载}，网络报文不得覆盖本端 NodeID（§5.1.1.13，DC-S01-002）；持久化在 FSM Thread 内；线程归属 FSM Thread
- **依赖任务**：CF1-TASK-006
- **输入**：design.md §2.2.2.1 接口业务说明、spec §5.1.1 Stable NodeID 规则
- **输出**：`core/s01_event/node_identity_manager.hpp`、`core/s01_event/node_identity_manager.cpp`
- **验收标准**：
  1. 首次启动生成 UUIDv4 并持久化，后续启动加载已有 NodeID
  2. NodeID 跨 IP 变更/重启/断线重连不变（DC-S01-001）
  3. NodeID 写入路径仅含本地生成/加载，网络报文不得覆盖（DC-S01-002）
  4. `NodeId.generate()` 不消费 PublicKey（AMEND-001 规则 3）
  5. 持久化在 FSM Thread 内，无跨线程竞争
- **Design Contract**：DC-S01-001（Stable NodeID 不变性）、DC-S01-002（NodeID 不可伪造）
- **Acceptance Test**：NodeID 不变性测试、持久化往返测试
- **优先级**：P0
- **预估工作量**：2-3 小时
- **平台归属**：跨平台共享

### 2.3 CF1-TASK-008 PublicKey 字段位预留与生命周期解耦（AMEND-001）

- **任务标题**：实现 PublicKey 字段位预留与生命周期解耦契约（AMEND-001）
- **任务描述**：在 `NodeIdentityManager` 中实现 PublicKey 字段位预留与生命周期解耦；`updatePublicKey(std::span<const u8>)` 更换 PublicKey 时 NodeID 不变（AMEND-001 规则 1）；`NodeId.generate()` 不消费 PublicKey（AMEND-001 规则 3）；网络报文不得以 PublicKey 变更为由覆盖本端 NodeID（AMEND-001 规则 2）；CF1 阶段 PublicKey 允许空（`PublicKey::empty()`），具体算法延迟至 CF8；`PublicKey.isPresent` 标识是否存在
- **依赖任务**：CF1-TASK-007
- **输入**：design.md §2.2.2.1 AMEND-001 契约、spec §5.1.4 PublicKey Lifecycle Contract
- **输出**：`NodeIdentityManager::updatePublicKey()` 实现、`PublicKey` 字段位预留
- **验收标准**：
  1. `updatePublicKey()` 更换 PublicKey 后 `currentIdentity().nodeId` 不变（AMEND-001 规则 1）
  2. `NodeId.generate()` 实现无 PublicKey 参数（AMEND-001 规则 3）
  3. CF1 阶段 PublicKey 允许空，`PublicKey::empty().isPresent == false`
  4. 网络报文处理不以 PublicKey 变更为由覆盖 NodeID（AMEND-001 规则 2）
- **Design Contract**：DC-S01-003（PublicKey 与 NodeID 生命周期解耦）
- **Acceptance Test**：PublicKey 更换后 NodeID 不变测试
- **优先级**：P0
- **预估工作量**：1-2 小时
- **平台归属**：跨平台共享

### 2.4 CF1-TASK-009 Capabilities 声明与 Endpoint Addresses 动态管理

- **任务标题**：实现 Capabilities 声明与 Endpoint Addresses 动态管理
- **任务描述**：在 `NodeIdentityManager` 中实现 Capabilities 声明与 Endpoint Addresses 动态管理；`updateCapabilities()` 探测本机支持的输入事件类型子集、屏幕边界、循环切换支持、协议版本号；`updateEndpointAddresses()` 订阅本机网络子系统地址变更（网卡启用/禁用、IP 分配/释放、DHCP 续约），地址列表可变但 NodeID 不变；每条 EndpointAddress 含 addressType（IPv4/IPv6/hostname）+ value + port + priority；本机无网络地址时 NodeIdentity 仍初始化，Endpoint Addresses 为空，mDNS 声明暂缓
- **依赖任务**：CF1-TASK-007
- **输入**：design.md §2.1.2 K-05/K-06 功能点、spec §5.1.1.7/§5.1.1.8 Capabilities/Endpoint Addresses
- **输出**：`NodeIdentityManager::updateCapabilities/updateEndpointAddresses` 实现
- **验收标准**：
  1. Capabilities 含 supportedInputTypes/screenBoundary/supportsCircular/protocolVersion
  2. Endpoint Addresses 可变，NodeID 不变（§5.1.1.8）
  3. 本机无网络地址时 NodeIdentity 仍初始化，地址列表为空
  4. 网络恢复后地址列表补全
- **Design Contract**：DC-S01-001（Stable NodeID 不变性）
- **Acceptance Test**：地址变更后 NodeID 不变测试
- **优先级**：P1
- **预估工作量**：2 小时
- **平台归属**：跨平台共享

### 2.5 CF1-TASK-010 Session Epoch 生成与持久化

- **任务标题**：实现 Session Epoch 生成与持久化（EPOCH-001）
- **任务描述**：在 `NodeIdentityManager` 中实现 Session Epoch 生成与持久化；`incrementSessionEpoch()` 每次启动/重连递增，从 1 开始，u64 单调递增不得回跳（EPOCH-001）；递增后立即持久化（防崩溃丢失）；`SessionEpoch.isMonotonicAfter(prev)` 校验单调性；持久化损坏时 Epoch 重置为 1 并告警 `CFX-W-SESS-PERSIST-CORRUPT`
- **依赖任务**：CF1-TASK-007
- **输入**：design.md §2.7.3 EPOCH-001 实现方案、spec §5.5.4.2 规则 1
- **输出**：`NodeIdentityManager::incrementSessionEpoch` 实现、`SessionEpochGenerator` 组件
- **验收标准**：
  1. SessionEpoch 从 1 开始，每次启动/重连递增（EPOCH-001）
  2. u64 单调递增不得回跳
  3. 递增后立即持久化，崩溃重启不丢失
  4. 持久化损坏时重置为 1 并告警
- **Design Contract**：EPOCH-001（SessionEpoch 单调递增）
- **Acceptance Test**：Epoch 单调递增测试、持久化往返测试
- **优先级**：P0
- **预估工作量**：1-2 小时
- **平台归属**：跨平台共享

### 2.6 CF1-TASK-011 NodeID 持久化完整性恢复

- **任务标题**：实现 NodeID 持久化完整性恢复策略
- **任务描述**：在 `NodeIdentityManager` 中实现 `recoverFromCorruption()` 持久化完整性恢复；NodeID 损坏 → 重新生成 + 告警 `CFX-W-SESS-PERSIST-CORRUPT`；Trusted List 损坏 → 清空 + 告警 `CFX-W-PAIR-TRUST-CORRUPT`；Topology Membership 损坏 → 置空等待重新发现；Session Epoch 损坏 → 重置为 1；不崩溃，保持 Safety Invariant；全部恢复策略记录结构化日志
- **依赖任务**：CF1-TASK-007
- **输入**：design.md §2.9.3 异常恢复策略、spec §5.1.3.1 持久化文件损坏
- **输出**：`NodeIdentityManager::recoverFromCorruption` 实现
- **验收标准**：
  1. NodeID 损坏 → 重新生成 + 告警，不崩溃
  2. Trusted List 损坏 → 清空 + 告警，不影响本端 NodeID
  3. Topology Membership 损坏 → 置空等待重新发现
  4. 全部恢复策略记录结构化日志
  5. 恢复后 Safety Invariant 仍成立
- **Design Contract**：DC-S01-001（P3 Recoverable）
- **Acceptance Test**：各类损坏恢复测试
- **优先级**：P1
- **预估工作量**：2 小时
- **平台归属**：跨平台共享

---

## 3. CF1-S02 Discovery Protocol

> 本组任务实现 mDNS 发现机制。线程归属 Control Plane Thread。发现以 NodeID 为键，不以 IP 为键。

### 3.1 CF1-TASK-012 IDiscoveryService 接口定义

- **任务标题**：定义 IDiscoveryService 接口（mDNS 发现服务）
- **任务描述**：在 `core/s02_topology/i_discovery_service.hpp` 中定义 `IDiscoveryService` 接口；`start(const NodeIdentity&) -> std::expected<void, CfxError>` 启动发现（发布 mDNS 声明 + 监听）；`stop()` 停止（发布 goodbye）；`republishAnnouncement()` 重新发布（IP/拓扑变更时）；`discoveredPeers() const -> std::span<const DiscoveryRecord>` 查询已发现端点（以 NodeID 为键）；`findDiscovered(const NodeId&) const -> std::optional<DiscoveryRecord>` 查询指定 NodeID；`onDiscovered/onPeerLeft/onNodeidConflict` 回调（concepts 约束）；全部可失败操作返回 `std::expected`
- **依赖任务**：CF1-TASK-003
- **输入**：design.md §2.2.2.2 Discovery 服务接口、spec §5.2 发现机制规则
- **输出**：`core/s02_topology/i_discovery_service.hpp` 新增
- **验收标准**：
  1. 接口含全部 7 个方法，签名对齐 C++20 设施
  2. `discoveredPeers()` 返回 `std::span<const DiscoveryRecord>`，无 vector 拷贝
  3. 回调使用 concepts 约束（`std::invocable`）
  4. 接口无平台头文件依赖
- **Design Contract**：DC-S02-001、DC-S02-002、DC-S02-003
- **Acceptance Test**：接口契约测试
- **优先级**：P0
- **预估工作量**：1 小时
- **平台归属**：跨平台共享

### 3.2 CF1-TASK-013 MdnsAnnouncer mDNS 声明发布

- **任务标题**：实现 MdnsAnnouncer mDNS 声明发布
- **任务描述**：在 `core/s02_topology/mdns_announcer.hpp/cpp` 中实现 `MdnsAnnouncer`；发布 mDNS service 声明，service name 固定 CrossFlow-X 标识（不得随版本变更）；TXT 记录携带 DiscoveryDigest（NodeID/Platform/Capabilities 指纹/Session Epoch/协议版本/Topology ID）；发布延迟 ≤500ms（DFX 红线 §4.1.1）；周期性刷新防止声明过期（≥1 次/60s，≤1 次/5s 避免风暴）；IP 变更/拓扑变更时立即触发额外声明；mDNS 套接字在 Control Plane Thread 内；macOS 使用 Bonjour API，Windows 使用 mDNS API
- **依赖任务**：CF1-TASK-012
- **输入**：design.md §2.1.2 L-01/L-08 功能点、spec §5.2.1 发现声明规则
- **输出**：`core/s02_topology/mdns_announcer.hpp`、`core/s02_topology/mdns_announcer.cpp`
- **验收标准**：
  1. mDNS 声明发布延迟 ≤500ms
  2. service name 固定 CrossFlow-X 标识
  3. TXT 记录携带 DiscoveryDigest 完整
  4. 周期性刷新 ≥1 次/60s，≤1 次/5s
  5. mDNS 套接字在 Control Plane Thread 内
- **Design Contract**：DC-S02-001（不依赖固定 IP 发现）
- **Acceptance Test**：mDNS 声明发布延迟测试
- **优先级**：P0
- **预估工作量**：3-4 小时
- **平台归属**：跨平台共享（macOS Bonjour + Windows mDNS）

### 3.3 CF1-TASK-014 MdnsListener mDNS 声明监听 + DiscoveryTable

- **任务标题**：实现 MdnsListener mDNS 声明监听与 DiscoveryTable（NodeID 索引）
- **任务描述**：在 `core/s02_topology/mdns_listener.hpp/cpp` 中实现 `MdnsListener`；周期性监听局域网 mDNS service 声明；解析 TXT 记录为 `DiscoveryDigest`；在 `core/s02_topology/discovery_table.hpp/cpp` 中实现 `DiscoveryTable`，以 NodeId 为键记录对端（不以 IP 为键，§5.2.1.4，DC-S02-002）；对端发现延迟 ≤1s（DFX 红线 §4.1.2）；DiscoveryTable 容量 ≤64；DiscoveryTable 维护在 Control Plane Thread 内，`std::mutex` 持锁 ≤100us（非热路径）
- **依赖任务**：CF1-TASK-013
- **输入**：design.md §2.1.2 L-02 功能点、spec §5.2.1.4 NodeID 解耦发现
- **输出**：`core/s02_topology/mdns_listener.hpp`、`core/s02_topology/mdns_listener.cpp`、`core/s02_topology/discovery_table.hpp`、`core/s02_topology/discovery_table.cpp`
- **验收标准**：
  1. mDNS 声明监听正确解析 TXT 记录为 DiscoveryDigest
  2. DiscoveryTable 以 NodeId 为键（不以 IP 为键）
  3. 对端发现延迟 ≤1s
  4. DiscoveryTable 容量 ≤64
  5. mutex 持锁 ≤100us（非热路径）
- **Design Contract**：DC-S02-002（发现以 NodeID 为键）
- **Acceptance Test**：发现记录 NodeID 索引测试
- **优先级**：P0
- **预估工作量**：2-3 小时
- **平台归属**：跨平台共享

### 3.4 CF1-TASK-015 LAN Broadcast / Manual Config fallback

- **任务标题**：实现 LAN Broadcast fallback 与 Manual Config fallback
- **任务描述**：在 `core/s02_topology/lan_broadcast_fallback.hpp/cpp` 中实现 `LanBroadcastFallback`；mDNS 服务不可用时降级为局域网广播发现报文，记录告警 `CFX-W-DISC-MDNS-UNAVAILABLE`；在 `core/s02_topology/manual_config_fallback.hpp/cpp` 中实现 `ManualConfigFallback`；mDNS + 广播均不可用时依赖运维配置的端点清单直接连接，记录告警 `CFX-W-DISC-MANUAL-FALLBACK`；跨子网端点不可见时记录告警 `CFX-W-DISC-CROSS-SUBNET`（第一版不跨子网发现）
- **依赖任务**：CF1-TASK-013
- **输入**：design.md §2.1.2 L-03/L-04 功能点、spec §5.2.3.1/§5.2.3.4 异常场景
- **输出**：`core/s02_topology/lan_broadcast_fallback.hpp/cpp`、`core/s02_topology/manual_config_fallback.hpp/cpp`
- **验收标准**：
  1. mDNS 不可用时降级 LAN Broadcast，告警 `CFX-W-DISC-MDNS-UNAVAILABLE`
  2. mDNS + 广播均不可用时降级 Manual Config，告警 `CFX-W-DISC-MANUAL-FALLBACK`
  3. 跨子网端点不可见告警 `CFX-W-DISC-CROSS-SUBNET`
  4. fallback 不影响已建立连接
- **Design Contract**：DC-S02-001（不依赖固定 IP 发现）
- **Acceptance Test**：mDNS 不可用降级测试
- **优先级**：P1
- **预估工作量**：2 小时
- **平台归属**：跨平台共享

### 3.5 CF1-TASK-016 Discovery Digest 构建与 TXT 记录

- **任务标题**：实现 Discovery Digest 构建与 mDNS TXT 记录编解码
- **任务描述**：在 `core/s02_topology/discovery_digest.hpp/cpp` 中实现 `DiscoveryDigest` 构建与 TXT 记录编解码；`DiscoveryDigest::fromIdentity(const NodeIdentity&)` 从完整 NodeIdentity 构建摘要（NodeID/Platform/Capabilities 指纹/Session Epoch/协议版本/Topology ID）；`toTxtRecord()` 编码为 mDNS TXT 记录键值对（紧凑格式，NodeID 为 hex 字符串）；`fromTxtRecord()` 从 TXT 记录解码；Capabilities 指纹为 SHA-256 摘要（std::array<u8,32>）；完整 NodeIdentity 在注册握手时交换，mDNS 仅携带摘要
- **依赖任务**：CF1-TASK-005、CF1-TASK-013
- **输入**：design.md §2.1.2 L-05 功能点、spec §6.7 Discovery Digest
- **输出**：`core/s02_topology/discovery_digest.hpp`、`core/s02_topology/discovery_digest.cpp`
- **验收标准**：
  1. `fromIdentity()` 正确构建 DiscoveryDigest
  2. `toTxtRecord()` / `fromTxtRecord()` 往返一致
  3. Capabilities 指纹为 SHA-256 摘要（32 字节）
  4. TXT 记录紧凑，DiscoveryDigest ≤256 字节
- **Design Contract**：DC-S02-001
- **Acceptance Test**：Digest 构建与 TXT 往返测试
- **优先级**：P1
- **预估工作量**：2 小时
- **平台归属**：跨平台共享

### 3.6 CF1-TASK-017 动态加入/离开+幂等+跨拓扑隔离+冲突检测

- **任务标题**：实现动态加入/离开、重复发现幂等、跨拓扑隔离、NodeID 冲突检测
- **任务描述**：在 `DiscoveryService` 中实现动态加入/离开与冲突检测；动态加入：新端点 mDNS 声明后纳入已发现列表，Topology ID 一致才纳入候选（§5.2.1.9，DC-S02-003）；动态离开：正常离开发布 goodbye，异常离开由 CF0 心跳超时判定（复用 CF0 §5.5.1.6）；重复发现幂等：同 NodeID 同 Epoch 仅更新 Endpoint Addresses，同 NodeID 更高 Epoch 标记新会话触发身份恢复（§5.2.1.7）；NodeID 冲突检测：两个端点声明相同 NodeID 时记录告警 `CFX-E-TOPO-NODEID-DUP` 并拒绝二者注册（§5.2.3.3）；禁止 IP 段扫描、禁止发现未声明端点
- **依赖任务**：CF1-TASK-014
- **输入**：design.md §2.1.2 L-06/L-07/L-09/L-10/L-11 功能点、spec §5.2.1/§5.2.3 发现规则
- **输出**：`DiscoveryService` 动态加入/离开/冲突检测实现
- **验收标准**：
  1. 新端点声明后纳入已发现列表，Topology ID 不一致忽略
  2. 同 NodeID 同 Epoch 仅更新地址，更高 Epoch 触发身份恢复
  3. NodeID 冲突告警 `CFX-E-TOPO-NODEID-DUP` 并拒绝注册
  4. goodbye 声明正确移除发现记录
  5. 禁止 IP 段扫描、禁止发现未声明端点
- **Design Contract**：DC-S02-002、DC-S02-003（跨拓扑隔离）
- **Acceptance Test**：动态加入/离开/冲突检测测试
- **优先级**：P1
- **预估工作量**：2-3 小时
- **平台归属**：跨平台共享

---

## 4. CF1-S03 Pairing / Registration

> 本组任务实现配对/注册 7 状态 FSM（AMEND-002）。五层分层不可跃迁。配对码经 Control Plane 传输，禁 mDNS 广播。

### 4.1 CF1-TASK-018 IPairingManager/IRegistrationManager/ITrustedNodeList 接口定义

- **任务标题**：定义 IPairingManager、IRegistrationManager、ITrustedNodeList 接口
- **任务描述**：在 `core/s02_topology/i_pairing_manager.hpp` 中定义 `IPairingManager`（`peerState/initiatePairing/handlePairingRequest/trustedNodes/isTrusted/onStateTransition/onPairingFailed`）；在 `core/s02_topology/i_registration_manager.hpp` 中定义 `IRegistrationManager`（`initiateRegistration/handleRegistrationRequest/registrationState/isRegistered/isMember/onStateTransition`）；在 `core/s02_topology/i_trusted_node_list.hpp` 中定义 `ITrustedNodeList`（`add/remove/contains/entries/updateLastSeenEpoch/persist/load`）；全部可失败操作返回 `std::expected`，回调使用 concepts 约束，批量传参使用 `std::span`
- **依赖任务**：CF1-TASK-003
- **输入**：design.md §2.2.2.3/§2.2.2.4/§2.2.2.9 接口定义、spec §5.3/§6.6 规则
- **输出**：`i_pairing_manager.hpp`、`i_registration_manager.hpp`、`i_trusted_node_list.hpp` 新增
- **验收标准**：
  1. 三个接口含全部方法，签名对齐 C++20 设施
  2. `initiatePairing` 配对码参数为 `std::span<const u8>`
  3. `isRegistered`/`isMember` 供 CF0 HandoffOrchestrator 校验对端状态
  4. 接口无平台头文件依赖
- **Design Contract**：DC-S03-001、DC-S03-002、DC-S03-003
- **Acceptance Test**：接口契约测试
- **优先级**：P0
- **预估工作量**：1-2 小时
- **平台归属**：跨平台共享

### 4.2 CF1-TASK-019 PairingFsm 7 状态 FSM 实现

- **任务标题**：实现 PairingFsm 7 状态 FSM（AMEND-002）
- **任务描述**：在 `core/s02_topology/pairing_fsm.hpp/cpp` 中实现 `PairingFsm`；状态集 7 成功态 + 2 失败态（DISCOVERED/UNTRUSTED/PAIRING/TRUSTED/REGISTERING/REGISTERED/MEMBER/REJECTED/ROLLBACK），任意时刻唯一（§5.3.4.1）；成功路径：DISCOVERED→UNTRUSTED→PAIRING→TRUSTED→REGISTERING→REGISTERED→MEMBER；失败路径：任一阶段→REJECTED/ROLLBACK→保持原有安全状态；FSM 实例由 FSM Thread 独占（单线程所有权，复用 CF0 契约⑦）；转移路径内禁 I/O、禁锁等待、禁异常（≤1ms）；非法转移告警 `CFX-E-PAIR-ILLEGAL-TRANS` 且 FSM 保持原态
- **依赖任务**：CF1-TASK-018
- **输入**：design.md §2.1.3.2 状态机设计、spec §5.3.4/AMEND-002 规则
- **输出**：`core/s02_topology/pairing_fsm.hpp`、`core/s02_topology/pairing_fsm.cpp`
- **验收标准**：
  1. 9 态完整（7 成功 + 2 失败），任意时刻唯一
  2. 成功路径 6 步转移全部实现
  3. 失败路径任一阶段→REJECTED/ROLLBACK→保持安全状态
  4. 非法转移告警 `CFX-E-PAIR-ILLEGAL-TRANS` 且保持原态
  5. FSM 单线程所有权，转移 ≤1ms，禁 I/O/锁/异常
- **Design Contract**：DC-S03-003（五层分层不可跃迁）
- **Acceptance Test**：FSM 9 态转移测试
- **优先级**：P0
- **预估工作量**：2-3 小时
- **平台归属**：跨平台共享

### 4.3 CF1-TASK-020 五层分层不可跃迁守卫

- **任务标题**：实现五层分层不可跃迁守卫（Discovery ≠ Trust ≠ Pairing ≠ Registration ≠ Membership）
- **任务描述**：在 `PairingFsm` 中实现五层分层不可跃迁守卫；Discovery ≠ Trust ≠ Pairing ≠ Registration ≠ Topology Membership 五层分层；禁止跃迁（如 DISCOVERED→MEMBER 直接跳步）；发现不建立信任（§5.3.4.2 规则 2-3，Discovery 层不写入 Trusted List）；进入 MEMBER 前必须完成 DISCOVERED→PAIRING→REGISTERING→REGISTERED 全部成功（§5.3.4.2 规则 5）；跃迁请求被拒绝并告警 `CFX-E-PAIR-ILLEGAL-TRANS`
- **依赖任务**：CF1-TASK-019
- **输入**：design.md §2.1.3.2 五层分层、spec §5.3.4.2 规则 2-3/5-6
- **输出**：`PairingFsm` 五层分层守卫实现
- **验收标准**：
  1. DISCOVERED→MEMBER 跃迁被拒绝
  2. Discovery 层不写入 Trusted List（发现不建立信任）
  3. 进入 MEMBER 前全部 6 步成功校验
  4. 跃迁拒绝告警 `CFX-E-PAIR-ILLEGAL-TRANS`
- **Design Contract**：DC-S03-003（五层分层不可跃迁）
- **Acceptance Test**：跃迁拒绝测试
- **优先级**：P0
- **预估工作量**：1-2 小时
- **平台归属**：跨平台共享

### 4.4 CF1-TASK-021 TrustedNodeList 持久化实现

- **任务标题**：实现 TrustedNodeList 可信端点列表持久化
- **任务描述**：在 `core/s02_topology/trusted_node_list.hpp/cpp` 中实现 `TrustedNodeList`（实现 `ITrustedNodeList`）；`add()` 配对成功时添加可信端点 NodeID 并立即持久化（防崩溃丢失）；`remove()` 运维显式移除；`contains()` 校验是否可信；`updateLastSeenEpoch()` 更新对端最近观测 Epoch（§5.5.1.9）；`persist()/load()` JSON 持久化；持久化失败告警 `CFX-E-PAIR-TRUST-PERSIST-FAIL`，加载损坏清空 + 告警 `CFX-W-PAIR-TRUST-CORRUPT`（不影响本端 NodeID）；Trusted List 容量 ≤32；维护在 FSM Thread 内，`std::mutex` 持锁 ≤100us
- **依赖任务**：CF1-TASK-018
- **输入**：design.md §2.2.2.9 TrustedNodeList 接口、spec §6.6 Trusted Node List 持久化
- **输出**：`core/s02_topology/trusted_node_list.hpp`、`core/s02_topology/trusted_node_list.cpp`
- **验收标准**：
  1. 配对成功后立即持久化，崩溃重启不丢失
  2. `contains()` 校验正确，仅可信端点可完成注册
  3. 持久化失败告警 `CFX-E-PAIR-TRUST-PERSIST-FAIL`
  4. 加载损坏清空 + 告警 `CFX-W-PAIR-TRUST-CORRUPT`，不影响本端 NodeID
  5. Trusted List 容量 ≤32，mutex 持锁 ≤100us
- **Design Contract**：DC-S03-001（配对前置）
- **Acceptance Test**：持久化往返测试、损坏恢复测试
- **优先级**：P0
- **预估工作量**：2 小时
- **平台归属**：跨平台共享

### 4.5 CF1-TASK-022 配对码经 Control Plane 传输（禁广播）

- **任务标题**：实现配对码经 Control Plane 点对点传输（禁 mDNS 广播）
- **任务描述**：在 `core/s02_topology/pairing_manager.hpp/cpp` 中实现 `PairingManager`（实现 `IPairingManager`）；`initiatePairing(peerNodeId, pairingCode)` 配对码经 `IControlPlaneChannel.send()` 点对点传输至对端（禁 mDNS 广播，§5.3.1.12）；`handlePairingRequest()` 处理对端配对请求，配对码匹配则对端 NodeID 加入 Trusted List 并持久化，不匹配则拒绝 + 安全告警 `CFX-E-PAIR-CODE-MISMATCH`；配对码明文广播尝试告警 `CFX-E-PAIR-CODE-BROADCAST`；配对码经 Control Plane Thread 发送，状态机维护在 FSM Thread
- **依赖任务**：CF1-TASK-021
- **输入**：design.md §2.1.2 M-04 功能点、spec §5.3.1.12 配对码传输规则
- **输出**：`core/s02_topology/pairing_manager.hpp`、`core/s02_topology/pairing_manager.cpp`
- **验收标准**：
  1. 配对码经 `IControlPlaneChannel.send()` 点对点传输，不经 mDNS 广播
  2. 配对码匹配 → 对端 NodeID 加入 Trusted List 并持久化
  3. 配对码不匹配 → 拒绝 + 告警 `CFX-E-PAIR-CODE-MISMATCH`
  4. 配对码明文广播尝试告警 `CFX-E-PAIR-CODE-BROADCAST`
- **Design Contract**：DC-S03-001（配对前置）
- **Acceptance Test**：配对码传输与匹配测试
- **优先级**：P0
- **预估工作量**：2 小时
- **平台归属**：跨平台共享

### 4.6 CF1-TASK-023 HandshakeOrchestrator 注册握手编排

- **任务标题**：实现 HandshakeOrchestrator 注册握手编排（版本+能力+拓扑同步）
- **任务描述**：在 `core/s02_topology/handshake_orchestrator.hpp/cpp` 中实现 `HandshakeOrchestrator`；`initiateRegistration(peerNodeId)` 前置校验：对端在 Trusted List 且处于 TRUSTED 态（DC-S03-001），未配对拒绝 `CFX-E-PAIR-UNPAIRED`；注册前置发现校验：必须先经发现记录对端 NodeIdentity 摘要（§5.3.1.1），未发现拒绝 `CFX-E-REG-UNDISCOVERED`；原子完成四步：(1) 协议版本协商（复用 CF0 §5.5.1.7）(2) Capabilities 交换与兼容性判定 (3) Topology Membership 同步 (4) 完整 NodeIdentity 交换；任一步失败整体回滚，不产生半注册状态（§5.3.1.9）；注册握手超时 2s（DFX 红线 §4.1.3）
- **依赖任务**：CF1-TASK-019、CF1-TASK-022
- **输入**：design.md §2.1.2 M-06~M-10 功能点、spec §5.3.1 注册握手规则
- **输出**：`core/s02_topology/handshake_orchestrator.hpp`、`core/s02_topology/handshake_orchestrator.cpp`
- **验收标准**：
  1. 注册前置校验 Trusted List + 发现记录，未配对/未发现拒绝
  2. 四步原子完成（版本+能力+拓扑+身份交换）
  3. 任一步失败整体回滚，无半注册状态
  4. 协议版本不兼容拒绝 `CFX-E-PROTO-VER`
  5. Capabilities 不兼容拒绝 `CFX-E-REG-CAP-INCOMPAT`
  6. Topology ID 不一致拒绝 `CFX-E-REG-TOPOID-MISMATCH`
  7. 注册握手超时 2s
- **Design Contract**：DC-S03-002（注册原子性）
- **Acceptance Test**：注册握手四步原子完成测试
- **优先级**：P0
- **预估工作量**：3-4 小时
- **平台归属**：跨平台共享

### 4.7 CF1-TASK-024 注册原子性+幂等+失败回滚

- **任务标题**：实现注册原子性、幂等与失败回滚保持安全状态
- **任务描述**：在 `HandshakeOrchestrator` 中实现注册原子性、幂等与失败回滚；注册原子性：要么全部四步成功，要么任一失败整体回滚至旧状态（§5.3.1.9，DC-S03-002）；注册幂等：同 NodeID 同 Epoch 返回已注册应答，同 NodeID 更高 Epoch 更新会话信息触发身份恢复（§5.3.1.10）；失败回滚：回滚该次流程全部副作用，Trusted List 与 Topology Membership 集合不变（§5.3.4.2 规则 4）；注册中途链路断开告警 `CFX-E-REG-LINK-BROKEN` 并整体回滚；禁止未注册端点参与 Handoff（§5.3.1.11，CF0 HandoffOrchestrator 校验 `isRegistered()`）
- **依赖任务**：CF1-TASK-023
- **输入**：design.md §2.1.2 M-11~M-14 功能点、spec §5.3.1.9/§5.3.1.10/§5.3.1.11
- **输出**：`HandshakeOrchestrator` 原子性/幂等/回滚实现
- **验收标准**：
  1. 注册原子性：全或无，无半注册状态
  2. 注册幂等：同 NodeID 同 Epoch 返回已注册应答
  3. 失败回滚：Trusted List 与 Topology Membership 集合不变
  4. 注册中途链路断开告警 `CFX-E-REG-LINK-BROKEN` 并回滚
  5. 未注册端点参与 Handoff 被拒绝，告警 `CFX-W-HANDOFF-UNREGISTERED-PEER`
- **Design Contract**：DC-S03-002（注册原子性全或无）
- **Acceptance Test**：原子性/幂等/回滚测试
- **优先级**：P1
- **预估工作量**：2-3 小时
- **平台归属**：跨平台共享

---

## 5. CF1-S04 Topology Membership Mgmt

> 本组任务实现动态拓扑成员管理与 TopologyVersion Atomic Commit（AMEND-003）。含 D-TOPO-ATOMIC-005 严格协议状态机与 D-TOPO-COORD-005 Coordinator Identity Lifecycle。不引入 Raft/Paxos/Consensus。

### 5.1 CF1-TASK-025 IMembershipManager/ITopologyVersionManager 接口定义

- **任务标题**：定义 IMembershipManager、ITopologyVersionManager 接口
- **任务描述**：在 `core/s02_topology/i_membership_manager.hpp` 中定义 `IMembershipManager`（`addMember/removeMember/onMemberDisconnect/members/nodeCount/segmentCount/isCircularHandoffEnabled/isDegraded/onMembershipChange/onDegraded`）；在 `core/s02_topology/i_topology_version_manager.hpp` 中定义 `ITopologyVersionManager`（`currentVersion/proposeChange/handleIncomingUpdate/commitState/onVersionCommitted/onStaleUpdateRejected/onCommitRolledBack`）；全部可失败操作返回 `std::expected`，回调使用 concepts 约束
- **依赖任务**：CF1-TASK-003
- **输入**：design.md §2.2.2.5/§2.2.2.6 接口定义、spec §5.4 拓扑成员规则
- **输出**：`i_membership_manager.hpp`、`i_topology_version_manager.hpp` 新增
- **验收标准**：
  1. 两个接口含全部方法，签名对齐 C++20 设施
  2. `nodeCount()`/`segmentCount()` 区分运行态/配置态
  3. `isCircularHandoffEnabled()`/`isDegraded()` 供 DEGRADED 判定
  4. `proposeChange()` 触发 Atomic Commit 流程
  5. 接口无平台头文件依赖
- **Design Contract**：DC-S04-001~DC-S04-009
- **Acceptance Test**：接口契约测试
- **优先级**：P0
- **预估工作量**：1-2 小时
- **平台归属**：跨平台共享

### 5.2 CF1-TASK-026 MembershipManager 动态成员管理

- **任务标题**：实现 MembershipManager 动态成员管理
- **任务描述**：在 `core/s02_topology/membership_manager.hpp/cpp` 中实现 `MembershipManager`（实现 `IMembershipManager`）；`addMember()` 注册成功后正式加入拓扑，Topology Membership 同步至全拓扑，全拓扑 ≤1s 内一致（§5.4.1.1）；`removeMember()` 正常离开发布 goodbye + 通知全拓扑（§5.4.1.2）；`onMemberDisconnect()` 异常断线离开由心跳超时判定，该方向 Handoff 暂停（§5.4.1.3）；邻居关系维护：每端点最多两个邻居（左/右），成员变更时重新校验（§5.4.1.6）；禁止多路径拓扑（树状/网状），成员关系必须线性序列含回环（§5.4.1.10）；禁止未通知的成员变更（§5.4.1.11）；与 CF0 `ITopologyManager.currentView()` 保持一致（§5.4.1.9）
- **依赖任务**：CF1-TASK-025
- **输入**：design.md §2.1.2 N-01~N-06/N-11~N-13 功能点、spec §5.4.1 拓扑成员规则
- **输出**：`core/s02_topology/membership_manager.hpp`、`core/s02_topology/membership_manager.cpp`
- **验收标准**：
  1. 成员加入后全拓扑 ≤1s 内一致
  2. 正常离开发布 goodbye + 通知全拓扑
  3. 异常断线该方向 Handoff 暂停
  4. 邻居数量 ≤2，多路径拓扑拒绝 `CFX-E-TOPO-MULTIPATH`
  5. 与 CF0 `ITopologyManager.currentView()` 保持一致
- **Design Contract**：DC-S04-001（循环拓扑有效运行态）
- **Acceptance Test**：动态成员管理测试
- **优先级**：P0
- **预估工作量**：3-4 小时
- **平台归属**：跨平台共享

### 5.3 CF1-TASK-027 NodeCountGuard+DEGRADED 判定+循环拓扑维护

- **任务标题**：实现 NodeCountGuard、DEGRADED 判定与循环拓扑回环维护
- **任务描述**：在 `core/s02_topology/node_count_guard.hpp/cpp` 中实现 `NodeCountGuard`；`isCircularHandoffEnabled()` Node Count ≥3 时启用循环 Handoff（§5.4.5 规则 1）；`isDegraded()` Node Count ∈ {1,2} 时 DEGRADED + 循环 Handoff 禁用；Node Count=2 告警 `CFX-W-TOPO-DEGRADED-NON-CIRCULAR`，Node Count=1 告警 `CFX-W-TOPO-DEGRADED-SINGLE`；在 `core/s02_topology/circular_topology_maintainer.hpp/cpp` 中实现 `CircularTopologyMaintainer`；循环拓扑回环维护：最左端左邻居=最右端，最右端右邻居=最左端（复用 CF0 契约③，§5.4.1.5）；Segment Count ≥2 为配置态约束，成员离开 Node Count 下降但 Segment Count 不变（§5.4.1.8）
- **依赖任务**：CF1-TASK-026
- **输入**：design.md §2.1.2 N-07 功能点、spec §5.4.5/§5.4.1.8 DEGRADED 规则
- **输出**：`node_count_guard.hpp/cpp`、`circular_topology_maintainer.hpp/cpp`
- **验收标准**：
  1. Node Count ≥3 时 `isCircularHandoffEnabled()` 返回 true
  2. Node Count=2 告警 `CFX-W-TOPO-DEGRADED-NON-CIRCULAR`，循环 Handoff 禁用
  3. Node Count=1 告警 `CFX-W-TOPO-DEGRADED-SINGLE`
  4. 循环拓扑最左端左邻居=最右端，最右端右邻居=最左端
  5. Segment Count 不因成员离开而下降
- **Design Contract**：DC-S04-001（循环拓扑有效运行态 Node Count ≥3）
- **Acceptance Test**：DEGRADED 判定与循环拓扑测试
- **优先级**：P1
- **预估工作量**：2 小时
- **平台归属**：跨平台共享

### 5.4 CF1-TASK-028 TopologyVersionManager 版本覆盖规则

- **任务标题**：实现 TopologyVersionManager 版本覆盖规则（AMEND-003）
- **任务描述**：在 `core/s02_topology/topology_version_manager.hpp/cpp` 中实现 `TopologyVersionManager`（实现 `ITopologyVersionManager`）；`currentVersion()` 返回当前 `TopologyVersion`（value:u64 + topologyId + lastCommitAt + commitState）；`handleIncomingUpdate()` 版本覆盖规则：incoming < current → reject + 告警 `CFX-W-TOPO-STALE-VERSION`（§5.4.4.3 规则 2）；incoming == current → idempotent；incoming > current → validate → atomic commit（§5.4.4.3 规则 3）；`current_version` 不因过期消息而降低（§5.4.4.3 规则 6）；TopologyVersion 单调递增不得回跳；维护在 FSM Thread 内
- **依赖任务**：CF1-TASK-025
- **输入**：design.md §2.1.2 N-09 功能点、spec §5.4.4.2/§5.4.4.3 版本覆盖规则
- **输出**：`core/s02_topology/topology_version_manager.hpp`、`core/s02_topology/topology_version_manager.cpp`
- **验收标准**：
  1. incoming < current → reject + 告警 `CFX-W-TOPO-STALE-VERSION`
  2. incoming == current → idempotent
  3. incoming > current → validate → atomic commit
  4. `current_version` 不因过期消息而降低
  5. TopologyVersion 单调递增不得回跳
- **Design Contract**：DC-S04-002（过期更新拒绝）
- **Acceptance Test**：版本覆盖规则测试
- **优先级**：P0
- **预估工作量**：2-3 小时
- **平台归属**：跨平台共享

### 5.5 CF1-TASK-029 Topology Atomic Commit 严格协议状态机（D-TOPO-ATOMIC-005）

- **任务标题**：实现 Topology Atomic Commit 严格协议状态机（D-TOPO-ATOMIC-005，v3 BLOCKER-003）
- **任务描述**：在 `TopologyVersionManager` 中实现 D-TOPO-ATOMIC-005 严格协议状态机；状态流转：CURRENT N → PROPOSED N+1 → VALIDATED N+1 → PREPARED N+1 → ACTIVATION AUTHORIZED → LOCAL ATOMIC SNAPSHOT SWAP → ACTIVE N+1；`TopologyCommitState` 扩展 `Prepared`/`ActivationAuthorized` 两态；Proposal ≠ Active / Prepare ≠ Active / ACK ≠ Active；仅唯一 Coordinator 发出 Activate/Commit Authorization 后节点才能将 N+1 设为 Active；Coordinator Failure 时其他节点禁止自主 Commit；Coordinator Failure 必须保持旧版本 N 为 authoritative baseline；新会话或恢复后必须重新验证并重新提交；stale version 必须被拒绝；不允许两个 Active TopologyVersion Authority；Atomic Commit 超时 1s（全拓扑 ACK 收敛）；任一端点失败/超时 → 整体回滚至旧版本（全或无语义，§5.4.4.3 规则 5）
- **依赖任务**：CF1-TASK-028
- **输入**：design.md §2.6.7 D-TOPO-ATOMIC-005 严格协议状态机、spec §5.4.4.3 规则 4-5
- **输出**：`TopologyVersionManager` 严格协议状态机实现、`TopologyCommitState` 扩展
- **验收标准**：
  1. 协议状态机 7 态流转正确（CURRENT→PROPOSED→VALIDATED→PREPARED→ACTIVATION AUTHORIZED→LOCAL ATOMIC SNAPSHOT SWAP→ACTIVE）
  2. Proposal ≠ Active / Prepare ≠ Active / ACK ≠ Active
  3. 仅 Coordinator 发出 Activate Authorization 后节点才能 Active
  4. Coordinator Failure 时其他节点禁止自主 Commit，旧版本 N 保持 authoritative
  5. stale version 被拒绝
  6. 不允许两个 Active TopologyVersion Authority
  7. 全或无语义：任一端点失败整体回滚
  8. Atomic Commit 超时 1s
- **Design Contract**：DC-S04-003（原子提交全或无）、DC-S04-008（D-TOPO-ATOMIC-005）
- **Acceptance Test**：AT-BLOCKER-003-1~7（7 条验收测试）
- **优先级**：P0
- **预估工作量**：3-4 小时
- **平台归属**：跨平台共享

### 5.6 CF1-TASK-030 Coordinator Identity Lifecycle（D-TOPO-COORD-005）

- **任务标题**：实现 Coordinator Identity Lifecycle（D-TOPO-COORD-005，v3 BLOCKER-004，v4 语义统一）
- **任务描述**：在 `core/s02_topology/coordinator_identity_manager.hpp/cpp` 中实现 `CoordinatorIdentityManager`；在 `core/s02_topology/coordinator_health_monitor.hpp/cpp` 中实现 `CoordinatorHealthMonitor`；Coordinator 由运维配置预先指定（TopologyAuthorityNodeID），第一版**禁止自动 Coordinator Election**（D-TOPO-COORD-005）；Coordinator 身份验证：NodeID Check + Topology Authority Membership Check +（CF8 启用时）Signature Check；Coordinator NodeID 必须属于 Topology Authority；配置缺失时启动失败（不按字典序自动推导）；`CoordinatorHealthMonitor` 检测 Coordinator 离线（进程崩溃/网络分区/FSM Thread 阻塞）；Coordinator 离线 → `commitState()` 返回 `Locked` → 新 Proposal 一律 REJECT（告警 `CFX-W-TOPO-COORD-OFFLINE`）→ 旧版本 N 保持 authoritative baseline；Coordinator 恢复后从旧版本 N 继续，不自动重提交前作废的 N+1 提案；禁止多个节点同时声称 Coordinator，检测到多节点同时声称告警 `CFX-E-TOPO-COORD-DUPLICATE-CLAIM`；不引入 Raft/Paxos/Consensus
- **依赖任务**：CF1-TASK-029
- **输入**：design.md §2.6.8 D-TOPO-COORD-005、spec §5.4.4 Coordinator Identity Lifecycle
- **输出**：`coordinator_identity_manager.hpp/cpp`、`coordinator_health_monitor.hpp/cpp`、`TopologyCommitState::Locked` 态
- **验收标准**：
  1. Coordinator 由运维配置预先指定，无自动 Election 日志（`election=AUTO_FORBIDDEN`）
  2. Coordinator 身份验证：NodeID + Topology Authority Membership +（CF8）Signature
  3. 配置缺失时启动失败，不按字典序自动推导
  4. Coordinator 离线 → `commitState()==Locked` → 新 Proposal REJECT → 旧版本 N 保持 authoritative
  5. Coordinator 恢复后从旧版本 N 继续，不自动重提交前作废的 N+1
  6. 多节点同时声称 Coordinator 告警 `CFX-E-TOPO-COORD-DUPLICATE-CLAIM`
  7. 无 Raft/Paxos/Consensus 库依赖，无多数派投票逻辑
- **Design Contract**：DC-S04-004~DC-S04-007、DC-S04-009（D-TOPO-COORD-005）
- **Acceptance Test**：AT-BLOCKER-002-1~6、AT-BLOCKER-004-1~6（12 条验收测试）
- **优先级**：P0
- **预估工作量**：3-4 小时
- **平台归属**：跨平台共享

### 5.7 CF1-TASK-031 成员变更通知广播

- **任务标题**：实现成员变更通知广播（经 Control Plane 可靠有序传输）
- **任务描述**：在 `core/s02_topology/member_change_notifier.hpp/cpp` 中实现 `MemberChangeNotifier`；成员加入/离开事件生成 `MembershipChangeNotification` 报文；经 CF0 `IControlPlaneChannel.send()` 可靠有序传输至全拓扑；全拓扑 ≤1s 内收悉（DFX 红线 §4.1.6）；任何成员加入/离开必须通知全拓扑，禁止静默变更（§5.4.1.11）；报文含 `SessionFence` 三元组供消息准入检查；报文归属 Control Plane Thread，不侵入 Input Plane
- **依赖任务**：CF1-TASK-026
- **输入**：design.md §2.1.2 N-04/N-13 功能点、spec §5.4.1.4 成员变更通知
- **输出**：`core/s02_topology/member_change_notifier.hpp`、`core/s02_topology/member_change_notifier.cpp`
- **验收标准**：
  1. 成员变更通知经 Control Plane 可靠有序传输
  2. 全拓扑 ≤1s 内收悉
  3. 禁止静默变更，任何加入/离开必须通知
  4. 报文含 SessionFence 三元组
  5. 报文归属 Control Plane Thread，不侵入 Input Plane
- **Design Contract**：DC-S04-001
- **Acceptance Test**：成员变更通知广播测试
- **优先级**：P1
- **预估工作量**：2 小时
- **平台归属**：跨平台共享

---

## 6. CF1-S05 Session Epoch & Reconnection

> 本组任务实现 (NodeID, SessionEpoch, SessionInstanceID) 三元栅栏（AMEND-004）与断线重连身份恢复。含 Bootstrap/Established Session 两级 Message Admission（BLOCKER-001）。

### 6.1 CF1-TASK-032 ISessionFence/IIdentityRecoveryManager 接口定义

- **任务标题**：定义 ISessionFence、IIdentityRecoveryManager 接口
- **任务描述**：在 `core/s02_topology/i_session_fence.hpp` 中定义 `ISessionFence`（`currentFence/newSession/checkIncoming/peerFence/updatePeerFence/onStaleEpochRejected/onInstanceConflict/onNewSessionAccepted`）；在 `core/s02_topology/i_identity_recovery_manager.hpp` 中定义 `IIdentityRecoveryManager`（`initiateRecovery/handleRecoveryRequest/recoveryState/onRecoveryCompleted/onRecoveryFailed`）；`checkIncoming()` 返回 `std::expected<SessionFenceVerdict, CfxError>`；全部可失败操作返回 `std::expected`，回调使用 concepts 约束
- **依赖任务**：CF1-TASK-003
- **输入**：design.md §2.2.2.7/§2.2.2.8 接口定义、spec §5.5.4/§5.5.1 规则
- **输出**：`i_session_fence.hpp`、`i_identity_recovery_manager.hpp` 新增
- **验收标准**：
  1. 两个接口含全部方法，签名对齐 C++20 设施
  2. `checkIncoming()` 返回 `std::expected<SessionFenceVerdict, CfxError>`
  3. `newSession()` 返回新 `SessionFence`（Epoch 递增 + 新 InstanceID）
  4. 接口无平台头文件依赖
- **Design Contract**：DC-S05-001~DC-S05-004、EPOCH-001~006
- **Acceptance Test**：接口契约测试
- **优先级**：P0
- **预估工作量**：1 小时
- **平台归属**：跨平台共享

### 6.2 CF1-TASK-033 SessionFence 三元栅栏实现

- **任务标题**：实现 SessionFence 三元栅栏（NodeID + Epoch + InstanceID）
- **任务描述**：在 `core/s02_topology/session_fence.hpp/cpp` 中实现 `SessionFence`（实现 `ISessionFence`）；`currentFence()` 返回本端当前三元组（NodeID + SessionEpoch + SessionInstanceId）；`newSession()` 启动/重连时调用，Epoch 递增 + 生成新 InstanceID（UUIDv4）；`checkIncoming(nodeId, epoch, instanceId)` 消息准入检查流程：NodeID Check → Epoch Check → InstanceID Check → ACCEPT/REJECT；`peerFence()`/`updatePeerFence()` 维护对端已记录栅栏；消息准入检查在 FSM Thread 内执行，禁锁等待、禁 I/O、禁异常（≤1ms）；`std::atomic<SessionEpoch>` 跨线程读快照
- **依赖任务**：CF1-TASK-032
- **输入**：design.md §2.2.2.7 SessionFence 接口、spec §5.5.4.1 三元栅栏
- **输出**：`core/s02_topology/session_fence.hpp`、`core/s02_topology/session_fence.cpp`
- **验收标准**：
  1. `currentFence()` 返回三元组（NodeID + Epoch + InstanceID）
  2. `newSession()` Epoch 递增 + 新 InstanceID
  3. `checkIncoming()` 流程：NodeID → Epoch → InstanceID Check
  4. 消息准入检查 ≤1ms，禁锁/禁 I/O/禁异常
  5. `std::atomic<SessionEpoch>` `is_lock_free()` 为 true
- **Design Contract**：DC-S05-001（低 Epoch 消息拒绝）
- **Acceptance Test**：三元栅栏检查测试
- **优先级**：P0
- **预估工作量**：2-3 小时
- **平台归属**：跨平台共享

### 6.3 CF1-TASK-034 EPOCH-001~006 六条测试契约实现

- **任务标题**：实现 EPOCH-001~006 六条测试契约
- **任务描述**：在 `SessionFence` 中实现 EPOCH-001~006 六条契约；EPOCH-001：SessionEpoch 单调递增不得回跳（已在 CF1-TASK-010 实现）；EPOCH-002：`checkEpoch()` 低 Epoch 消息拒绝，记录告警 `CFX-W-SESSION-STALE-EPOCH`，recorded_epoch 不因过期消息降低；EPOCH-003：`checkInstanceId()` 同 Epoch 不同 InstanceID 冲突检测，记录告警 `CFX-E-SESSION-INSTANCE-CONFLICT`，要求人工排查不自动裁决；EPOCH-004：`acceptNewSession()` 新 Epoch 接受新会话淘汰旧 Session；EPOCH-005：旧 Session 不修改五类关键状态（Topology/Trust/Ownership/EndpointAddress/HandoffState）；EPOCH-006：Reconnect 不重新 Pairing（在 IdentityRecoveryManager 实现）
- **依赖任务**：CF1-TASK-033
- **输入**：design.md §2.7.3 EPOCH-001~006 实现方案、spec §5.5.4.2 规则 1-6
- **输出**：`SessionFence` EPOCH-001~006 契约实现
- **验收标准**：
  1. EPOCH-001：SessionEpoch 单调递增不得回跳
  2. EPOCH-002：低 Epoch 消息拒绝 + 告警 `CFX-W-SESSION-STALE-EPOCH`
  3. EPOCH-003：同 Epoch 不同 InstanceID 冲突 + 告警 `CFX-E-SESSION-INSTANCE-CONFLICT`，不自动裁决
  4. EPOCH-004：新 Epoch 接受新会话淘汰旧 Session
  5. EPOCH-005：旧 Session 不修改五类关键状态
  6. EPOCH-006：Reconnect 不重新 Pairing
- **Design Contract**：EPOCH-001~006、DC-S05-001、DC-S05-002
- **Acceptance Test**：EPOCH-001~006 六条契约测试
- **优先级**：P0
- **预估工作量**：2-3 小时
- **平台归属**：跨平台共享

### 6.4 CF1-TASK-035 Bootstrap/Established Session 两级 Message Admission

- **任务标题**：实现 Bootstrap/Established Session 两级 Message Admission（v2 BLOCKER-001）
- **任务描述**：在 `core/s02_topology/bootstrap_admission.hpp/cpp` 中实现 `BootstrapAdmission`；两级 Message Admission 边界：Bootstrap 类消息（DiscoveryAnnouncement / PairingRequest / PairingResponse）允许未知 NodeID，走独立 Trust Gate（`BootstrapAdmission.check()`），不经过 `SessionFence.checkIncoming()` 的 NodeID Check；Established Session 类消息（Registration / MembershipChange / IdentityRecovery / Goodbye / Topology update）才执行 NodeID→Epoch→InstanceID 三元 Fence（`SessionFence.checkIncoming()`）；禁止 SessionFence 将首次 Pairing 锁死；配对成功后对端 NodeID 写入 Trusted List，后续 Established Session 类消息进入三元 Fence；未知对端 Established Session 消息拒绝 + 告警 `CFX-W-SESSION-UNKNOWN-NODE`
- **依赖任务**：CF1-TASK-033
- **输入**：design.md §2.11.8.1 BLOCKER-001 修复、spec §5.5.4 Message Admission
- **输出**：`core/s02_topology/bootstrap_admission.hpp`、`core/s02_topology/bootstrap_admission.cpp`
- **验收标准**：
  1. Bootstrap 类消息（DiscoveryAnnouncement/PairingRequest/PairingResponse）允许未知 NodeID
  2. Bootstrap 类消息不经过 `SessionFence.checkIncoming()` NodeID Check
  3. Established Session 类消息强制三元 Fence
  4. 未知对端 Established Session 消息拒绝 + 告警 `CFX-W-SESSION-UNKNOWN-NODE`
  5. 配对成功后切换至 Established Session 通道
- **Design Contract**：DC-S05-004（Bootstrap/Established Session 两级 Admission）
- **Acceptance Test**：AT-BLOCKER-001-1~4（4 条验收测试）
- **优先级**：P0
- **预估工作量**：2 小时
- **平台归属**：跨平台共享

### 6.5 CF1-TASK-036 IdentityRecoveryManager 断线重连身份恢复

- **任务标题**：实现 IdentityRecoveryManager 断线重连身份恢复
- **任务描述**：在 `core/s02_topology/identity_recovery_manager.hpp/cpp` 中实现 `IdentityRecoveryManager`（实现 `IIdentityRecoveryManager`）；`initiateRecovery(peerNodeId)` 断线重连成功后调用，不重新配对（EPOCH-006）；`handleRecoveryRequest()` 处理对端恢复请求；身份恢复流程：校验 NodeID ∈ Trusted List（不在则拒绝 `CFX-E-RECOVERY-UNTRUSTED`）→ 携带 NodeID + 新 Session Epoch + 新 InstanceID → 恢复拓扑成员关系；身份恢复 ≤3s 完成（DFX 红线 §4.1.4）；恢复期间 Handoff 暂停（§5.5.1.5）；不恢复断线前按下状态（§5.5.1.8）；身份恢复幂等；对端不可达告警 `CFX-E-RECOVERY-UNREACHABLE`，恢复超时告警 `CFX-E-RECOVERY-TIMEOUT`；CF0 `LinkSupervisor` 重连成功后通过回调触发身份恢复（不修改 CF0 LinkSupervisor 接口）
- **依赖任务**：CF1-TASK-033、CF1-TASK-035
- **输入**：design.md §2.2.2.8 IdentityRecovery 接口、spec §5.5.1 断线重连规则
- **输出**：`core/s02_topology/identity_recovery_manager.hpp`、`core/s02_topology/identity_recovery_manager.cpp`
- **验收标准**：
  1. 身份恢复不重新配对（EPOCH-006）
  2. NodeID 不在 Trusted List 拒绝 `CFX-E-RECOVERY-UNTRUSTED`
  3. 身份恢复 ≤3s 完成
  4. 恢复期间 Handoff 暂停
  5. 不恢复断线前按下状态
  6. 身份恢复幂等
  7. Safety Invariant 全程成立
- **Design Contract**：DC-S05-003（重连不破坏 Safety Invariant）、EPOCH-006
- **Acceptance Test**：断线重连身份恢复测试
- **优先级**：P1
- **预估工作量**：2-3 小时
- **平台归属**：跨平台共享

---

## 7. 单元测试

> 本组任务实现 CF1 核心模块单元测试，与实现同步。复用 CF0 CTest 框架。

### 7.1 CF1-TASK-037 NodeIdentity 领域模型单元测试

- **任务标题**：编写 NodeIdentity 七要素领域模型单元测试
- **任务描述**：在 `tests/test_cf1_node_identity.cpp` 中编写 NodeIdentity 单元测试；NodeIdentity 七要素创建与校验；NodeID UUIDv4 生成唯一性与 IP 解耦；PublicKey 字段位预留（`empty().isPresent == false`）；Capabilities 声明完整性；EndpointAddress 动态列表管理；SessionEpoch 单调递增；SessionInstanceId 唯一性；NodeID 持久化往返（生成 → 持久化 → 加载 == 原始）；持久化损坏恢复策略
- **依赖任务**：CF1-TASK-007
- **输入**：design.md §2.3.2 模型实现、spec §5.1 Node Identity 规则
- **输出**：`tests/test_cf1_node_identity.cpp` 新增
- **验收标准**：
  1. NodeIdentity 七要素创建与校验正确
  2. NodeID UUIDv4 唯一性与 IP 解耦验证
  3. PublicKey `empty().isPresent == false`
  4. SessionEpoch 单调递增验证
  5. NodeID 持久化往返一致
  6. 持久化损坏恢复策略正确
- **Design Contract**：DC-S01-001~DC-S01-003
- **Acceptance Test**：单元测试全部通过
- **优先级**：P1
- **预估工作量**：2 小时
- **平台归属**：跨平台共享

### 7.2 CF1-TASK-038 Discovery 协议单元测试

- **任务标题**：编写 Discovery 协议单元测试
- **任务描述**：在 `tests/test_cf1_discovery.cpp` 中编写 Discovery 单元测试；mDNS 声明发布与监听；DiscoveryTable 以 NodeID 为键索引；DiscoveryDigest 构建与 TXT 记录编解码往返；动态加入/离开；重复发现幂等（同 NodeID 同 Epoch 仅更新地址）；跨拓扑隔离（Topology ID 不一致忽略）；NodeID 冲突检测告警；LAN Broadcast / Manual Config fallback 降级
- **依赖任务**：CF1-TASK-014
- **输入**：design.md §2.1.2 模块 L 功能点、spec §5.2 发现规则
- **输出**：`tests/test_cf1_discovery.cpp` 新增
- **验收标准**：
  1. mDNS 声明发布与监听正确
  2. DiscoveryTable 以 NodeID 为键
  3. DiscoveryDigest TXT 编解码往返一致
  4. 重复发现幂等
  5. 跨拓扑隔离正确
  6. NodeID 冲突检测告警正确
- **Design Contract**：DC-S02-001~DC-S02-003
- **Acceptance Test**：单元测试全部通过
- **优先级**：P1
- **预估工作量**：2 小时
- **平台归属**：跨平台共享

### 7.3 CF1-TASK-039 Pairing/Registration FSM 单元测试

- **任务标题**：编写 Pairing/Registration 7 状态 FSM 单元测试
- **任务描述**：在 `tests/test_cf1_pairing_fsm.cpp` 中编写 Pairing/Registration FSM 单元测试；9 态完整（7 成功 + 2 失败）；成功路径 6 步转移全部覆盖；失败路径任一阶段→REJECTED/ROLLBACK→保持安全状态；五层分层不可跃迁守卫（DISCOVERED→MEMBER 跃迁拒绝）；配对码经 Control Plane 传输（禁广播）；TrustedNodeList 持久化往返；注册握手四步原子完成；注册幂等；失败回滚 Trusted List 与 Topology Membership 不变
- **依赖任务**：CF1-TASK-019、CF1-TASK-023
- **输入**：design.md §2.1.3.2 状态机设计、spec §5.3.4/AMEND-002
- **输出**：`tests/test_cf1_pairing_fsm.cpp` 新增
- **验收标准**：
  1. 9 态完整，成功路径 6 步转移全部通过
  2. 失败路径回滚保持安全状态
  3. 五层分层跃迁拒绝 + 告警 `CFX-E-PAIR-ILLEGAL-TRANS`
  4. 配对码经 Control Plane，禁广播
  5. 注册原子性全或无
  6. 注册幂等
- **Design Contract**：DC-S03-001~DC-S03-003
- **Acceptance Test**：单元测试全部通过
- **优先级**：P1
- **预估工作量**：2-3 小时
- **平台归属**：跨平台共享

### 7.4 CF1-TASK-040 TopologyVersion Atomic Commit 单元测试

- **任务标题**：编写 TopologyVersion Atomic Commit 单元测试
- **任务描述**：在 `tests/test_cf1_topology_commit.cpp` 中编写 TopologyVersion Atomic Commit 单元测试；版本覆盖规则（incoming < current → reject / == → idempotent / > → validate → commit）；D-TOPO-ATOMIC-005 严格协议状态机 7 态流转；Proposal ≠ Active / Prepare ≠ Active / ACK ≠ Active；仅 Coordinator 发出 Activate Authorization 后 Active；Coordinator 离线 → LOCKED → 新 Proposal REJECT；Coordinator 恢复后从旧版本继续；多节点同时声称 Coordinator 告警；全或无语义（任一端点失败整体回滚）；不引入 Raft/Paxos/Consensus 验证
- **依赖任务**：CF1-TASK-029、CF1-TASK-030
- **输入**：design.md §2.6.7/§2.6.8 D-TOPO-ATOMIC-005/D-TOPO-COORD-005、spec §5.4.4
- **输出**：`tests/test_cf1_topology_commit.cpp` 新增
- **验收标准**：
  1. 版本覆盖规则三条全部正确
  2. D-TOPO-ATOMIC-005 协议状态机 7 态流转正确
  3. Coordinator 离线 LOCKED + 旧版本 authoritative
  4. Coordinator 恢复后从旧版本继续，不自动重提交
  5. 多节点同时声称 Coordinator 告警
  6. 全或无语义验证
  7. 无 Raft/Paxos/Consensus 依赖
- **Design Contract**：DC-S04-002~DC-S04-009
- **Acceptance Test**：AT-BLOCKER-002-1~6、AT-BLOCKER-003-1~7、AT-BLOCKER-004-1~6
- **优先级**：P1
- **预估工作量**：3 小时
- **平台归属**：跨平台共享

### 7.5 CF1-TASK-041 SessionFence 三元栅栏单元测试

- **任务标题**：编写 SessionFence 三元栅栏与 EPOCH 契约单元测试
- **任务描述**：在 `tests/test_cf1_session_fence.cpp` 中编写 SessionFence 单元测试；三元栅栏构建（NodeID + Epoch + InstanceID）；`checkIncoming()` 准入检查流程；EPOCH-001 SessionEpoch 单调递增；EPOCH-002 低 Epoch 拒绝 + 告警；EPOCH-003 同 Epoch 不同 InstanceID 冲突 + 告警；EPOCH-004 新 Epoch 接受新会话；EPOCH-005 旧 Session 不修改关键状态；Bootstrap/Established Session 两级 Admission（Bootstrap 类消息不经过 NodeID Check，Established Session 类消息强制三元 Fence）；未知对端 Established Session 消息拒绝
- **依赖任务**：CF1-TASK-034
- **输入**：design.md §2.7.3 EPOCH 实现方案、§2.11.8.1 BLOCKER-001
- **输出**：`tests/test_cf1_session_fence.cpp` 新增
- **验收标准**：
  1. 三元栅栏构建与检查正确
  2. EPOCH-001~006 六条契约全部通过
  3. Bootstrap 类消息不经过 NodeID Check
  4. Established Session 类消息强制三元 Fence
  5. 未知对端 Established Session 消息拒绝
- **Design Contract**：DC-S05-001~DC-S05-004、EPOCH-001~006
- **Acceptance Test**：AT-BLOCKER-001-1~4
- **优先级**：P1
- **预估工作量**：2 小时
- **平台归属**：跨平台共享

---

## 8. 集成测试

> 本组任务实现跨模块集成测试，验证 CF1 端到端身份建立与恢复流程。

### 8.1 CF1-TASK-042 端到端身份建立集成测试

- **任务标题**：编写端到端身份建立集成测试（发现→配对→注册→成员加入）
- **任务描述**：在 `tests/integration/test_cf1_identity_e2e.cpp` 中编写端到端集成测试；模拟两端点 A、B 完整身份建立流程：A 发布 mDNS 声明 → B 发现 A → B 发起配对（配对码经 Control Plane）→ 配对成功 A 加入 B 的 Trusted List → B 发起注册（版本+能力+拓扑同步）→ 注册成功 → A、B 互为拓扑成员；验证 PairingState 状态流转 DISCOVERED→UNTRUSTED→PAIRING→TRUSTED→REGISTERING→REGISTERED→MEMBER；验证全拓扑 ≤1s 内收悉成员变更；验证 NodeID 跨流程不变
- **依赖任务**：CF1-TASK-017、CF1-TASK-024、CF1-TASK-031
- **输入**：design.md §2.1.3.2 状态机、spec §5.3 注册流程
- **输出**：`tests/integration/test_cf1_identity_e2e.cpp` 新增
- **验收标准**：
  1. 端到端身份建立流程正确（发现→配对→注册→成员加入）
  2. PairingState 7 态流转正确
  3. 全拓扑 ≤1s 内收悉成员变更
  4. NodeID 跨流程不变
- **Design Contract**：DC-S01-001、DC-S03-001~DC-S03-003
- **Acceptance Test**：端到端身份建立测试通过
- **优先级**：P1
- **预估工作量**：3 小时
- **平台归属**：跨平台共享

### 8.2 CF1-TASK-043 Topology Atomic Commit 集成测试

- **任务标题**：编写 Topology Atomic Commit 集成测试（BLOCKER-002/003/004）
- **任务描述**：在 `tests/integration/test_cf1_topology_commit_e2e.cpp` 中编写 Atomic Commit 集成测试；3 节点拓扑（A/B/C），A 为 Coordinator（配置预先指定）；AT-BLOCKER-002-1：单一 Coordinator，无自动 Election；AT-BLOCKER-002-2：Proposal 经 Coordinator；AT-BLOCKER-002-3：并发 Proposal 串行化；AT-BLOCKER-002-4：Coordinator 离线 LOCKED；AT-BLOCKER-002-5：Coordinator 恢复后继续；AT-BLOCKER-003-1~7：严格协议状态机 7 条验收；AT-BLOCKER-004-1~6：Coordinator Identity Lifecycle 6 条验收；验证全或无语义、无 split-brain、无 Raft/Paxos
- **依赖任务**：CF1-TASK-040
- **输入**：design.md §2.11.8.2/§2.11.9 BLOCKER-002/003/004 验收测试
- **输出**：`tests/integration/test_cf1_topology_commit_e2e.cpp` 新增
- **验收标准**：
  1. AT-BLOCKER-002-1~6 全部通过
  2. AT-BLOCKER-003-1~7 全部通过
  3. AT-BLOCKER-004-1~6 全部通过
  4. 全或无语义验证
  5. 无 split-brain（`active_authority_count ≤ 1`）
  6. 无 Raft/Paxos/Consensus 依赖
- **Design Contract**：DC-S04-004~DC-S04-009
- **Acceptance Test**：AT-BLOCKER-002/003/004 全部通过
- **优先级**：P1
- **预估工作量**：3-4 小时
- **平台归属**：跨平台共享

### 8.3 CF1-TASK-044 Session Fencing 集成测试

- **任务标题**：编写 Session Fencing 集成测试（BLOCKER-001）
- **任务描述**：在 `tests/integration/test_cf1_session_fence_e2e.cpp` 中编写 Session Fencing 集成测试；AT-BLOCKER-001-1：首次配对不锁死（A、B 未配对，PairingRequest 经 Bootstrap ACCEPT）；AT-BLOCKER-001-2：Bootstrap 消息不经过三元 Fence NodeID Check；AT-BLOCKER-001-3：Established Session 消息强制三元 Fence（未知端点 C 发送 MembershipChangeNotification 被 REJECT）；AT-BLOCKER-001-4：配对成功后切换至 Established Session 通道；EPOCH-001~006 六条契约端到端验证
- **依赖任务**：CF1-TASK-041
- **输入**：design.md §2.11.8.1 BLOCKER-001 验收测试、§2.7.3 EPOCH 契约
- **输出**：`tests/integration/test_cf1_session_fence_e2e.cpp` 新增
- **验收标准**：
  1. AT-BLOCKER-001-1~4 全部通过
  2. EPOCH-001~006 端到端验证通过
  3. Bootstrap 类消息不锁死首次配对
  4. Established Session 类消息强制三元 Fence
- **Design Contract**：DC-S05-004、EPOCH-001~006
- **Acceptance Test**：AT-BLOCKER-001-1~4 全部通过
- **优先级**：P1
- **预估工作量**：2-3 小时
- **平台归属**：跨平台共享

### 8.4 CF1-TASK-045 断线重连身份恢复集成测试

- **任务标题**：编写断线重连身份恢复集成测试
- **任务描述**：在 `tests/integration/test_cf1_recovery_e2e.cpp` 中编写身份恢复集成测试；模拟 A、B 已配对注册，A 断线后重连；验证身份恢复流程：重连成功 → 触发 `IdentityRecoveryManager.initiateRecovery()` → 携带 NodeID + 新 Epoch + 新 InstanceID → 恢复拓扑成员关系；验证身份恢复 ≤3s；验证不重新配对（EPOCH-006）；验证不恢复断线前按下状态；验证恢复期间 Handoff 暂停；验证 Safety Invariant 全程成立；模拟 IP 变更重新发现（≤2s）
- **依赖任务**：CF1-TASK-036
- **输入**：design.md §2.2.2.8 IdentityRecovery 接口、spec §5.5 断线重连规则
- **输出**：`tests/integration/test_cf1_recovery_e2e.cpp` 新增
- **验收标准**：
  1. 身份恢复 ≤3s 完成
  2. 不重新配对（EPOCH-006）
  3. 不恢复断线前按下状态
  4. 恢复期间 Handoff 暂停
  5. Safety Invariant 全程成立
  6. IP 变更重新发现 ≤2s
- **Design Contract**：DC-S05-003、EPOCH-006
- **Acceptance Test**：断线重连身份恢复测试通过
- **优先级**：P1
- **预估工作量**：2-3 小时
- **平台归属**：跨平台共享

---

## 9. Design Contract 验证

> 本组任务验证 CF1 全部 Design Contract 与 CF0 Safety Invariant 保持。最高优先级为 Safety Invariant 保持验证。

### 9.1 CF1-TASK-046 S01~S05 Design Contract 架构测试

- **任务标题**：实现 S01~S05 Design Contract 架构测试
- **任务描述**：在 `tests/architecture/test_cf1_design_contracts.cpp` 中实现 S01~S05 全部 Design Contract 架构测试；DC-S01-001：Stable NodeID 不变性（IP 变更/重启/重连 NodeID 不变）；DC-S01-002：NodeID 不可伪造（网络报文不覆盖本端 NodeID）；DC-S01-003：PublicKey 与 NodeID 生命周期解耦；DC-S02-001：不依赖固定 IP 发现；DC-S02-002：发现以 NodeID 为键；DC-S02-003：跨拓扑隔离；DC-S03-001：配对前置；DC-S03-002：注册原子性；DC-S03-003：五层分层不可跃迁；DC-S04-001：循环拓扑有效运行态；DC-S04-002：过期更新拒绝；DC-S04-003：原子提交全或无；DC-S05-001：低 Epoch 消息拒绝；DC-S05-002：旧 Session 不修改关键状态；DC-S05-003：重连不破坏 Safety Invariant
- **依赖任务**：CF1-TASK-037~041
- **输入**：design.md §2.11.1~§2.11.5 Design Contract 回映
- **输出**：`tests/architecture/test_cf1_design_contracts.cpp` 新增
- **验收标准**：
  1. DC-S01-001~003 全部通过
  2. DC-S02-001~003 全部通过
  3. DC-S03-001~003 全部通过
  4. DC-S04-001~003 全部通过
  5. DC-S05-001~003 全部通过
- **Design Contract**：DC-S01~S05 全部
- **Acceptance Test**：架构测试全部通过
- **优先级**：P1
- **预估工作量**：2-3 小时
- **平台归属**：跨平台共享

### 9.2 CF1-TASK-047 4 个 Amendment Design Contract 验证

- **任务标题**：实现 4 个 Amendment Design Contract 验证
- **任务描述**：在 `tests/architecture/test_cf1_amendments.cpp` 中实现 4 个 Amendment Design Contract 验证；AMEND-001（DC-S01-003）：PublicKey 更换后 NodeID 不变；AMEND-002（DC-S03-003）：7 状态 FSM + 五层分层不可跃迁；AMEND-003（DC-S04-002~DC-S04-009）：TopologyVersion Atomic Commit 8 条契约（含 D-TOPO-ATOMIC-005 严格协议状态机 + D-TOPO-COORD-005 Coordinator Identity Lifecycle）；AMEND-004（DC-S05-001~DC-S05-004 + EPOCH-001~006）：Session Fencing 4 条契约 + 6 条 EPOCH 测试契约；全部 Amendment 已转为 Design Contract 并回映到 spec §9 Verification Matrix
- **依赖任务**：CF1-TASK-046
- **输入**：design.md §2.11.7 Amendment 回映汇总、§2.11.8/§2.11.9 Blocker 验收测试
- **输出**：`tests/architecture/test_cf1_amendments.cpp` 新增
- **验收标准**：
  1. AMEND-001：PublicKey 更换后 NodeID 不变
  2. AMEND-002：7 状态 FSM + 五层分层不可跃迁
  3. AMEND-003：8 条契约全部通过（含 D-TOPO-ATOMIC-005 + D-TOPO-COORD-005）
  4. AMEND-004：4 条契约 + 6 条 EPOCH 全部通过
  5. 全部 Amendment 回映 spec §9 Verification Matrix
- **Design Contract**：AMEND-001~004 全部
- **Acceptance Test**：AT-BLOCKER-001~004 全部通过
- **优先级**：P1
- **预估工作量**：2-3 小时
- **平台归属**：跨平台共享

### 9.3 CF1-TASK-048 CF0 Safety Invariant 保持验证

- **任务标题**：实现 CF0 Safety Invariant 保持验证（最高优先级）
- **任务描述**：在 `tests/architecture/test_cf1_safety_invariant.cpp` 中实现 CF0 Safety Invariant 保持验证；验证 CF1 的任何身份变更、发现失败、注册失败、重连恢复，都不得导致 CF0 Safety Invariant 被破坏；P1 No Split-Brain：CF1 拓扑变更不导致版本脑裂（D-TOPO-ATOMIC-005 + D-TOPO-COORD-005 保障）；P2 No Void-Owner：CF1 成员离开不导致控制权悬空（NodeCountGuard + DEGRADED 判定）；P3 Recoverable：CF1 身份恢复/持久化损坏后可恢复（IdentityRecoveryManager + recoverFromCorruption）；验证 CF1 不修改 CF0 冻结的接口签名、领域对象、FSM、线程模型、双平面隔离
- **依赖任务**：CF1-TASK-047
- **输入**：design.md §2.12.2 不扩张保障、CF0 design §2.17 Safety Invariant
- **输出**：`tests/architecture/test_cf1_safety_invariant.cpp` 新增
- **验收标准**：
  1. P1 No Split-Brain：CF1 拓扑变更不导致版本脑裂
  2. P2 No Void-Owner：CF1 成员离开不导致控制权悬空
  3. P3 Recoverable：CF1 身份恢复/持久化损坏后可恢复
  4. CF1 不修改 CF0 冻结的接口签名/领域对象/FSM/线程模型/双平面隔离
  5. 本测试为 CF1 冻结必要条件，任一失败 CF1 不得冻结
- **Design Contract**：P1 ∧ P2 ∧ P3 保持
- **Acceptance Test**：Safety Invariant 全部通过
- **优先级**：P0
- **预估工作量**：2-3 小时
- **平台归属**：跨平台共享

---

## 10. 验证与冻结

> 本组任务实现最终验证与 CF1 阶段冻结。

### 10.1 CF1-TASK-049 DFX 红线验证

- **任务标题**：验证 CF1 DFX 红线达标
- **任务描述**：验证 CF1 全部 DFX 红线：mDNS 声明发布延迟 ≤500ms（§4.1.1）；对端发现延迟 ≤1s（§4.1.2）；端点注册握手延迟 ≤2s（§4.1.3）；身份恢复延迟 ≤3s（§4.1.4）；IP 变更重新发现延迟 ≤2s（§4.1.5）；成员变更通知延迟 ≤1s 全拓扑收悉（§4.1.6）；SessionFence 消息准入检查 ≤1ms；Atomic Commit 超时 1s；CF1 复用 CF0 8 线程，不引入新线程
- **依赖任务**：CF1-TASK-048
- **输入**：design.md §2.1.2 配置项取值策略、spec §4.1 性能 DFX 红线
- **输出**：CF1 DFX 红线验证报告
- **验收标准**：
  1. mDNS 声明发布延迟 ≤500ms
  2. 对端发现延迟 ≤1s
  3. 端点注册握手延迟 ≤2s
  4. 身份恢复延迟 ≤3s
  5. IP 变更重新发现延迟 ≤2s
  6. 成员变更通知延迟 ≤1s
  7. SessionFence 检查 ≤1ms
  8. Atomic Commit 超时 1s
  9. CF1 不引入新线程（≤8）
- **Design Contract**：DFX 红线全部达标
- **Acceptance Test**：DFX 红线验证报告
- **优先级**：P1
- **预估工作量**：2 小时
- **平台归属**：跨平台共享

### 10.2 CF1-TASK-050 CF1 架构冻结审查与交付

- **任务标题**：执行 CF1 架构冻结审查与交付
- **任务描述**：汇总全部验收结果；确认 S01~S05 Design Contract（DC-S01-001~003 / DC-S02-001~003 / DC-S03-001~003 / DC-S04-001~009 / DC-S05-001~004）全部通过；确认 4 个 Amendment Design Contract 全部通过；确认 EPOCH-001~006 六条契约全部通过；确认 BLOCKER-001~004 验收测试（AT-BLOCKER-001-1~4 / AT-BLOCKER-002-1~6 / AT-BLOCKER-003-1~7 / AT-BLOCKER-004-1~6）全部通过；确认 CF0 Safety Invariant（P1/P2/P3）保持；确认 DFX 红线全部达标；确认不引入 Raft/Paxos/Consensus；确认不修改 CF0 Frozen Architecture；任一验收项失败 → CF1 不得冻结，回退修正
- **依赖任务**：CF1-TASK-049
- **输入**：全部验收报告
- **输出**：CF1 架构冻结审查报告
- **验收标准**：
  1. S01~S05 Design Contract 全部通过
  2. 4 个 Amendment Design Contract 全部通过
  3. EPOCH-001~006 全部通过
  4. AT-BLOCKER-001~004 全部通过
  5. CF0 Safety Invariant P1/P2/P3 保持
  6. DFX 红线全部达标
  7. 不引入 Raft/Paxos/Consensus
  8. 不修改 CF0 Frozen Architecture
  9. 全部通过 → CF1 架构冻结，交付后续阶段
  10. 任一失败 → CF1 不得冻结，回退修正
- **Design Contract**：全部 Design Contract + Safety Invariant
- **Acceptance Test**：冻结审查报告
- **优先级**：P0
- **预估工作量**：2 小时
- **平台归属**：跨平台共享

---

## Design Contract / Acceptance Test 映射表

### S01 Node Identity Model

| Design Contract | 实现任务 | spec Requirement ID | 保障的 Safety Invariant |
|----------------|---------|---------------------|------------------------|
| DC-S01-001: Stable NodeID 不变性 | CF1-TASK-007 | CF1-S01-REQ-001 | P3 Recoverable |
| DC-S01-002: NodeID 不可伪造 | CF1-TASK-007 | CF1-S01-REQ-002 | P1 No Split-Brain |
| DC-S01-003: PublicKey 生命周期解耦（AMEND-001） | CF1-TASK-008 | CF1-S01-REQ-003 | P3 Recoverable |

### S02 Discovery Protocol

| Design Contract | 实现任务 | spec Requirement ID | 保障的 Safety Invariant |
|----------------|---------|---------------------|------------------------|
| DC-S02-001: 不依赖固定 IP 发现 | CF1-TASK-013 | CF1-S02-REQ-001 | - |
| DC-S02-002: 发现以 NodeID 为键 | CF1-TASK-014 | CF1-S02-REQ-002 | - |
| DC-S02-003: 跨拓扑隔离 | CF1-TASK-017 | CF1-S02-REQ-003 | - |

### S03 Registration & Handshake

| Design Contract | 实现任务 | spec Requirement ID | 保障的 Safety Invariant |
|----------------|---------|---------------------|------------------------|
| DC-S03-001: 配对前置 | CF1-TASK-022 | CF1-S03-REQ-001 | P1 No Split-Brain |
| DC-S03-002: 注册原子性 | CF1-TASK-023 | CF1-S03-REQ-002 | P1 ∧ P2 |
| DC-S03-003: 五层分层不可跃迁（AMEND-002） | CF1-TASK-020 | CF1-S03-REQ-003 | P1 No Split-Brain |

### S04 Topology Membership

| Design Contract | 实现任务 | spec Requirement ID | 保障的 Safety Invariant |
|----------------|---------|---------------------|------------------------|
| DC-S04-001: 循环拓扑有效运行态 | CF1-TASK-027 | CF1-S04-REQ-001 | P2 No Void-Owner |
| DC-S04-002: 过期更新拒绝（AMEND-003） | CF1-TASK-028 | CF1-S04-REQ-002 | P1 No Split-Brain |
| DC-S04-003: 原子提交全或无（AMEND-003） | CF1-TASK-029 | CF1-S04-REQ-003 | P1 No Split-Brain |
| DC-S04-004: 单一 Commit Authority（D-TOPO-COORD-001） | CF1-TASK-030 | CF1-S04-REQ-003 | P1 No Split-Brain |
| DC-S04-005: 所有 Proposal 经 Coordinator（D-TOPO-COORD-002） | CF1-TASK-030 | CF1-S04-REQ-003 | P1 No Split-Brain |
| DC-S04-006: 并发 Proposal 串行化（D-TOPO-COORD-003） | CF1-TASK-030 | CF1-S04-REQ-003 | P1 No Split-Brain |
| DC-S04-007: Coordinator 离线 LOCKED（D-TOPO-COORD-004） | CF1-TASK-030 | CF1-S04-REQ-003 | P1 ∧ P3 |
| DC-S04-008: 严格协议状态机（D-TOPO-ATOMIC-005） | CF1-TASK-029 | CF1-S04-REQ-003 | P1 No Split-Brain |
| DC-S04-009: Coordinator Identity Lifecycle（D-TOPO-COORD-005） | CF1-TASK-030 | CF1-S04-REQ-003 | P1 ∧ P3 |

### S05 Session Epoch & Reconnection

| Design Contract | 实现任务 | spec Requirement ID | 保障的 Safety Invariant |
|----------------|---------|---------------------|------------------------|
| DC-S05-001: 低 Epoch 消息拒绝（EPOCH-002） | CF1-TASK-034 | CF1-S05-REQ-001 | P1 No Split-Brain |
| DC-S05-002: 旧 Session 不修改关键状态（EPOCH-005） | CF1-TASK-034 | CF1-S05-REQ-002 | P1 ∧ P2 |
| DC-S05-003: 重连不破坏 Safety Invariant | CF1-TASK-036 | CF1-S05-REQ-003 | P1 ∧ P2 ∧ P3 |
| DC-S05-004: Bootstrap/Established Session 两级 Admission（BLOCKER-001） | CF1-TASK-035 | CF1-S05-REQ-001 | P1 No Split-Brain |

### EPOCH-001~006 契约映射

| 契约 ID | 实现任务 | spec 规则条目 |
|---------|---------|-------------|
| EPOCH-001 | CF1-TASK-010 | §5.5.4 规则 1 |
| EPOCH-002 | CF1-TASK-034 | §5.5.4 规则 2 |
| EPOCH-003 | CF1-TASK-034 | §5.5.4 规则 3 |
| EPOCH-004 | CF1-TASK-034 | §5.5.4 规则 4 |
| EPOCH-005 | CF1-TASK-034 | §5.5.4 规则 5 |
| EPOCH-006 | CF1-TASK-036 | §5.5.4 规则 6 |

### Blocker 验收测试映射

| Blocker | Acceptance Test | 实现任务 |
|---------|----------------|---------|
| BLOCKER-001 Bootstrap Admission | AT-BLOCKER-001-1~4 | CF1-TASK-035 |
| BLOCKER-002 Commit Authority | AT-BLOCKER-002-1~6 | CF1-TASK-030 |
| BLOCKER-003 Atomic Commit 严格语义 | AT-BLOCKER-003-1~7 | CF1-TASK-029 |
| BLOCKER-004 Coordinator Identity Lifecycle | AT-BLOCKER-004-1~6 | CF1-TASK-030 |

---

## 与 CF0 衔接说明

1. **CF0 已实现基础**：CF0-TASK-001~069 已实现 C++20 项目骨架、错误码体系、领域模型、跨平台抽象层、CF0-S01~S05 核心模块、防抖动机制、并发模型、Handoff FSM、单元/集成/架构契约测试。CF1 在此基础上扩展。

2. **复用 CF0 基础设施**：
   - 错误码：复用 CF0 `error_code.hpp`，扩展 30 个 CF1 新增错误码（CF1-TASK-001）
   - 日志：复用 CF0 `logger.hpp` JSON Lines 日志器，扩展 sessionEpoch/sessionInstanceId 字段
   - 领域模型：复用 CF0 `domain.hpp` 的 NodeId/Platform/ScreenBoundary/NeighborRelation/TopologyView，扩展 NodeIdentity 七要素（CF1-TASK-002）
   - 消息：复用 CF0 `messages.hpp` ControlMessage variant，扩展 6 类子类型（CF1-TASK-004）
   - 传输层：复用 CF0 `IControlPlaneChannel` 传输 CF1 控制报文，不修改 CF0 接口
   - 线程模型：复用 CF0 8 线程，CF1 不引入新线程（保持 ≤8）

3. **不修改 CF0 冻结内容**：
   - 不修改 CF0 接口签名（ITopologyManager/IControlPlaneChannel/IHandoffOrchestrator 等）
   - 不修改 CF0 领域对象定义（NodeId/Platform/ScreenBoundary 等，仅扩展不修改已有字段）
   - 不修改 CF0 Handoff 六态 FSM（CF1 仅通过 `IRegistrationManager.isMember()` 校验对端注册状态）
   - 不修改 CF0 线程模型与双平面隔离架构
   - 不修改 CF0 Safety Invariant P1/P2/P3

4. **C++20 升级**：CF0 tasks.md 已包含 C++17→C++20 升级任务（CF0-TASK-001/002/043/044），CF1 直接基于 C++20 设施（std::expected/std::span/std::jthread/std::chrono/std::atomic）开发，无需额外升级任务。

5. **技术约束**：
   - C++20（MUST: expected/span/chrono/atomic/jthread / SHOULD: coroutine/variant/concepts）
   - 不引入 Raft/Paxos/Consensus（TopologyVersionManager 为中心化协调者模型）
   - 不修改 CF0 Frozen Architecture
   - 不修改 Handoff FSM
   - 保持 Safety Invariant P1/P2/P3
   - 文件名 snake_case，任务编号 CF1-TASK-XXX

---

> **任务规划结束**
> 本编码任务规划基于 `.codeartsdoer/specs/cf1_endpoint_disc/spec.md`（v2，1092 行，已 FROZEN）与 `.codeartsdoer/specs/cf1_endpoint_disc/design.md`（v4，~2997 行，已 FROZEN）生成。
> **任务统计**：共 10 组 50 个任务（Group 1: 5 + Group 2: 6 + Group 3: 6 + Group 4: 7 + Group 5: 7 + Group 6: 5 + Group 7: 5 + Group 8: 4 + Group 9: 3 + Group 10: 2）。
> **覆盖范围**：
> - **CF1-S01 Node Identity Model**：NodeIdentity 七要素、INodeIdentityManager、PublicKey 生命周期解耦（AMEND-001）、Capabilities、Endpoint Addresses、Session Epoch、持久化完整性恢复（CF1-TASK-006~011）；
> - **CF1-S02 Discovery Protocol**：mDNS 发现、LAN Broadcast/Manual Config fallback、DiscoveryTable（NodeID 索引）、Discovery Digest、动态加入/离开、跨拓扑隔离、NodeID 冲突检测（CF1-TASK-012~017）；
> - **CF1-S03 Pairing / Registration**：7 状态 FSM（AMEND-002）、五层分层不可跃迁、TrustedNodeList 持久化、配对码经 Control Plane、注册握手原子性、注册幂等、失败回滚（CF1-TASK-018~024）；
> - **CF1-S04 Topology Membership Mgmt**：动态成员管理、TopologyVersion Atomic Commit（AMEND-003）、D-TOPO-ATOMIC-005 严格协议状态机、D-TOPO-COORD-005 Coordinator Identity Lifecycle、NodeCountGuard、DEGRADED 判定、循环拓扑维护、成员变更通知（CF1-TASK-025~031）；
> - **CF1-S05 Session Epoch & Reconnection**：(NodeID, Epoch, InstanceID) 三元栅栏（AMEND-004）、EPOCH-001~006 六条契约、Bootstrap/Established Session 两级 Message Admission（BLOCKER-001）、断线重连身份恢复（CF1-TASK-032~036）；
> - **跨组件**：错误码扩展（30 个）、领域模型扩展、ControlMessage variant 扩展（6 类）、消息序列化、单元测试、集成测试、Design Contract 验证、Safety Invariant 保持验证、DFX 红线验证（CF1-TASK-001~005, 037~050）；
> - **4 个 Amendment**：AMEND-001 PublicKey Lifecycle（DC-S01-003）、AMEND-002 Pairing/Registration FSM（DC-S03-003）、AMEND-003 TopologyVersion Atomic Commit（DC-S04-002~009，含 D-TOPO-ATOMIC-005 + D-TOPO-COORD-005）、AMEND-004 Session Fencing（DC-S05-001~004 + EPOCH-001~006）；
> - **4 个 Blocker 修复**：BLOCKER-001 Bootstrap Admission（AT-BLOCKER-001-1~4）、BLOCKER-002 Commit Authority（AT-BLOCKER-002-1~6）、BLOCKER-003 Atomic Commit 严格语义（AT-BLOCKER-003-1~7）、BLOCKER-004 Coordinator Identity Lifecycle（AT-BLOCKER-004-1~6）。
> **与 CF0 衔接**：复用 CF0-TASK-001~069 基础设施（错误码/日志/领域模型/传输层/线程模型），不修改 CF0 Frozen Architecture、不重新设计 Handoff FSM、不引入 Raft/Paxos/Consensus、保持 Safety Invariant P1/P2/P3。
> 待用户审查确认后，交付编码实现阶段。