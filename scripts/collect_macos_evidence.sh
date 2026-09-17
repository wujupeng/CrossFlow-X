#!/bin/bash
set -e

echo "=== CF2 Group 2 macOS Physical Evidence Collection ==="
echo "Date: $(date)"
echo "macOS Version: $(sw_vers -productVersion)"
echo "Architecture: $(uname -m)"
echo "Commit: $(git rev-parse HEAD)"
echo ""

echo "[1/4] Checking Accessibility Permission..."
python3 -c "
import subprocess
result = subprocess.run(['sqlite3', '/Library/Application Support/com.apple.TCC/TCC.db', 'SELECT client,allowed FROM access WHERE service=\"kTCCServiceAccessibility\"'], capture_output=True, text=True)
print(result.stdout if result.stdout else 'Unable to query TCC database (may need sudo)')
"

echo ""
echo "[2/4] Building project..."
cmake -S . -B build_mac -DCMAKE_BUILD_TYPE=Release 2>&1 | tail -5
cmake --build build_mac --config Release 2>&1 | tail -10

echo ""
echo "[3/4] Running all tests..."
ctest --test-dir build_mac --build-config Release --output-on-failure 2>&1

echo ""
echo "[4/4] Running physical test specifically..."
./build_mac/tests/mac/Release/test_mac_platform 2>&1 || ./build_mac/tests/mac/test_mac_platform 2>&1

echo ""
echo "=== Evidence Collection Complete ==="