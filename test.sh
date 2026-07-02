#!/bin/bash

export GTEST_COLOR=1

cmake --build build || exit 1

cd build

# --repeat until-pass:2 retries a failed test once, and --timeout 300
# converts a hung test into a failure (which then gets the retry): the
# networking integration tests (e.g. ConnectionLockingTest) are
# timing-sensitive on shared CI runners and have both flaked and hung
# there, on the old toolchain as well as the new one. A genuinely broken
# test still fails both attempts. The whole suite normally runs in ~35 s;
# 300 s per test is generous.
if [ "$#" -gt 0 ]; then
    ctest -V --repeat until-pass:2 --timeout 300 -R "$@"
else
    ctest -V --repeat until-pass:2 --timeout 300
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
