#!/bin/bash
set -euo pipefail

echo "=== CF2 Group 4 Phase E: macOS Physical Evidence Runner ==="
echo "Host: $(hostname)"
echo "User: $(whoami)"
echo "Date: $(date)"
echo "macOS: $(sw_vers -productVersion)"
echo "Arch: $(uname -m)"
echo ""

if [ "$(whoami)" = "root" ]; then
    echo "[ERROR] Must run as normal user (hunt), NOT root. TCC permissions do not inherit root."
    exit 1
fi

REPO_DIR="${1:-/Users/hunt/CrossFlow-X}"
echo "Repo: $REPO_DIR"

if [ ! -d "$REPO_DIR" ]; then
    echo "[ERROR] Repo directory not found: $REPO_DIR"
    exit 1
fi

cd "$REPO_DIR"

echo ""
echo "=== Step 1: Pull latest ==="
git pull origin main 2>/dev/null || echo "[WARN] git pull failed, using current state"

echo ""
echo "=== Step 2: Current HEAD ==="
git log -1 --format="SHA: %H%nMessage: %s%nDate: %ci"

echo ""
echo "=== Step 3: Build physical evidence test ==="
BUILD_DIR="build_phase_e"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake .. -DCMAKE_BUILD_TYPE=Release -G "Unix Makefiles" 2>&1 | tail -5
echo "PHASE_E_CMAKE = OK"

make physical_evidence_phase_e -j$(sysctl -n hw.ncpu)
echo "PHASE_E_BUILD = OK"

EXE_PATH="./tests/mac/physical_evidence_phase_e"
if [ ! -f "$EXE_PATH" ]; then
    echo "[ERROR] Executable not found: $EXE_PATH"
    echo "PHASE_E_BUILD = FAIL"
    exit 1
fi
echo "PHASE_E_EXECUTABLE = FOUND"

echo ""
echo "=== Step 4: Run physical evidence test ==="
echo "NOTE: You may need to grant Accessibility permission in System Settings > Privacy & Security"
echo ""

"$EXE_PATH" 2>&1 | tee phase_e_output.txt

echo ""
echo "=== Step 5: Evidence saved to $REPO_DIR/$BUILD_DIR/phase_e_output.txt ==="
echo "PHASE_E_RUN = COMPLETE"
