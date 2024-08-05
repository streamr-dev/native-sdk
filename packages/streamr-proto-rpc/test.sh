#!/bin/bash

cmake --build build
cd build
export LOG_LEVEL=trace
if [ "$#" -gt 0 ]; then
    ctest -V -R "$@"
else
    ctest -V
fi

cd ..

