#!/bin/bash

rm -rf ~/.cache/vcpkg

# Parse MonorepoPackages.cmake and loop through them
for package in $(cat MonorepoPackages.cmake | grep -v "set(MonorepoPackages" | grep -v ")"); do
    cd packages/$package/build && ls -A | while read FILE; do echo "\"$FILE\""; done | grep -v ".gitignore" | xargs rm -rf && cd ../../..
done

cd build && ls -A | while read FILE; do echo "\"$FILE\""; done | grep -v ".gitignore" | xargs rm -rf && cd ..
