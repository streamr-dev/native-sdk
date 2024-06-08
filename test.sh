#!/bin/bash

cd build
cmake ..
make

export GTEST_COLOR=1

if [ "$#" -gt 0 ]; then
    ctest -V -R "$@"
else
    ctest -V
fi

CTEST_RETURN_CODE=$?

cd ..

return $CTEST_RETURN_CODE


