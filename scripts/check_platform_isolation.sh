#!/bin/bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CORE_DIR="$REPO_ROOT/core"

PATTERNS=(
    'CGEvent\.h'
    'CGEventTap\.h'
    'CGEventType'
    'CGEventRef'
    'NSScreen\.h'
    'CGDisplay\.h'
    'ApplicationServices'
    'CoreGraphics'
    'CoreFoundation'
    'Carbon/Carbon\.h'
    '#ifdef __APPLE__'
    '#if defined(__APPLE__)'
    'kCGEvent'
    'CGEventTapCreate'
    'AXIsProcessTrusted'
)

VIOLATIONS=0

for pattern in "${PATTERNS[@]}"; do
    if grep -rn "$pattern" "$CORE_DIR" --include='*.hpp' --include='*.cpp' --include='*.h' 2>/dev/null; then
        echo "VIOLATION: Pattern '$pattern' found in core/"
        VIOLATIONS=$((VIOLATIONS + 1))
    fi
done

if [ "$VIOLATIONS" -gt 0 ]; then
    echo "FAIL: $VIOLATIONS platform isolation violation(s) found in core/"
    exit 1
fi

echo "PASS: No platform headers found in core/"
exit 0