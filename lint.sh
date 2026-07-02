#!/bin/bash

set -e

# Verify that the per-package copies of the CMake helper files match the
# canonical versions in cmake/ (see sync-cmake-files.sh).
if ! ./sync-cmake-files.sh --check; then
    echo "::error title=cmake helper files out of sync::Run ./sync-cmake-files.sh and commit the result."
    exit 1
fi

# Parse MonorepoPackages.cmake and loop through them
for package in $(cat MonorepoPackages.cmake | grep -v "set(MonorepoPackages" | grep -v ")"); do
    echo ""
    echo "Running lint.sh for $package"
    LOG_FILE=$(mktemp)
    if (cd packages/$package && ./lint.sh) > "$LOG_FILE" 2>&1; then
        cat "$LOG_FILE"
        rm -f "$LOG_FILE"
    else
        cat "$LOG_FILE"
        # Emit the failure tail as a GitHub Actions error annotation so the
        # cause is visible in the Checks UI without opening the raw logs.
        MSG=$(tail -40 "$LOG_FILE" | sed 's/%/%25/g' | awk '{printf "%s%%0A", $0}')
        echo "::error title=lint failed for ${package}::${MSG}"
        rm -f "$LOG_FILE"
        exit 1
    fi
done
