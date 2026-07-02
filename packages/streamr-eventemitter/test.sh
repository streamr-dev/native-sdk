#!/bin/bash

export GTEST_COLOR=1

cmake --build build || exit 1

cd build

if [ "$#" -gt 0 ]; then
    ctest -V -R "$@"
else
    ctest -V
fi

CTEST_RETURN_CODE=$?

cd ..

exit $CTEST_RETURN_CODE
