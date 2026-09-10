# CrossFlow-X Physical mDNS Interoperability Test

This directory contains scripts for physical cross-platform mDNS interoperability testing (R4-014).

## Prerequisites

- One macOS machine and one Windows machine on the same LAN
- CrossFlow-X built on both machines
- Both machines have network firewall allowing mDNS (port 5353 UDP)

## Test Procedure

### Step 1: macOS Announcer → Windows Listener

On macOS machine:
```bash
./build/bin/test_mdns_announcer_mac --service "mac-test-node" --port 12345 --topology "test-topo-1"
```

On Windows machine:
```powershell
.\build\bin\test_mdns_listener_win.exe --service-type "_crossflow-x._tcp" --timeout 5
```

Expected: Windows listener discovers macOS announcer within 1 second with correct NodeID, TXT, and endpoint.

### Step 2: Windows Announcer → macOS Listener

On Windows machine:
```powershell
.\build\bin\test_mdns_announcer_win.exe --service "win-test-node" --port 12346 --topology "test-topo-1"
```

On macOS machine:
```bash
./build/bin/test_mdns_listener_mac --service-type "_crossflow-x._tcp" --timeout 5
```

Expected: macOS listener discovers Windows announcer within 1 second with correct NodeID, TXT, and endpoint.

### Step 3: Bidirectional + Conflict Detection

Run both announcers simultaneously, verify both listeners see both nodes, and test duplicate/stale/conflict handling.

## Evidence Collection

For each test run, collect:
- Timestamp
- Announcer platform + NodeID + TXT
- Listener platform + discovered records
- Discovery latency (ms)
- Any conflicts/duplicates/stale detected

This evidence must be attached to Evidence Report v7 as physical interop proof.