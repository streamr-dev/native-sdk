#!/bin/bash

export GTEST_COLOR=1
cmake --build build
cd build

if [ "$#" -gt 0 ]; then
    ctest -V -R "$@"
else
    ctest -V
fi

cd ..

