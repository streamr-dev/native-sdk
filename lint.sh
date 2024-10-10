#!/bin/bash

set -e

# Parse MonorepoPackages.cmake and loop through them
for package in $(cat MonorepoPackages.cmake | grep -v "set(MonorepoPackages" | grep -v ")"); do
    echo ""
    echo "Running lint.sh for $package"
    cd packages/$package && ./lint.sh && cd ../..
done
