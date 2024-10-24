#! /bin/bash

set -e

# Execute tests for all monorepo packages in their own build directories
for package in $(cat MonorepoPackages.cmake | grep -v "set(MonorepoPackages" | grep -v ")"); do
    cd build/packages/$package
    # for all test executables in the format <package-name>-test-*
    for test in $(ls | grep -E "^$package-test-"); do
        adb push $test /data/local/tmp/$test
        R=$(adb shell "cd /data/local/tmp && chmod +x $test && ./$test --gtest_color=yes --gtest_break_on_failure 1>&2 && printf SUCCESS || printf FAIL")
        if [ "$R" != "SUCCESS" ]; then
            echo "Test $test failed"
            exit 1
        fi
    done
    echo "All tests passed successfully"
    cd ../../..
done
