#!/bin/bash

set -e

# Parse command-line options
PROD_BUILD=false
TARGET_TRIPLET=""
CHAINLOAD_TOOLCHAIN_FILE=""
PLATFORM=""
ANDROID_ABI="arm64-v8a"
ANDROID_PLATFORM=24

while [[ "$#" -gt 0 ]]; do
    case $1 in
        --prod) PROD_BUILD=true ;;
        --ios) TARGET_TRIPLET="arm64-ios"; CHAINLOAD_TOOLCHAIN_FILE="$(pwd)/../../overlaytriplets/arm64-ios.cmake"; PLATFORM="OS64" ;;
        --android) TARGET_TRIPLET="arm64-android"; CHAINLOAD_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake" ;;
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
    export PLATFORM=$PLATFORM
    echo "Target platform: $TARGET_TRIPLET"
fi

if [ -n "$TARGET_TRIPLET" ]; then
    if [ "$TARGET_TRIPLET" = "arm64-android" ]; then
        cd build && cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DVCPKG_TARGET_TRIPLET=$TARGET_TRIPLET -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=$CHAINLOAD_TOOLCHAIN_FILE -DANDROID_ABI=$ANDROID_ABI -DANDROID_PLATFORM=$ANDROID_PLATFORM .. && cmake --build . && cd .. && cmake --install build
    else
        cd build && cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DVCPKG_TARGET_TRIPLET=$TARGET_TRIPLET -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=$CHAINLOAD_TOOLCHAIN_FILE -DPLATFORM=$PLATFORM .. && cmake --build . && cd .. && cmake --install build
    fi
else
    cd build && cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE .. && cmake --build . && cmake --install . && cd ..
fi
