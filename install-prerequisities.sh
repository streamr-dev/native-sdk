#!/bin/bash

# This script needs to be run using "source install-prerequisities.sh"
# Calling it directly will not work

git submodule add https://github.com/microsoft/vcpkg.git
git submodule add https://github.com/lljbash/clangd-tidy.git
git submodule update --init --recursive

TEMP_PROFILE_CONTENTS=""

if [[ "$OSTYPE" == "darwin"* ]]; then    
    #PROFILE_FILE=~/.zprofile
    PROFILE_FILE=/tmp/testfile
    xcode-select --install
    brew install jq
    brew install llvm
    brew install cmake
    brew install pkg-config
    ln -s /opt/homebrew/Cellar/llvm/18.1.6/bin/clang-format /opt/homebrew/bin/clang-format
    ln -s /opt/homebrew/Cellar/llvm/18.1.6/bin/clangd /opt/homebrew/bin/clangd
    brew install trunk

    rm -f /opt/homebrew/bin/clang-tidy

else
    PROFILE_FILE=~/.profile
    sudo apt-add-repository http://apt.llvm.org/jammy/llvm-toolchain-jammy main
    sudo apt-get update
    sudo apt-get install -y build-essential cmake ninja-build jq clang-format
    
    curl https://get.trunk.io -fsSL | bash -s -- -y
fi

cd clangd-tidy
rm -f clang-tidy
ln -s clangd-tidy clang-tidy
cd ..

TEMP_PROFILE_CONTENTS+="export PATH=$(pwd)/clangd-tidy:\$PATH\n"

CLANGD_TIDY_PATH="$(pwd)/clangd-tidy"

if [[ ":$PATH:" != *":$CLANGD_TIDY_PATH:"* ]]; then
    export PATH="$CLANGD_TIDY_PATH:$PATH"
fi

cd vcpkg
./bootstrap-vcpkg.sh -disableMetrics
cd ..

export VCPKG_ROOT=$(pwd)/vcpkg

if [[ ":$PATH:" != *":$VCPKG_ROOT:"* ]]; then
    export PATH=$PATH:$VCPKG_ROOT
fi

# Add VCPKG_ROOT environment variable
TEMP_PROFILE_CONTENTS+="export VCPKG_ROOT=$(pwd)/vcpkg\n"

# Add PATH environment variable to include VCPKG_ROOT
TEMP_PROFILE_CONTENTS+="export PATH=\$VCPKG_ROOT:\$PATH\n"

echo $TEMP_PROFILE_CONTENTS

# Remove old version of the block
sed -i.bak '/# streamr-native-sdk added start/,/# streamr-native-sdk added end/d' $PROFILE_FILE
# Add new block to profile file
echo -e "# streamr-native-sdk added start\n$TEMP_PROFILE_CONTENTS# streamr-native-sdk added end" >> $PROFILE_FILE
