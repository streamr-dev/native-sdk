#!/bin/bash
#
# On-device libc++ smoke test for the Homebrew-libc++ XCFramework.
#
# Builds the iOSUnitTesting APP (not the broken unit-test bundle) with a tiny
# Swift smoke test that calls the proxyclient C API, installs it on a connected
# iPhone, launches it, and captures the console. The app exercises the
# Homebrew-libc++-built library (folly, exceptions, exception_ptr) on the real
# device runtime and prints STREAMR_SMOKE_RESULT: PASS/FAIL.
#
# Usage:
#   IOS_DEVELOPMENT_TEAM=25ZBRH2X7A ./iossmoke.sh
# (defaults to the Personal Team 25ZBRH2X7A if unset)

set -euo pipefail
cd "$(dirname "$0")"
ROOT="$(pwd)"

TEAM="${IOS_DEVELOPMENT_TEAM:-25ZBRH2X7A}"
BASE_BUNDLE_ID="testing.iOSUnitTesting"
BUNDLE_ID="${BASE_BUNDLE_ID}.${TEAM}"
PROJ="test/ios/iOSUnitTesting/iOSUnitTesting.xcodeproj"
DERIVED="build/ios-smoke"
CONSOLE_LOG="build/ios-smoke-console.log"

# --- find the connected device ---
# Pick the first paired/connected physical device (a UUID-shaped identifier).
DEVICE_ID=$(xcrun devicectl list devices 2>/dev/null \
  | awk '/[0-9A-Fa-f]{8}-([0-9A-Fa-f]{4}-){3}[0-9A-Fa-f]{12}/ {for (i=1;i<=NF;i++) if ($i ~ /^[0-9A-Fa-f]{8}-([0-9A-Fa-f]{4}-){3}[0-9A-Fa-f]{12}$/) {print $i; exit}}')
if [ -z "${DEVICE_ID:-}" ]; then
    echo "ERROR: no paired iOS device found (xcrun devicectl list devices)." >&2
    exit 2
fi
echo "Device:      $DEVICE_ID"
echo "Team:        $TEAM"
echo "Bundle id:   $BUNDLE_ID"

# --- build + sign for the device ---
echo "=== building app for device ==="
xcodebuild \
    -project "$PROJ" \
    -scheme iOSUnitTesting \
    -configuration Debug \
    -destination "id=$DEVICE_ID" \
    -derivedDataPath "$DERIVED" \
    DEVELOPMENT_TEAM="$TEAM" \
    CODE_SIGN_STYLE=Automatic \
    PRODUCT_BUNDLE_IDENTIFIER="$BUNDLE_ID" \
    IPHONEOS_DEPLOYMENT_TARGET=26.0 \
    SWIFT_OBJC_BRIDGING_HEADER="$ROOT/test/ios/iOSUnitTesting/iOSUnitTesting/StreamrBridge.h" \
    HEADER_SEARCH_PATHS="$ROOT/packages/streamr-libstreamrproxyclient/include" \
    FRAMEWORK_SEARCH_PATHS="$ROOT/dist/ios" \
    -allowProvisioningUpdates \
    -allowProvisioningDeviceRegistration \
    build

APP=$(find "$DERIVED/Build/Products" -name 'iOSUnitTesting.app' -maxdepth 3 | head -1)
if [ -z "${APP:-}" ]; then echo "ERROR: built .app not found under $DERIVED" >&2; exit 3; fi
echo "Built app:   $APP"

# --- install + launch, capturing the console until the app exits ---
echo "=== installing on device ==="
xcrun devicectl device install app --device "$DEVICE_ID" "$APP"

echo "=== launching (console streams until the app exits) ==="
: > "$CONSOLE_LOG"
xcrun devicectl device process launch \
    --console --terminate-existing \
    --device "$DEVICE_ID" "$BUNDLE_ID" 2>&1 | tee "$CONSOLE_LOG" || true

echo
echo "=== RESULT ==="
if grep -q 'STREAMR_SMOKE_RESULT: PASS' "$CONSOLE_LOG"; then
    grep -E 'STREAMR_SMOKE' "$CONSOLE_LOG" || true
    echo ">>> PASS: Homebrew-libc++ library ran on the device (exceptions included)."
    exit 0
elif grep -q 'STREAMR_SMOKE_RESULT: FAIL' "$CONSOLE_LOG"; then
    grep -E 'STREAMR_SMOKE' "$CONSOLE_LOG" || true
    echo ">>> FAIL: library ran but the smoke test's pass condition was not met."
    exit 1
else
    echo ">>> INCONCLUSIVE: no STREAMR_SMOKE_RESULT line captured on the console."
    echo "    Check the phone screen (the app shows SMOKE PASS/FAIL) and $CONSOLE_LOG."
    exit 4
fi
