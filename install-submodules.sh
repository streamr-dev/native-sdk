#!/bin/bash

# This script needs to be run using "source install-vcpkg.sh"
# Calling it directly will not work

git submodule add https://github.com/microsoft/vcpkg.git
git submodule add https://github.com/lljbash/clangd-tidy.git
git submodule update --init --recursive

cd clangd-tidy
rm /opt/homebrew/bin/clang-tidy
ln -s clangd-tidy /opt/homebrew/bin/clang-tidy

cd vcpkg
./bootstrap-vcpkg.sh -disableMetrics
cd ..

export VCPKG_ROOT=$(pwd)/vcpkg
export PATH=$VCPKG_ROOT:$PATH

# Check the operating system type. If it's a Mac, use the .zprofile file. 
# Otherwise, use the .profile file.
if [[ "$OSTYPE" == "darwin"* ]]; then
    PROFILE_FILE=~/.zprofile
else
    PROFILE_FILE=~/.profile
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

source $PROFILE_FILE
