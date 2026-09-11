import subprocess
import time
import sys
import os
import json
import socket
from datetime import datetime, timezone
from zeroconf import Zeroconf, ServiceBrowser, ServiceListener

base_dir = os.path.dirname(__file__)
duration = 30

print("=" * 60)
print("CrossFlow-X Physical mDNS Cross-Platform Test (Windows)")
print("=" * 60)
print(f"Date: {datetime.now(timezone.utc).isoformat()}")
print(f"Platform: {sys.platform}")
print(f"Duration: {duration} seconds")
print()

announcer = subprocess.Popen([
    sys.executable,
    os.path.join(base_dir, "mdns_announcer_py.py"),
    "--service", "CFX-WinCrossTest",
    "--port", "5353",
    "--topology", "phy-cross-test",
    "--node-high", "1",
    "--node-low", "500",
    "--duration", str(duration + 5)
], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)

time.sleep(2)

class CrossFlowListener(ServiceListener):
    def __init__(self, start_time):
        self.start_time = start_time
        self.discoveries = []
        
    def add_service(self, zc, service_type, name):
        info = zc.get_service_info(service_type, name)
        if info:
            elapsed = (time.time() - self.start_time) * 1000
            d = {
                "count": len(self.discoveries) + 1,
                "latency_ms": round(elapsed, 1),
                "service_name": name,
                "port": info.port,
                "addresses": [socket.inet_ntoa(a) for a in info.addresses],
                "server": info.server,
                "properties": {k.decode(): v.decode() for k, v in info.properties.items()},
                "timestamp": datetime.now(timezone.utc).isoformat(),
            }
            self.discoveries.append(d)
            print(f"\n--- Discovery #{d['count']} (latency: {d['latency_ms']} ms) ---")
            print(f"Service: {name}")
            print(f"Port: {info.port}")
            print(f"Addresses: {d['addresses']}")
            print(f"Server: {info.server}")
            print(f"Properties: {d['properties']}")
    
    def remove_service(self, zc, service_type, name):
        print(f"Service removed: {name}")
    
    def update_service(self, zc, service_type, name):
        pass

print("Windows announcer running (CFX-WinCrossTest, node-h=1, node-l=500)")
print("Windows listener listening for _crossflow-x._tcp...")
print()
print(">>> NOW RUN ON DEBIAN: ./run_debian_full_test.sh <<<")
print()

start_time = time.time()
zeroconf = Zeroconf()
listener = CrossFlowListener(start_time)
browser = ServiceBrowser(zeroconf, "_crossflow-x._tcp.local.", listener)

time.sleep(duration)

browser.cancel()
zeroconf.close()

announcer.wait(timeout=10)

total_time = (time.time() - start_time) * 1000

print()
print("=" * 60)
print("Test Summary")
print("=" * 60)
print(f"Total time: {total_time:.0f} ms")
print(f"Discoveries: {len(listener.discoveries)}")

win_discoveries = [d for d in listener.discoveries if "Win" in d["service_name"]]
debian_discoveries = [d for d in listener.discoveries if "Debian" in d["service_name"] or "linux" in d.get("properties", {}).get("plat", "")]

print(f"Windows services discovered: {len(win_discoveries)}")
print(f"Debian/Linux services discovered: {len(debian_discoveries)}")

evidence = {
    "test_type": "Cross-Platform mDNS Interoperability",
    "windows_side": {
        "platform": sys.platform,
        "timestamp": datetime.now(timezone.utc).isoformat(),
        "announcer": "CFX-WinCrossTest",
        "node_id": "1:500",
        "topology": "phy-cross-test",
    },
    "listener_discoveries": listener.discoveries,
    "summary": {
        "total_discoveries": len(listener.discoveries),
        "windows_discoveries": len(win_discoveries),
        "debian_discoveries": len(debian_discoveries),
    }
}

print()
print("=== Evidence JSON ===")
print(json.dumps(evidence, indent=2))

if debian_discoveries:
    print("\n*** Test A SUCCESS: Windows listener discovered Debian announcer! ***")
else:
    print("\n*** Test A: No Debian announcer discovered (may need to run Debian side) ***")

print()
print("=== Announcer Output ===")
print(announcer.stdout.read())
