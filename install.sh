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

# Check if VCPKG_OVERLAY_TRIPLETS is set
if [ -z "$VCPKG_OVERLAY_TRIPLETS" ]; then
    echo "Error: VCPKG_OVERLAY_TRIPLETS is not set. Please set it to the path of the triplets directory."
    exit 1
fi

# Parse command-line options
PROD_BUILD=false
TARGET_TRIPLET=""
CHAINLOAD_TOOLCHAIN_FILE=""
PLATFORM=""
ANDROID_ABI="arm64-v8a"
ANDROID_PLATFORM=24

STANDALONE_PACKAGES=true

while [[ "$#" -gt 0 ]]; do
    case $1 in
        --prod) PROD_BUILD=true ;;
        --ios) TARGET_TRIPLET="arm64-ios"; CHAINLOAD_TOOLCHAIN_FILE="$(pwd)/overlaytriplets/arm64-ios.cmake"; PLATFORM="OS64"; CREATE_XCFRAMEWORK=true ;;
        --android) TARGET_TRIPLET="arm64-android"; CHAINLOAD_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake";;
        # Skip the per-package standalone builds and build only the root
        # tree (MODERNIZATION.md "After the consolidation: CI speed"). The
        # standalone builds validate that each package still works as an
        # independent vcpkg-style unit — that check is host-independent, so
        # CI runs it on the macOS leg only; the other host legs pass this
        # flag. Not allowed with --ios: the XCFramework is assembled from
        # the per-package build outputs.
        --no-standalone) STANDALONE_PACKAGES=false ;;
        *) echo "Unknown parameter passed: $1. Usage: ./install.sh [--prod] [--ios] [--android] [--no-standalone]"; exit 1 ;;
    esac
    shift
done

if [ "$STANDALONE_PACKAGES" = false ] && [ "$TARGET_TRIPLET" = "arm64-ios" ]; then
    echo "Error: --no-standalone cannot be combined with --ios (the XCFramework is built from the per-package outputs)."
    exit 1
fi

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

git config core.hooksPath .githooks
./merge-dependencies.sh

if [ -n "$TARGET_TRIPLET" ]; then
    vcpkg install --x-install-root=build/vcpkg_installed --triplet=$TARGET_TRIPLET
else
    vcpkg install --x-install-root=build/vcpkg_installed
fi

# Call build for all monorepo packages in their own build directories
if [ "$STANDALONE_PACKAGES" = true ]; then
for package in $(cat MonorepoPackages.cmake | grep -v "set(MonorepoPackages" | grep -v ")"); do
    cd packages/$package/build
    if [ -n "$TARGET_TRIPLET" ]; then
        if [ "$TARGET_TRIPLET" = "arm64-android" ]; then
            cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DVCPKG_TARGET_TRIPLET=$TARGET_TRIPLET -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=$CHAINLOAD_TOOLCHAIN_FILE -DANDROID_ABI=$ANDROID_ABI -DANDROID_PLATFORM=$ANDROID_PLATFORM ..
        else
            cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DVCPKG_TARGET_TRIPLET=$TARGET_TRIPLET -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=$CHAINLOAD_TOOLCHAIN_FILE -DPLATFORM=$PLATFORM ..
        fi
    else
        cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE ..
    fi
    cmake --build .

    #if the package is streamr-libstreamrproxyclient, run cmake --install .
    if [ "$package" = "streamr-libstreamrproxyclient" ]; then
        if [ "$TARGET_TRIPLET" = "arm64-ios" ];  then
            cd ../../.. 
            ./create-streamr-xcframework.pl
            cd packages/$package/build
        fi
        cmake --install .
    fi
    cd ../../..
done
fi

# Call build for the root project
echo "Building root project"
if [ -n "$TARGET_TRIPLET" ]; then
    if [ "$TARGET_TRIPLET" = "arm64-android" ]; then
            cd build && cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DVCPKG_TARGET_TRIPLET=$TARGET_TRIPLET -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=$CHAINLOAD_TOOLCHAIN_FILE -DANDROID_ABI=$ANDROID_ABI -DANDROID_PLATFORM=$ANDROID_PLATFORM .. && cmake --build . && cd ..
        else
            cd build && cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DVCPKG_TARGET_TRIPLET=$TARGET_TRIPLET -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=$CHAINLOAD_TOOLCHAIN_FILE -DPLATFORM=$PLATFORM .. && cmake --build . && cd ..
    fi
else
    cd build && cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE .. && cmake --build . && cd ..
fi
