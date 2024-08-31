#!/bin/bash

rm -rf TestResults.xcresult

# Run tests
xcodebuild test -scheme iOSUnitTesting -destination 'platform=macOS' -resultBundlePath TestResults.xcresult

# Check if xcodebuild was successful
if [ $? -ne 0 ]; then
    echo "xcodebuild failed to run tests"
    exit 2
fi

# Process results
RESULT=$(xcrun xcresulttool get --format json --path TestResults.xcresult)

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

exit $EXIT_CODE
