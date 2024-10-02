#!/bin/bash

# This script needs to be run using "source install-prerequisities.sh"
# Calling it directly will not work

git submodule update --init --recursive

TEMP_PROFILE_CONTENTS=""

if [[ "$OSTYPE" == "darwin"* ]]; then    
    #PROFILE_FILE=~/.zprofile
    PROFILE_FILE=./setenvs.sh
    
    xcode-select --install
    export HOMEBREW_PREFIX=$(brew --prefix)
    if [[ -n "$GITHUB_ENV" ]]; then
        echo "HOMEBREW_PREFIX=$(brew --prefix)" >> $GITHUB_ENV
    fi
    TEMP_PROFILE_CONTENTS+="export HOMEBREW_PREFIX=$(brew --prefix)\n"

    brew install jq
    brew uninstall llvm
    brew install llvm@17
    brew install cmake
    brew install pkg-config
   
    brew link --overwrite --force llvm@17
    
    rm -f $HOMEBREW_PREFIX/bin/clang-tidy
    
else
    #PROFILE_FILE=~/.profile
    PROFILE_FILE=./setenvs.sh
    wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add -
    sudo apt-add-repository 'deb http://apt.llvm.org/noble/ llvm-toolchain-noble main'
    sudo apt-get update
    sudo apt-get remove -y clang-format clangd
    sudo apt-get install -y build-essential cmake ninja-build jq clang-format-18 clangd-18
    sudo ln -s /usr/bin/clang-format-18 /usr/bin/clang-format
    sudo ln -s /usr/bin/clangd-18 /usr/bin/clangd
    if [[ -n "$GITHUB_ENV" ]]; then
        echo "CC=gcc-14" >> $GITHUB_ENV
        echo "CXX=g++-14" >> $GITHUB_ENV
    fi
fi

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
