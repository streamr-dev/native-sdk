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

my $filename = "$build_include/module.modulemap";
open(my $fh, '>', $filename) or die "Could not open file '$filename' $!";

print $fh <<'END_MODULE';
module streamr {
    header "streamrproxyclient.h"    // Replace with your main header file name
    export *
}
END_MODULE

close $fh;
 
# Find all include and lib directories and process them
find(\&process_dir, "./build/vcpkg_installed/arm64-ios");
dircopy("$abs_path/build/vcpkg_installed/arm64-ios/lib", $build_lib);
# Merge the vcpkg dependency archives with the monorepo's own per-package
# static libraries. Since the C++ modules migration, each package compiles
# to a real static lib (libstreamr-<pkg>.a) that carries the module object
# files (previously these were header-only INTERFACE libs with nothing to
# archive) — they MUST be included or the XCFramework is missing all streamr
# code and nothing can link against it. The generated protobuf objects
# (NetworkRpc/DhtRpc/ProtoRpc .pb.cc.o) are already inside these package
# libs, so they are no longer listed explicitly (doing so would duplicate
# symbols). streamrproxyclient.cpp.o stays explicit: that package builds a
# shared library, not a static archive.
my @package_libs = (
    "packages/streamr-json/build/libstreamr-json.a",
    "packages/streamr-logger/build/libstreamr-logger.a",
    "packages/streamr-eventemitter/build/libstreamr-eventemitter.a",
    "packages/streamr-utils/build/libstreamr-utils.a",
    "packages/streamr-proto-rpc/build/libstreamr-proto-rpc.a",
    "packages/streamr-dht/build/libstreamr-dht.a",
    "packages/streamr-trackerless-network/build/libstreamr-trackerless-network.a",
);
my $package_libs_str = join(" ", @package_libs);
`libtool -static -o $build_lib/libstreamr.a $build_lib/*.a $package_libs_str packages/streamr-libstreamrproxyclient/build/CMakeFiles/streamrproxyclient.dir/src/streamrproxyclient.cpp.o`;
`xcodebuild -create-xcframework -library $build_lib/libstreamr.a -headers $build_include -output $dist_path/streamr.xcframework`; 
print "\nstreamr.xcframework was created in the directory: dist/ios.\n";

# Subroutine to process each directory
sub process_dir {
    my $dir = $File::Find::name;
    if ($dir =~ m|^\./build/vcpkg_installed/arm64-ios/include$|) {
       # Copy includes from vcpkg-packages (vcpkg/packages/<Package>/include)
       dircopy("$abs_path/$dir", $build_include);
       print "cp $abs_path/$dir $build_include\n";
    } 
}

