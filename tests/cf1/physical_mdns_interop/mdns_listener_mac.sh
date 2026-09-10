#!/bin/bash
# CrossFlow-X Physical mDNS Interoperability Test - macOS Listener
# Run on macOS machine: ./mdns_listener_mac.sh [timeout_seconds]

TIMEOUT=${1:-5}

echo "=== CrossFlow-X Physical mDNS Listener (macOS) ==="
echo "Service type: _crossflow-x._tcp"
echo "Timeout: $TIMEOUT seconds"
echo "Platform: macOS"
echo "Timestamp: $(date -u +%Y-%m-%dT%H:%M:%SZ)"

# Use dns-sd command line tool (built into macOS)
# -B means Browse
timeout $TIMEOUT dns-sd -B _crossflow-x._tcp local. 2>&1

echo "=== Browse complete ==="
echo "For full resolve+TXT, use: dns-sd -L <instance-name> _crossflow-x._tcp local."