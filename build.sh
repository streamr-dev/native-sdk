#!/bin/bash

# Parse MonorepoPackages.cmake and loop through them
for package in $(cat MonorepoPackages.cmake | grep -v "set(MonorepoPackages" | grep -v ")"); do
    cd packages/$package/build && cmake .. && make && cd ../../..
done

cd build && cmake .. && make && cd ..
