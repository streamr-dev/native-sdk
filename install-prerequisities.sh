#!/bin/bash

# This script needs to be run using "source install-prerequisities.sh"
# Calling it directly will not work

git submodule update --init --recursive

TEMP_PROFILE_CONTENTS=""

if [[ "$OSTYPE" == "darwin"* ]]; then    
    #PROFILE_FILE=~/.zprofile
    PROFILE_FILE=./setenvs.sh
    
    xcode-select --install
    brew install jq
    brew install llvm
    brew install cmake
    brew install pkg-config
   
    rm -f /opt/homebrew/bin/clang-format
    rm -f /opt/homebrew/bin/clangd
    ln -s /opt/homebrew/opt/llvm/bin/clang-format /opt/homebrew/bin/clang-format
    ln -s /opt/homebrew/opt/llvm/bin/clangd /opt/homebrew/bin/clangd
    rm -f /opt/homebrew/bin/clang-tidy

else
    #PROFILE_FILE=~/.profile
    PROFILE_FILE=./setenvs.sh
    wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add -
    sudo apt-add-repository 'deb http://apt.llvm.org/jammy/ llvm-toolchain-jammy main'
    sudo apt-get update
    sudo apt-get install -y build-essential cmake ninja-build jq clang-format clangd
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

echo $TEMP_PROFILE_CONTENTS

# Remove old version of the block
sed -i '/# streamr-native-sdk added start/,/# streamr-native-sdk added end/d' $PROFILE_FILE
# Add new block to profile file
echo -e "# streamr-native-sdk added start\n$TEMP_PROFILE_CONTENTS# streamr-native-sdk added end" >> $PROFILE_FILE

git config core.hooksPath .githooks
