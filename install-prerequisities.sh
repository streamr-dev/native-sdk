#!/bin/bash

# This script needs to be run using "source install-prerequisities.sh"
# Calling it directly will not work

git submodule update --init --recursive

TEMP_PROFILE_CONTENTS=""

if [[ "$OSTYPE" == "darwin"* ]]; then    
    #PROFILE_FILE=~/.zprofile
    PROFILE_FILE=./setenvs.sh
    
    xcode-select --install || true

    # iOS builds target deployment target 26.0 and need the iOS 26 SDK.
    XCODE_MAJOR=$(xcodebuild -version 2>/dev/null | awk 'NR==1{print int($2)}')
    if [ -n "$XCODE_MAJOR" ] && [ "$XCODE_MAJOR" -lt 26 ]; then
        echo "WARNING: Xcode $XCODE_MAJOR found; Xcode 26 or newer is required for iOS builds."
    fi

    export HOMEBREW_PREFIX=$(brew --prefix)
    if [[ -n "$GITHUB_ENV" ]]; then
        echo "HOMEBREW_PREFIX=$(brew --prefix)" >> $GITHUB_ENV
    fi
    TEMP_PROFILE_CONTENTS+="export HOMEBREW_PREFIX=$(brew --prefix)\n"

    brew install jq || true
    # Latest LLVM (keg-only: not linked into $HOMEBREW_PREFIX/bin; the build
    # finds it via the LLVM_PREFIX environment variable exported below).
    brew install llvm || true
    brew upgrade llvm || true
    brew install cmake || true
    brew install ninja || true
    brew install pkg-config || true

    export LLVM_PREFIX=$(brew --prefix llvm)
    if [[ -n "$GITHUB_ENV" ]]; then
        echo "LLVM_PREFIX=$LLVM_PREFIX" >> $GITHUB_ENV
    fi
    TEMP_PROFILE_CONTENTS+="export LLVM_PREFIX=$(brew --prefix llvm)\n"

    # clangd (used by clangd-tidy for linting) must be able to parse the
    # libc++ headers of the LLVM in use, so it comes from the same keg.
    export PATH="$LLVM_PREFIX/bin:$PATH"
    if [[ -n "$GITHUB_PATH" ]]; then
        echo "$LLVM_PREFIX/bin" >> $GITHUB_PATH
    fi
    TEMP_PROFILE_CONTENTS+="export PATH=\$LLVM_PREFIX/bin:\$PATH\n"

else
    #PROFILE_FILE=~/.profile
    PROFILE_FILE=./setenvs.sh
    wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | sudo tee /etc/apt/trusted.gpg.d/apt-llvm-org.asc > /dev/null
    sudo apt-add-repository -y 'deb http://apt.llvm.org/noble/ llvm-toolchain-noble-22 main'
    sudo apt-get update
    # clang-22 + libc++: Linux builds use the same LLVM toolchain family and
    # standard library as macOS/iOS/Android (uniform C++26 feature set and a
    # single modules implementation). The lint tools (clangd, clang-format)
    # come from the same LLVM version on every platform: clangd must be able
    # to parse libc++ 22 headers, and clang-format versions must not diverge
    # between macOS and Linux or the format check flip-flops.
    sudo apt-get install -y build-essential cmake ninja-build jq \
        clang-22 lld-22 clang-tools-22 clangd-22 libc++-22-dev libc++abi-22-dev \
        clang-format-22 \
        autoconf autoconf-archive automake libtool
    sudo rm -f /usr/bin/clang-format
    sudo rm -f /usr/bin/clangd
    sudo ln -s /usr/bin/clang-format-22 /usr/bin/clang-format
    sudo ln -s /usr/bin/clangd-22 /usr/bin/clangd
    export CC=clang-22
    export CXX=clang++-22
    if [[ -n "$GITHUB_ENV" ]]; then
        echo "CC=clang-22" >> $GITHUB_ENV
        echo "CXX=clang++-22" >> $GITHUB_ENV
    fi
    TEMP_PROFILE_CONTENTS+="export CC=clang-22\n"
    TEMP_PROFILE_CONTENTS+="export CXX=clang++-22\n"
fi

# Use the Ninja generator for all CMake builds. Ninja is faster than
# Makefiles and is required by CMake's C++ modules support (upcoming
# modules migration). NOTE: build dirs configured with the old Makefile
# generator must be cleaned once after this change (./clean.sh).
export CMAKE_GENERATOR=Ninja
if [[ -n "$GITHUB_ENV" ]]; then
    echo "CMAKE_GENERATOR=Ninja" >> $GITHUB_ENV
fi
TEMP_PROFILE_CONTENTS+="export CMAKE_GENERATOR=Ninja\n"

cd clangd-tidy
rm -f clang-tidy
ln -s clangd-tidy clang-tidy
cd ..

TEMP_PROFILE_CONTENTS+="export PATH=$(pwd)/clangd-tidy:\$PATH\n"

CLANGD_TIDY_PATH="$(pwd)/clangd-tidy"

if [[ ":$PATH:" != *":$CLANGD_TIDY_PATH:"* ]]; then
    export PATH="$CLANGD_TIDY_PATH:$PATH"
    if [[ -n "$GITHUB_PATH" ]]; then
        echo "$CLANGD_TIDY_PATH" >> $GITHUB_PATH
    fi
fi

cd vcpkg
./bootstrap-vcpkg.sh -disableMetrics
cd ..

export VCPKG_ROOT=$(pwd)/vcpkg
if [[ -n "$GITHUB_ENV" ]]; then
    echo "VCPKG_ROOT=$VCPKG_ROOT" >> $GITHUB_ENV
fi

export VCPKG_OVERLAY_TRIPLETS=$(pwd)/overlaytriplets
if [[ -n "$GITHUB_ENV" ]]; then
    echo "VCPKG_OVERLAY_TRIPLETS=$VCPKG_OVERLAY_TRIPLETS" >> $GITHUB_ENV
fi

export VCPKG_OVERLAY_PORTS=$(pwd)/overlayports
if [[ -n "$GITHUB_ENV" ]]; then
    echo "VCPKG_OVERLAY_PORTS=$VCPKG_OVERLAY_PORTS" >> $GITHUB_ENV
fi

if [[ ":$PATH:" != *":$VCPKG_ROOT:"* ]]; then
    export PATH=$PATH:$VCPKG_ROOT
    if [[ -n "$GITHUB_PATH" ]]; then
        echo "$VCPKG_ROOT" >> $GITHUB_PATH
    fi
fi

# Add VCPKG_ROOT environment variable
TEMP_PROFILE_CONTENTS+="export VCPKG_ROOT=$(pwd)/vcpkg\n"

# Add PATH environment variable to include VCPKG_ROOT
TEMP_PROFILE_CONTENTS+="export PATH=\$VCPKG_ROOT:\$PATH\n"

# Add VCPKG_OVERLAY_TRIPLETS environment variable
TEMP_PROFILE_CONTENTS+="export VCPKG_OVERLAY_TRIPLETS=$(pwd)/overlaytriplets\n"

# Add VCPKG_OVERLAY_PORTS environment variable
TEMP_PROFILE_CONTENTS+="export VCPKG_OVERLAY_PORTS=$(pwd)/overlayports\n"

echo $TEMP_PROFILE_CONTENTS

touch $PROFILE_FILE
# Remove old version of the block
# sed -i '/# streamr-native-sdk added start/,/# streamr-native-sdk added end/d' $PROFILE_FILE
# Add new block to profile file
echo -e "# streamr-native-sdk added start\n$TEMP_PROFILE_CONTENTS# streamr-native-sdk added end" > $PROFILE_FILE

git config core.hooksPath .githooks
