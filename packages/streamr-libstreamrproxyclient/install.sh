#!/bin/bash

# Parse command-line options
PROD_BUILD=false
TARGET_TRIPLET=""

while [[ "$#" -gt 0 ]]; do
    case $1 in
        --prod) PROD_BUILD=true ;;
        --iphone) TARGET_TRIPLET="arm64-ios" ;;
        --android) TARGET_TRIPLET="arm64-android" ;;
        *) echo "Unknown parameter passed: $1. Usage: ./install.sh [--prod] [--iphone] [--android]"; exit 1 ;;
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

if [ -n "$TARGET_TRIPLET" ]; then
    cd build && cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DVCPKG_TARGET_TRIPLET=$TARGET_TRIPLET .. && cmake --build . && cd ..
else
    cd build && cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE .. && cmake --build . && cd ..
fi
