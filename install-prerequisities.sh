#!/bin/bash

# This script needs to be run using "source install-prerequisities.sh"
# Calling it directly will not work

git submodule add https://github.com/microsoft/vcpkg.git
git submodule add https://github.com/lljbash/clangd-tidy.git
git submodule update --init --recursive

if [[ "$OSTYPE" == "darwin"* ]]; then    
    PROFILE_FILE=~/.zprofile
    xcode-select --install
    brew install jq
    brew install llvm
    brew install cmake
    brew install pkg-config
    ln -s /opt/homebrew/Cellar/llvm/18.1.6/bin/clang-format /opt/homebrew/bin/clang-format
    ln -s /opt/homebrew/Cellar/llvm/18.1.6/bin/clangd /opt/homebrew/bin/clangd
    brew install trunk

    cd clangd-tidy
    rm -f /opt/homebrew/bin/clang-tidy
    ln -s clangd-tidy /opt/homebrew/bin/clang-tidy
    cd ..
else
    PROFILE_FILE=~/.profile
    sudo apt-add-repository http://apt.llvm.org/jammy/llvm-toolchain-jammy main
    sudo apt-get update
    sudo apt-get install -y build-essential cmake ninja-build jq clang-format
    
    if [[ ":$PATH:" != *":clangd-tidy:"* ]]; then
        cd clangd-tidy
        rm -f clang-tidy
        ln -s clangd-tidy clang-tidy
        export PATH="$(pwd)/clangd-tidy:$PATH"
        echo 'export PATH=$(pwd)/clangd-tidy:$PATH' >> $PROFILE_FILE
    fi
fi

cd vcpkg
./bootstrap-vcpkg.sh -disableMetrics
cd ..

export VCPKG_ROOT=$(pwd)/vcpkg

if [[ ":$PATH:" != *":$VCPKG_ROOT:"* ]]; then
    export PATH="$VCPKG_ROOT:$PATH"
    echo 'export PATH=$VCPKG_ROOT:$PATH' >> $PROFILE_FILE
fi

# Check if the VCPKG_ROOT environment variable is already set in the profile file. 
# If it is, update it. If not, add it.

if grep -q 'export VCPKG_ROOT=' $PROFILE_FILE; then
    sed -i '' 's|export VCPKG_ROOT=.*|export VCPKG_ROOT=$(pwd)/vcpkg|' $PROFILE_FILE
else
    echo 'export VCPKG_ROOT=$(pwd)/vcpkg' >> $PROFILE_FILE
fi

# Check if the PATH environment variable is already set to include VCPKG_ROOT in the profile file. 
# If it is, update it. If not, add it.

if grep -q 'export PATH=$VCPKG_ROOT' $PROFILE_FILE; then
    sed -i '' 's|export PATH=$VCPKG_ROOT.*|export PATH=$VCPKG_ROOT:$PATH|' $PROFILE_FILE
else
    echo 'export PATH=$VCPKG_ROOT:$PATH' >> $PROFILE_FILE
fi
