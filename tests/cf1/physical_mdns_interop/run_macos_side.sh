#!/bin/bash
# CrossFlow-X Physical mDNS Interoperability Test - Full Bidirectional
# 
# Prerequisites:
#   - macOS machine and Windows machine on same LAN
#   - CrossFlow-X built on Windows machine (mdns_announcer_tool.exe + mdns_listener_tool.exe)
#   - macOS has dns-sd built in
#   - Both machines have firewall allowing mDNS (port 5353 UDP)
#
# Usage:
#   On Windows: powershell .\run_windows_side.ps1
#   On macOS:   ./run_macos_side.sh
#
# Test A: macOS announcer → Windows listener
# Test B: Windows announcer → macOS listener

echo "=== CrossFlow-X Physical mDNS Interoperability Test ==="
echo ""
echo "This script runs on macOS."
echo "Ensure Windows machine is running its side simultaneously."
echo ""
echo "=== Test A: macOS Announcer → Windows Listener ==="
echo "Starting macOS announcer..."
echo "On Windows, run: .\build\tests\cf1\Debug\mdns_listener_tool.exe --timeout 10"
echo ""

# Start macOS announcer in background
dns-sd -R "CFX-MacTest" _crossflow-x._tcp 5353 "node-h=2" "node-l=100" "topo=phy-test" "proto=1" "epoch=1" "plat=mac" &
ANNOUNCER_PID=$!

sleep 10

kill $ANNOUNCER_PID 2>/dev/null
echo "macOS announcer stopped."
echo ""

echo "=== Test B: Windows Announcer → macOS Listener ==="
echo "On Windows, run: .\build\tests\cf1\Debug\mdns_announcer_tool.exe --service CFX-WinTest --port 5353 --topology phy-test --node-high 1 --node-low 200"
echo "Starting macOS listener..."
echo ""

timeout 10 dns-sd -B _crossflow-x._tcp local. 2>&1

echo ""
echo "=== Test Complete ==="
echo "Collect evidence from both sides and attach to Evidence Report v8."