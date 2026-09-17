#!/bin/bash

# CF2 Group 2 macOS Physical Evidence Collection
# Hard Gate Pipeline: E0 -> E1 -> E2 -> E3 -> E4 -> E5
# Each gate is a hard checkpoint. Configure/Build failure = immediate exit.

EVIDENCE_FILE="macos_physical_evidence.txt"
BUILD_DIR="build_mac"
GATE_LOG=""
PASS_COUNT=0
FAIL_COUNT=0

gate_pass() { GATE_LOG="${GATE_LOG}PASS: $1\n"; PASS_COUNT=$((PASS_COUNT + 1)); }
gate_fail() { GATE_LOG="${GATE_LOG}FAIL: $1\n"; FAIL_COUNT=$((FAIL_COUNT + 1)); }

# ========== E0: Environment ==========
echo "=== CF2 Group 2 macOS Physical Evidence Collection ==="
DATE=$(date)
MACOS_VERSION=$(sw_vers -productVersion)
ARCH=$(uname -m)
COMMIT=$(git rev-parse HEAD)
echo "Date: $DATE"
echo "macOS Version: $MACOS_VERSION"
echo "Architecture: $ARCH"
echo "Commit: $COMMIT"
echo ""

# ========== E1: Configure (Hard Gate) ==========
echo "[E1] Configure..."
if cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release > /tmp/cfx_configure.log 2>&1; then
    echo "  [PASS] Configure"
    gate_pass "E1 Configure"
else
    echo "  [FAIL] Configure"
    tail -20 /tmp/cfx_configure.log
    gate_fail "E1 Configure"
    echo "  >> Pipeline STOPPED: Configure failed"
    exit 1
fi

# ========== E2: Build (Hard Gate) ==========
echo ""
echo "[E2] Build..."
if cmake --build "$BUILD_DIR" --config Release > /tmp/cfx_build.log 2>&1; then
    echo "  [PASS] Build"
    gate_pass "E2 Build"
else
    echo "  [FAIL] Build"
    tail -30 /tmp/cfx_build.log
    gate_fail "E2 Build"
    echo "  >> Pipeline STOPPED: Build failed (CTest will NOT be executed)"
    exit 1
fi

# ========== E3: CTest ==========
echo ""
echo "[E3] CTest..."
CTEST_OUTPUT=$(ctest --test-dir "$BUILD_DIR" --build-config Release --output-on-failure 2>&1)
CTEST_EXIT=$?

# Parse CTest results: distinguish PASS, FAIL, and Not Run
CTEST_PASSED=$(echo "$CTEST_OUTPUT" | grep -c "Passed" || true)
CTEST_FAILED=$(echo "$CTEST_OUTPUT" | grep -c "\*\*\*Failed" || true)
CTEST_NOTRUN=$(echo "$CTEST_OUTPUT" | grep -c "\*\*\*Not Run" || true)

echo "$CTEST_OUTPUT" | grep -E "Passed|Failed|Not Run" | sed 's/^/  /'

if [ $CTEST_EXIT -eq 0 ] && [ "$CTEST_NOTRUN" -eq 0 ]; then
    echo "  [PASS] CTest: ${CTEST_PASSED} passed, 0 failed, 0 not run"
    gate_pass "E3 CTest"
else
    echo "  [WARN] CTest: ${CTEST_PASSED} passed, ${CTEST_FAILED} failed, ${CTEST_NOTRUN} not run"
    gate_fail "E3 CTest"
fi

# ========== E4: Physical CGEventTap Test ==========
echo ""
echo "[E4] Physical CGEventTap Test..."

# Find the test binary
PHYSICAL_TEST=""
for path in \
    "$BUILD_DIR/tests/mac/Release/test_mac_platform" \
    "$BUILD_DIR/tests/mac/test_mac_platform"; do
    if [ -x "$path" ]; then
        PHYSICAL_TEST="$path"
        break
    fi
done

if [ -z "$PHYSICAL_TEST" ]; then
    echo "  [FAIL] test_mac_platform executable not found"
    gate_fail "E4 Physical CGEventTap"
    echo "  >> Cannot run physical evidence test"
else
    echo "  Binary: $PHYSICAL_TEST"
    PHYSICAL_OUTPUT=$("$PHYSICAL_TEST" 2>&1)
    PHYSICAL_EXIT=$?
    echo "$PHYSICAL_OUTPUT"

    # Extract physical evidence from output
    TAP_ACTIVE=$(echo "$PHYSICAL_OUTPUT" | grep -c "CGEventTap installation failed" || true)
    CALLBACK_COUNT=$(echo "$PHYSICAL_OUTPUT" | grep "onEvent called" | grep -oE '[0-9]+' || echo "0")
    RECEIVED_KINDS=$(echo "$PHYSICAL_OUTPUT" | grep "kinds=" | grep -oE '0x[0-9a-fA-F]+' || echo "0x00000000")
    PHYSICAL_PASS=$(echo "$PHYSICAL_OUTPUT" | grep -c "test_macos_physical_cgeventtap_chain" || true)

    if [ $PHYSICAL_EXIT -eq 0 ] && [ "$TAP_ACTIVE" -eq 0 ]; then
        echo "  [PASS] Physical CGEventTap: callback=$CALLBACK_COUNT, kinds=$RECEIVED_KINDS"
        gate_pass "E4 Physical CGEventTap"
    else
        echo "  [FAIL] Physical CGEventTap (exit=$PHYSICAL_EXIT)"
        gate_fail "E4 Physical CGEventTap"
    fi
fi

# ========== E5: Evidence Manifest ==========
echo ""
echo "[E5] Generating Evidence Manifest..."

cat > "$EVIDENCE_FILE" << EVIDENCE_EOF
=== CF2 Group 2 macOS Physical Evidence ===
Date: $DATE
macOS Version: $MACOS_VERSION
Architecture: $ARCH
Commit: $COMMIT

--- Gate Results ---
$(echo -e "$GATE_LOG")
--- Build Result ---
Configure: PASS
Build: PASS

--- CTest Summary ---
Passed: $CTEST_PASSED
Failed: $CTEST_FAILED
Not Run: $CTEST_NOTRUN

--- Physical CGEventTap Evidence ---
Tap Active: $([ "$TAP_ACTIVE" -eq 0 ] && echo "YES" || echo "NO")
Callback Count: $CALLBACK_COUNT
Received Kinds: $RECEIVED_KINDS
Physical Test Exit: $PHYSICAL_EXIT

--- Physical Test Full Output ---
$PHYSICAL_OUTPUT

--- Summary ---
Total Gates: $((PASS_COUNT + FAIL_COUNT))
Passed: $PASS_COUNT
Failed: $FAIL_COUNT
EVIDENCE_EOF

echo "  Evidence written to $EVIDENCE_FILE"

echo ""
echo "=== Evidence Collection Complete ==="
echo "Gates: $PASS_COUNT passed, $FAIL_COUNT failed"

if [ "$FAIL_COUNT" -eq 0 ]; then
    exit 0
else
    exit 1
fi
