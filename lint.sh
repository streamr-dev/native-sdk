#!/bin/bash

set -e

# Verify that the per-package copies of the CMake helper files match the
# canonical versions in cmake/ (see sync-cmake-files.sh).
./sync-cmake-files.sh --check

# Parse MonorepoPackages.cmake and loop through them
for package in $(cat MonorepoPackages.cmake | grep -v "set(MonorepoPackages" | grep -v ")"); do
    echo ""
    echo "Running lint.sh for $package"
    cd packages/$package && ./lint.sh && cd ../..
done
