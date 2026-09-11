#!/usr/bin/env python3
"""CrossFlow-X Physical mDNS Listener (Python/zeroconf)"""

import sys
import time
import socket
import json
import argparse
from datetime import datetime, timezone
from zeroconf import Zeroconf, ServiceBrowser, ServiceListener

class CrossFlowListener(ServiceListener):
    def __init__(self, start_time):
        self.start_time = start_time
        self.discoveries = []
        
    def add_service(self, zeroconf, service_type, name):
        info = zeroconf.get_service_info(service_type, name)
        if info:
            elapsed = (time.time() - self.start_time) * 1000
            discovery = {
                "count": len(self.discoveries) + 1,
                "latency_ms": round(elapsed, 1),
                "service_name": name,
                "type": service_type,
                "port": info.port,
                "addresses": [socket.inet_ntoa(addr) for addr in info.addresses],
                "server": info.server,
                "properties": {k.decode(): v.decode() for k, v in info.properties.items()},
                "timestamp": datetime.now(timezone.utc).isoformat(),
            }
            self.discoveries.append(discovery)
            
            print(f"\n--- Discovery #{discovery['count']} (latency: {discovery['latency_ms']} ms) ---")
            print(f"Service: {name}")
            print(f"Type: {service_type}")
            print(f"Port: {info.port}")
            print(f"Addresses: {discovery['addresses']}")
            print(f"Server: {info.server}")
            print(f"Properties:")
            for k, v in discovery["properties"].items():
                print(f"  {k} = {v}")
    
    def remove_service(self, zeroconf, service_type, name):
        print(f"Service removed: {name}")
    
    def update_service(self, zeroconf, service_type, name):
        pass

def main():
    parser = argparse.ArgumentParser(description="CrossFlow-X mDNS Listener")
    parser.add_argument("--timeout", type=int, default=10)
    args = parser.parse_args()

    print("=== CrossFlow-X Physical mDNS Listener (Python) ===")
    print(f"Service type: _crossflow-x._tcp")
    print(f"Timeout: {args.timeout} seconds")
    print(f"Platform: {sys.platform}")
    print(f"Timestamp: {datetime.now(timezone.utc).isoformat()}")

    start_time = time.time()
    
    zeroconf = Zeroconf()
    listener = CrossFlowListener(start_time)
    
    print("Listening for mDNS services...")
    
    browser = ServiceBrowser(zeroconf, "_crossflow-x._tcp.local.", listener)
    
    try:
        time.sleep(args.timeout)
    except KeyboardInterrupt:
        pass
    
    browser.cancel()
    zeroconf.close()
    
    total_time = (time.time() - start_time) * 1000
    
    print(f"\n=== Summary ===")
    print(f"Total time: {total_time:.0f} ms")
    print(f"Discoveries: {len(listener.discoveries)}")
    
    if listener.discoveries:
        print(f"RESULT: SUCCESS")
        print(f"\n=== Evidence JSON ===")
        evidence = {
            "test_type": "mDNS Listener",
            "platform": sys.platform,
            "timestamp": datetime.now(timezone.utc).isoformat(),
            "total_time_ms": round(total_time, 1),
            "discovery_count": len(listener.discoveries),
            "discoveries": listener.discoveries,
        }
        print(json.dumps(evidence, indent=2))
        return 0
    else:
        print(f"RESULT: TIMEOUT (no discovery within {args.timeout} seconds)")
        return 2

if __name__ == "__main__":
    sys.exit(main())