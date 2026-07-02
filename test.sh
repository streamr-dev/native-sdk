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

# On failure, surface the failed test names as a GitHub Actions error
# annotation so they are visible in the Checks UI without opening raw logs.
if [ $CTEST_RETURN_CODE -ne 0 ] && [ -f Testing/Temporary/LastTestsFailed.log ]; then
    MSG=$(sed 's/%/%25/g' Testing/Temporary/LastTestsFailed.log | awk '{printf "%s%%0A", $0}')
    echo "::error title=failed tests::${MSG}"
fi

cd ..

exit $CTEST_RETURN_CODE
