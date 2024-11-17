#!/bin/bash

# Check if version parameter is provided
if [ -z "$1" ]; then
    echo "Error: Version number is required"
    echo "Usage: ./create-android-library-module.sh <version>"
    exit 1
fi

VERSION=$1
cd dist/android-library-module
tar -czh -f "streamrproxyclient-arm64-android-${VERSION}.tgz" StreamrProxyClient
cd ../..
