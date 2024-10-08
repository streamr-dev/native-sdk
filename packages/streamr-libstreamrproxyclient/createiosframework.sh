#!/bin/bash

mkdir -p dist/arm64-ios
cp -rf templates/streamrproxyclient.framework dist/arm64-ios/streamrproxyclient.framework
cp include/streamrproxyclient.h dist/arm64-ios/streamrproxyclient.framework/Headers/streamrproxyclient.h
cp build/streamrproxyclient.framework/streamrproxyclient dist/arm64-ios/streamrproxyclient.framework/streamrproxyclient_in
cd dist/arm64-ios/streamrproxyclient.framework

lipo -create streamrproxyclient_in -output streamrproxyclient
install_name_tool -id @rpath/streamrproxyclient.framework/streamrproxyclient streamrproxyclient
rm -f streamrproxyclient_in

cd ../../..
