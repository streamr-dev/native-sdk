#!/bin/bash

./switch_build.sh test
cd StreamrProxyClient
./gradlew connectedAndroidTest
cd ..
./switch_build.sh 
