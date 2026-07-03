#!/bin/bash

# Usage: ./iostest.sh                  run the tests on this Mac (default)
#        ./iostest.sh --device         run on a connected iOS device
#        ./iostest.sh --device "name"  run on the named iOS device
#
# Signing: the project is set up for the organization team. To sign with a
# different team (e.g. your Personal Team while organization provisioning
# is unavailable), set IOS_DEVELOPMENT_TEAM to that team's ID:
#     IOS_DEVELOPMENT_TEAM=ABCDE12345 ./iostest.sh --device
# Command-line build settings override the project, so nothing is modified.
DESTINATION='platform=macOS'
if [ "$1" = "--device" ]; then
    if [ -n "$2" ]; then
        DESTINATION="platform=iOS,name=$2"
    else
        DESTINATION='platform=iOS'
    fi
fi

SIGNING_OVERRIDES=()
if [ -n "$IOS_DEVELOPMENT_TEAM" ]; then
    SIGNING_OVERRIDES=(
        "DEVELOPMENT_TEAM=$IOS_DEVELOPMENT_TEAM"
        "CODE_SIGN_STYLE=Automatic"
        "-allowProvisioningDeviceRegistration"
    )
fi

rm -rf build/ios

brew install chargepoint/xcparse/xcparse

# Run tests
xcodebuild test -project test/ios/iOSUnitTesting/iOSUnitTesting.xcodeproj -scheme iOSUnitTesting -destination "$DESTINATION" -configuration Debug -resultBundlePath build/ios/TestResults.xcresult -allowProvisioningUpdates "${SIGNING_OVERRIDES[@]}"

# Check if xcodebuild was successful
if [ $? -ne 0 ]; then
    echo "xcodebuild failed to run tests"
    exit 2
fi

# Process results
RESULT=$(xcrun xcresulttool get --format json --path build/ios/TestResults.xcresult)

# Check if xcresulttool was successful
if [ $? -ne 0 ]; then
    echo "Failed to process test results"
    exit 3
fi

# Extract test status
TEST_STATUS=$(echo $RESULT | jq -r '.issues.testFailureSummaries | length')

if [ "$TEST_STATUS" -eq "0" ]; then
    echo "All tests passed"
    EXIT_CODE=0
else
    echo "Some tests failed. Details:"
    echo $RESULT | jq '.issues.testFailureSummaries'
    EXIT_CODE=1
fi

xcparse logs build/ios/TestResults.xcresult build/ios/TestResults
find build/ios -name StandardOutputAndStandardError.txt -exec cat {} +
exit $EXIT_CODE
