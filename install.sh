#!/bin/bash

# Parse command-line options
PROD_BUILD=false
TARGET_TRIPLET=""
CHAINLOAD_TOOLCHAIN_FILE=""
PLATFORM=""

while [[ "$#" -gt 0 ]]; do
    case $1 in
        --prod) PROD_BUILD=true ;;
        --ios) TARGET_TRIPLET="arm64-ios"; CHAINLOAD_TOOLCHAIN_FILE="$(pwd)/overlaytriplets/arm64-ios.cmake"; PLATFORM="OS64" ;;
        --android) TARGET_TRIPLET="arm64-android"; CHAINLOAD_TOOLCHAIN_FILE="$(pwd)/overlaytriplets/arm64-android.cmake"; PLATFORM="android" ;;
        *) echo "Unknown parameter passed: $1. Usage: ./install.sh [--prod] [--ios] [--android]"; exit 1 ;;
    esac
    shift
done

# Set build type based on the --prod flag
if [ "$PROD_BUILD" = true ]; then
    BUILD_TYPE="Release"
else
    BUILD_TYPE="Debug"
fi

# Print the target platform if specified
if [ -n "$TARGET_TRIPLET" ]; then
    echo "Target platform: $TARGET_TRIPLET"
fi

git config core.hooksPath .githooks
./merge-dependencies.sh

if [ -n "$TARGET_TRIPLET" ]; then
    vcpkg install --x-install-root=build/vcpkg_installed --triplet=$TARGET_TRIPLET
else
    vcpkg install --x-install-root=build/vcpkg_installed
fi

# Call build for all monorepo packages in their own build directories
for package in $(cat MonorepoPackages.cmake | grep -v "set(MonorepoPackages" | grep -v ")"); do
    cd packages/$package/build
    if [ -n "$TARGET_TRIPLET" ]; then
        cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DVCPKG_TARGET_TRIPLET=$TARGET_TRIPLET -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=$CHAINLOAD_TOOLCHAIN_FILE -DPLATFORM=$PLATFORM ..
    else
        cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE ..
    fi
    cmake --build .
    cd ../../..
done

# Call build for the root project
echo "Building root project"
if [ -n "$TARGET_TRIPLET" ]; then
    cd build && cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DVCPKG_TARGET_TRIPLET=$TARGET_TRIPLET -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=$CHAINLOAD_TOOLCHAIN_FILE -DPLATFORM=$PLATFORM .. && cmake --build . && cd ..
else
    cd build && cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE .. && cmake --build . && cd ..
fi

