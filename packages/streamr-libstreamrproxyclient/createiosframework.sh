#!/bin/bash

mkdir -p dist/ios/dynamic
cp -rf templates/streamrproxyclient.framework dist/ios/dynamic/streamrproxyclient.framework
cp include/libstreamrproxyclient.hpp dist/ios/dynamic/streamrproxyclient.framework/Headers/streamrproxyclient.hpp
cp build/libstreamrproxyclient.framework/libstreamrproxyclient dist/ios/dynamic/streamrproxyclient.framework/libstreamrproxyclient
cd dist/ios/dynamic/streamrproxyclient.framework

lipo -create libstreamrproxyclient -output streamrproxyclient
install_name_tool -id @rpath/streamrproxyclient.framework/streamrproxyclient streamrproxyclient
rm -f libstreamrproxyclient

cd ../../..
