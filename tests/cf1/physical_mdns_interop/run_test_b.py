import subprocess
import time
import sys
import os
import json

base_dir = os.path.dirname(__file__)

announcer = subprocess.Popen([
    sys.executable,
    os.path.join(base_dir, "mdns_announcer_py.py"),
    "--service", "CFX-WinCrossTest",
    "--port", "5353",
    "--topology", "phy-cross-test",
    "--node-high", "1",
    "--node-low", "500",
    "--duration", "20"
], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)

time.sleep(3)

listener = subprocess.run([
    sys.executable,
    os.path.join(base_dir, "mdns_listener_py.py"),
    "--timeout", "15"
], capture_output=True, text=True)

print("=== TEST B: Windows Announcer → Debian Listener ===")
print("(Windows listener running to detect any mDNS services on LAN)")
print()
print("=== LISTENER OUTPUT ===")
print(listener.stdout)
if listener.stderr:
    print("STDERR:", listener.stderr)

announcer.wait(timeout=5)
print("\n=== ANNOUNCER OUTPUT ===")
print(announcer.stdout.read())

print("\n=== INSTRUCTIONS FOR DEBIAN SIDE ===")
print("On the Debian machine, run:")
print("  cd /path/to/share/CrossFlow-X/physical_mdns_interop")
print("  chmod +x test_b_debian_listener.sh")
print("  ./test_b_debian_listener.sh")
print()
print("Or manually:")
print("  avahi-browse -r _crossflow-x._tcp")