#!/bin/bash

export GTEST_COLOR=1

cmake --build build || exit 1

cd build

# --repeat until-pass:2 retries a failed test once, and --timeout converts
# a hung test into a failure (which then gets the retry): the networking
# integration tests (e.g. ConnectionLockingTest) are timing-sensitive on
# shared CI runners and have both flaked and hung there, on the old
# toolchain as well as the new one. A genuinely broken test still fails
# both attempts.
# The timeout must clear the heavy DHT convergence tests from milestone A
# (PR #73): Layer1ScaleTest.MultipleLayer1Dht runs 48 nodes x 4 layer-1
# services and takes ~214 s on a fast 10-core dev machine — on the 3-core
# macOS CI runners it exceeded the previous 300 s limit, so ctest killed
# it (and KademliaCorrectnessTest), failed the retry the same way, and
# the macOS leg went red while the faster Linux legs stayed green.
if [ "$#" -gt 0 ]; then
    ctest -V --repeat until-pass:2 --timeout 1200 -R "$@"
else
    ctest -V --repeat until-pass:2 --timeout 1200
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
