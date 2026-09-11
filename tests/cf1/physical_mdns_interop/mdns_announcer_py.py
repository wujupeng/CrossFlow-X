#!/usr/bin/env python3
"""CrossFlow-X Physical mDNS Announcer (Python/zeroconf)"""

import sys
import time
import socket
import json
import argparse
from datetime import datetime, timezone
from zeroconf import Zeroconf, ServiceInfo

def main():
    parser = argparse.ArgumentParser(description="CrossFlow-X mDNS Announcer")
    parser.add_argument("--service", default="CFX-PyTest")
    parser.add_argument("--port", type=int, default=5353)
    parser.add_argument("--topology", default="phy-test")
    parser.add_argument("--node-high", default="1")
    parser.add_argument("--node-low", default="100")
    parser.add_argument("--duration", type=int, default=15)
    args = parser.parse_args()

    print("=== CrossFlow-X Physical mDNS Announcer (Python) ===")
    print(f"Service: {args.service}")
    print(f"Port: {args.port}")
    print(f"Topology: {args.topology}")
    print(f"NodeID: {args.node_high}:{args.node_low}")
    print(f"Platform: {sys.platform}")
    print(f"Timestamp: {datetime.now(timezone.utc).isoformat()}")

    zeroconf = Zeroconf()
    
    txt_record = {
        b"node-h": args.node_high.encode(),
        b"node-l": args.node_low.encode(),
        b"topo": args.topology.encode(),
        b"proto": b"1",
        b"epoch": b"1",
        b"plat": sys.platform.encode(),
    }

    hostname = socket.gethostname()
    local_ip = socket.gethostbyname(hostname)
    
    service_type = "_crossflow-x._tcp.local."
    service_name = f"{args.service}.{service_type}"
    
    info = ServiceInfo(
        type_=service_type,
        name=service_name,
        addresses=[socket.inet_aton(local_ip)],
        port=args.port,
        properties=txt_record,
        server=f"{hostname}.local.",
    )

    print(f"Local IP: {local_ip}")
    print(f"Hostname: {hostname}")
    print(f"Service name: {service_name}")
    
    start_time = time.time()
    zeroconf.register_service(info)
    register_time = time.time() - start_time
    
    print(f"Registration time: {register_time*1000:.1f} ms")
    print(f"Status: ANNOUNCING")
    print(f"Waiting {args.duration} seconds...")
    
    try:
        time.sleep(args.duration)
    except KeyboardInterrupt:
        pass
    
    zeroconf.unregister_service(info)
    zeroconf.close()
    
    print(f"Stopped. Timestamp: {datetime.now(timezone.utc).isoformat()}")

if __name__ == "__main__":
    main()