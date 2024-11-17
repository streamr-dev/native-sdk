#!/bin/bash

if [ "$1" == "test" ]; then
    cp StreamrProxyClient/build.test.gradle.kts StreamrProxyClient/build.gradle.kts
    echo "Switched to test build file"
else
    git checkout StreamrProxyClient/build.gradle.kts
    echo "Switched to main build file"
fi

