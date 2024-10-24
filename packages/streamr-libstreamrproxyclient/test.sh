#!/bin/bash

cd build
cmake ..
make
export LOG_LEVEL=trace
export GTEST_COLOR=1
export GTEST_BREAK_ON_FAILURE=1

if [ "$#" -gt 0 ]; then
    ctest -V -R "$@"
else
    ctest -V
fi

CTEST_RETURN_CODE=$?

cd ..

exit $CTEST_RETURN_CODE
