#!/bin/bash

# Check if version parameter is provided
if [ -z "$1" ]; then
    echo "Error: Version number is required"
    echo "Usage: ./createiospackage.sh <version>"
    exit 1
fi

VERSION=$1
cd dist/ios
tar -czh -f "../streamrproxyclient-arm64-ios-${VERSION}.tgz" StreamrProxyClient
cd ../..
