# CF1-TASK-014-PHY Environment Limitation Record

## Date: 2026-09-10
## Environment: Windows Single Machine

## Limitation

Current development environment is a **Windows single machine** with the following constraints:

1. **No macOS machine available**: Physical macOS ↔ Windows mDNS interoperability test requires both platforms on the same LAN.
2. **Windows DNS-SD API limitation**: `DnsServiceRegister` API call fails in the current Windows development environment. This may be due to:
   - DNS Client service configuration
   - Network interface configuration
   - Windows version/build specific behavior
   - Development environment sandboxing

## Impact

- **Test A (macOS → Windows)**: Cannot execute — no macOS machine available
- **Test B (Windows → macOS)**: Cannot execute — no macOS machine available
- **Windows local mDNS test**: Cannot execute — `DnsServiceRegister` fails

## What HAS been completed

1. ✅ Physical mDNS interoperability test tools created:
   - `mdns_announcer_tool.cpp` — Windows/macOS announcer CLI tool
   - `mdns_listener_tool.cpp` — Windows/macOS listener CLI tool with DiscoveryTable integration
2. ✅ macOS side test scripts created:
   - `mdns_announcer_mac.sh` — macOS announcer using `dns-sd`
   - `mdns_listener_mac.sh` — macOS listener using `dns-sd -B`
3. ✅ Windows side test script created:
   - `run_windows_side.ps1` — Windows test orchestration
4. ✅ macOS side orchestration script:
   - `run_macos_side.sh` — macOS test orchestration
5. ✅ Test procedure documented in `README.md`
6. ✅ Evidence collection requirements specified

## What REMAINS to be done

1. ❌ Execute Test A: macOS announcer → Windows listener (requires macOS machine)
2. ❌ Execute Test B: Windows announcer → macOS listener (requires macOS machine)
3. ❌ Collect physical evidence:
   - Both OS/version
   - NodeID, service name, endpoint
   - TXT / DiscoveryDigest
   - Discovery start/success timestamp
   - Measured latency
   - duplicate/stale/conflict observation
   - Final DiscoveryTable state
   - Test result
   - Git commit
   - Evidence timestamp

## Recommendation

Physical mDNS interoperability test must be executed in a real cross-platform environment:
- One macOS machine (with Bonjour/dns-sd)
- One Windows machine (with DNS-SD API functioning)
- Both on the same LAN

Once physical evidence is collected, update Evidence Report to v8 and upgrade TASK-014 from PARTIAL to FULL.