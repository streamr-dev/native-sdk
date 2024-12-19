#!/bin/bash

./switch_build.sh test
cd StreamrProxyClient
./gradlew connectedDebugAndroidTest --info \
    -Pandroid.testInstrumentationRunnerArguments.class=network.streamr.proxyclient.StreamrProxyClientTest \
    --rerun-tasks
cd ..
./switch_build.sh 
