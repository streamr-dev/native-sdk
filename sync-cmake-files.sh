#!/bin/bash

# The CMake helper files that every package carries a copy of are kept
# canonical in cmake/. Each package has its own copy (instead of a symlink
# or a shared include) so that a package can be published as a standalone
# vcpkg package later — this duplication is intentional.
#
# Usage:
#   ./sync-cmake-files.sh          copy the canonical files into all packages
#   ./sync-cmake-files.sh --check  fail if any package copy differs from the
#                                  canonical file (used by lint.sh and CI)

set -e
cd "$(dirname "$0")"

SYNCED_FILES="homebrewClang.cmake monorepoPackage.cmake StreamrModules.cmake"

MODE_CHECK=false
if [ "$1" = "--check" ]; then
    MODE_CHECK=true
fi

STATUS=0
for package in $(cat MonorepoPackages.cmake | grep -v "set(MonorepoPackages" | grep -v ")"); do
    for file in $SYNCED_FILES; do
        if [ "$MODE_CHECK" = true ]; then
            if ! diff -q "cmake/$file" "packages/$package/$file" >/dev/null 2>&1; then
                echo "OUT OF SYNC: packages/$package/$file differs from cmake/$file"
                STATUS=1
            fi
        else
            cp "cmake/$file" "packages/$package/$file"
            echo "synced packages/$package/$file"
        fi
    done
done

if [ "$MODE_CHECK" = true ] && [ "$STATUS" -ne 0 ]; then
    echo ""
    echo "Run ./sync-cmake-files.sh to update the package copies from cmake/."
    exit 1
fi
