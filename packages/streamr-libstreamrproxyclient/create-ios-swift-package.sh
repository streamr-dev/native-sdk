#!/bin/bash

# Check if version parameter is provided
if [ -z "$1" ]; then
    echo "Error: Version number is required"
    echo "Usage: ./create-ios-swift-package.sh <version>"
    exit 1
fi

VERSION=$1
cd dist/ios-swift-package
tar -czh -f "streamrproxyclient-arm64-ios-${VERSION}.tgz" StreamrProxyClient
cd ../..
