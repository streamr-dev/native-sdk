#!/usr/bin/perl

use strict;
use warnings;
use File::Find;
use File::Path qw(make_path);
use File::Copy::Recursive qw(dircopy);
use Cwd;

my $abs_path = getcwd;
my $root_build = "build/ios";
my $build_include = "$abs_path/$root_build/include";
my $build_lib = "$abs_path/$root_build/lib";
my $dist_path = "$abs_path/dist/ios";

print "Please wait, building xcframework.\n";

`rm -rf $build_include`;
`rm -rf $build_lib`;
`rm -rf $dist_path`;

make_path($build_include);
make_path($build_lib);

# Copy streamr headers
dircopy("$abs_path/packages/streamr-proto-rpc/include", $build_include);
dircopy("$abs_path/packages/streamr-logger/include", $build_include);
dircopy("$abs_path/packages/streamr-json/include", $build_include);
dircopy("$abs_path/packages/streamr-dht/include", $build_include);
dircopy("$abs_path/packages/streamr-eventemitter/include", $build_include);
dircopy("$abs_path/packages/streamr-trackerless-network/include", $build_include);
dircopy("$abs_path/packages/streamr-utils/include", $build_include);
dircopy("$abs_path/packages/streamr-proto-rpc/src/proto", $build_include);
dircopy("$abs_path/packages/streamr-dht/src/proto", $build_include);
dircopy("$abs_path/packages/streamr-trackerless-network/src/proto", $build_include);
`cp packages/streamr-libstreamrproxyclient/include/streamrproxyclient.h $build_include`;
`cp packages/streamr-libstreamrproxyclient/include/StreamrProxyClient.hpp $build_include`;
`cp packages/streamr-libstreamrproxyclient/src/LibProxyClientApi.hpp $build_include`;
 
# Find all include and lib directories and process them 
find(\&process_dir, "./build/vcpkg_installed/arm64-ios");
dircopy("$abs_path/build/vcpkg_installed/arm64-ios/lib", $build_lib);
`libtool -static -o $build_lib/libstreamr.a $build_lib/*.a packages/streamr-trackerless-network/build/CMakeFiles/streamr-trackerless-network.dir/src/proto/packages/network/protos/NetworkRpc.pb.cc.o packages/streamr-dht/build/CMakeFiles/streamr-dht.dir/src/proto/packages/dht/protos/DhtRpc.pb.cc.o packages/streamr-libstreamrproxyclient/build/CMakeFiles/streamrproxyclient.dir/src/streamrproxyclient.cpp.o packages/streamr-proto-rpc/build/CMakeFiles/streamr-proto-rpc.dir/src/proto/packages/proto-rpc/protos/ProtoRpc.pb.cc.o`;
`xcodebuild -create-xcframework -library $build_lib/libstreamr.a -headers $build_include -output $dist_path/streamr.xcframework`; 
print "\nstreamr.xcframework was created in the directory: dist/ios.\n";

# Subroutine to process each directory
sub process_dir {
    my $dir = $File::Find::name;
    print "Current dir: $dir\n";
    if ($dir =~ m|^\./build/vcpkg_installed/arm64-ios/include$|) {
       # Copy includes from vcpkg-packages (vcpkg/packages/<Package>/include)
       dircopy("$abs_path/$dir", $build_include);
       print "cp $abs_path/$dir $build_include\n";
    } 
}

