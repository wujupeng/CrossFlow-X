#!/bin/bash
# CrossFlow-X Physical mDNS Interoperability Test - macOS Announcer
# Run on macOS machine: ./mdns_announcer_mac.sh

SERVICE_NAME=${1:-"CFX-MacAnnouncer"}
PORT=${2:-5353}
TOPOLOGY=${3:-"phy-mac-test"}
NODE_HIGH=${4:-2}
NODE_LOW=${5:-100}

echo "=== CrossFlow-X Physical mDNS Announcer (macOS) ==="
echo "Service: $SERVICE_NAME"
echo "Port: $PORT"
echo "Topology: $TOPOLOGY"
echo "NodeID: $NODE_HIGH:$NODE_LOW"
echo "Platform: macOS"
echo "Timestamp: $(date -u +%Y-%m-%dT%H:%M:%SZ)"

# Use dns-sd command line tool (built into macOS)
dns-sd -R "$SERVICE_NAME" _crossflow-x._tcp "$PORT" \
    "node-h=$NODE_HIGH" \
    "node-l=$NODE_LOW" \
    "topo=$TOPOLOGY" \
    "proto=1" \
    "epoch=1" \
    "plat=mac"