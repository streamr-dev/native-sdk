#!/bin/bash

./merge-dependencies.sh

# Call test for all monorepo packages in their own build directories
for package in $(cat MonorepoPackages.cmake | grep -v "set(MonorepoPackages" | grep -v ")"); do
    cd packages/$package && ./test.sh && cd ../..
done
