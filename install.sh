#!/bin/bash

set -e

# Check if VCPKG_ROOT is set and points to the correct directory
if [ -z "$VCPKG_ROOT" ]; then
    echo "Error: VCPKG_ROOT is not set. Please run 'source install-prerequisities.sh' or 'source setenvs.sh' before running this script."
    exit 1
fi

EXPECTED_VCPKG_ROOT="$(pwd)/vcpkg"
if [ "$VCPKG_ROOT" != "$EXPECTED_VCPKG_ROOT" ]; then
    echo "Error: VCPKG_ROOT points to the vcpkg of another project '$VCPKG_ROOT'. Please run 'source install-prerequisities.sh' or 'source setenvs.sh' before running this script."
    exit 1
fi

# Parse command-line options
PROD_BUILD=false

while [[ "$#" -gt 0 ]]; do
    case $1 in
        --prod) PROD_BUILD=true ;;
        *) echo "Unknown parameter passed: $1. Usage: ./install.sh [--prod]"; exit 1 ;;
    esac
    shift
done

# Set build type based on the --prod flag
if [ "$PROD_BUILD" = true ]; then
    BUILD_TYPE="Release"
else
    BUILD_TYPE="Debug"
fi

git config core.hooksPath .githooks
./merge-dependencies.sh

vcpkg install --x-install-root=build/vcpkg_installed

# Call build for all monorepo packages in their own build directories
for package in $(cat MonorepoPackages.cmake | grep -v "set(MonorepoPackages" | grep -v ")"); do
    cd packages/$package/build && cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE .. && cmake --build . && cd ../../..
done

# Call build for the root project
echo "Building root project"
cd build && cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE .. && cmake --build . && cd ..

