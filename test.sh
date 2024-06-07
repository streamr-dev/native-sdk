#!/bin/bash

cd build
cmake ..
make

if [ "$#" -gt 0 ]; then
    ctest -V -R "$@"
else
    ctest -V
fi

cd ..

