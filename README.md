# CrossFlow-X

> Cross-platform seamless keyboard & mouse input fabric for macOS, Windows and future Linux endpoints.

CrossFlow-X is an engineering project for making multiple computers behave like one continuous input workspace. The primary scenario is:

```text
macOS  ── mouse reaches right edge ──>  Windows 11
macOS  <── mouse reaches left edge ───  Windows 11
```

The first release targets a **driverless user-mode architecture**. Input capture, handoff, topology, state synchronization and low-latency transport remain in user space. A future Virtual HID layer may be evaluated only after the user-mode path meets performance and reliability gates.

## Project goals

- Seamless mouse edge handoff between endpoints.
- Keyboard follows the current mouse owner.
- Low-latency LAN transport with a dedicated input path.
- Deterministic endpoint topology and cyclic handoff.
- Coordinate continuity across different resolutions and scaling factors.
- Correct modifier/key/button state recovery after disconnects.
- Secure authenticated endpoint pairing.
- No screen streaming and no remote-desktop video path.
- Control plane must never block the input plane.

## Initial scope

### Endpoints

- macOS sender/receiver.
- Windows 11 sender/receiver.
- Architecture prepared for additional Windows/Linux endpoints.

### Core domains

1. Input Capture Engine
2. Input Injection Engine
3. Coordinate Engine
4. Handoff Engine / finite-state machine
5. Keyboard Mapping Engine
6. Transport Fabric
7. State Synchronization Engine
8. Topology Engine
9. Security / Pairing
10. Diagnostics / Performance Telemetry

## Non-goals for the first milestone

- Screen sharing or video streaming.
- Cloud relay.
- Remote desktop control.
- Kernel/driver implementation.
- Clipboard/file synchronization unless separately authorized.

## Engineering principles

- Evidence first.
- Input Plane is latency-critical.
- Control Plane must not block Input Plane.
- Stable NodeID must be independent of changing IP addresses.
- Handoff must be explicit and stateful, not an ad-hoc coordinate jump.
- All pressed keys/buttons must have a deterministic recovery path.
- Performance must be measured, not described subjectively as "smooth".

## Planned gates

- **CF0** Architecture Specification / Freeze
- **CF1** Endpoint Identity & Discovery
- **CF2** macOS Input Capture
- **CF3** Windows Input Injection
- **CF4** Low-Latency Transport
- **CF5** Coordinate & Topology Engine
- **CF6** Seamless Handoff FSM
- **CF7** Keyboard / Modifier State Synchronization
- **CF8** Security & Pairing
- **CF9** Physical E2E Validation
- **CF10** Performance Gate
- **CF11** GUI / Packaging

## Repository structure

```text
CrossFlow-X/
├── docs/
│   ├── architecture/
│   └── roadmap/
├── include/
├── src/
├── tests/
├── platform/
│   ├── macos/
│   └── windows/
└── README.md
```

## Current status

**Project initiation — CF0 authorized for architecture specification.**

The repository is intentionally kept specification-first. Implementation begins only after the core event model, topology model, handoff FSM, transport contract and state-recovery rules are frozen.
