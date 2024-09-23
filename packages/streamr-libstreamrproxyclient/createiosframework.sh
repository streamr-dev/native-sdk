#!/bin/bash

mkdir -p dist/ios/dynamic
cp -rf templates/streamrproxyclient.framework dist/ios/dynamic/streamrproxyclient.framework
cp include/streamrproxyclient.h dist/ios/dynamic/streamrproxyclient.framework/Headers/streamrproxyclient.h
cp build/streamrproxyclient.framework/streamrproxyclient dist/ios/dynamic/streamrproxyclient.framework/streamrproxyclient_in
cd dist/ios/dynamic/streamrproxyclient.framework

lipo -create streamrproxyclient_in -output streamrproxyclient
install_name_tool -id @rpath/streamrproxyclient.framework/streamrproxyclient streamrproxyclient
rm -f streamrproxyclient_in

cd ../../..
