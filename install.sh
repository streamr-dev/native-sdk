#!/bin/bash

git config core.hooksPath .githooks
./merge-dependencies.sh

# Call build for all monorepo packages in their own build directories
for package in $(cat MonorepoPackages.cmake | grep -v "set(MonorepoPackages" | grep -v ")"); do
    cd packages/$package/build && cmake .. && make && cd ../../..
done

# Call build for the root project
echo "Building root project"
cd build && cmake .. && make && cd ..
