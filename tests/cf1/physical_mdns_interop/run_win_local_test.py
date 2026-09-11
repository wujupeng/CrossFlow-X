import subprocess
import time
import sys
import os

announcer = subprocess.Popen([
    sys.executable,
    os.path.join(os.path.dirname(__file__), "mdns_announcer_py.py"),
    "--service", "CFX-WinLocal",
    "--port", "5353",
    "--topology", "phy-local-test",
    "--node-high", "1",
    "--node-low", "300",
    "--duration", "12"
], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)

time.sleep(3)

listener = subprocess.run([
    sys.executable,
    os.path.join(os.path.dirname(__file__), "mdns_listener_py.py"),
    "--timeout", "8"
], capture_output=True, text=True)

print("=== LISTENER OUTPUT ===")
print(listener.stdout)
if listener.stderr:
    print("STDERR:", listener.stderr)

announcer.wait(timeout=5)
print("\n=== ANNOUNCER OUTPUT ===")
print(announcer.stdout.read())