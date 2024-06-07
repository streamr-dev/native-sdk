#!/bin/bash

cmake --build build
cd build

if [ "$#" -gt 0 ]; then
    ctest -V -R "$@"
else
    ctest -V
fi

cd ..

