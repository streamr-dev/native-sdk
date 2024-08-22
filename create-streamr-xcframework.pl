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
dircopy("$abs_path/packages/streamr-logger/include", $build_include);
dircopy("$abs_path/packages/streamr-logger/include", $build_include);

# Find all include and lib directories and process them 
find(\&process_dir, "./vcpkg/packages");

`libtool -static -o $build_lib/streamr.a $build_lib/*.a`;
`xcodebuild -create-xcframework -library $build_lib/streamr.a -headers $build_include -output $dist_path/streamr.xcframework`; 
print "\nstreamr.xcframework was created in the directory: dist/ios.\n";

# Subroutine to process each directory
sub process_dir {
    my $dir = $File::Find::name;
    if ($dir =~ m|^\./vcpkg/packages/[^/]+/include$|) {
       # Copy includes from vcpkg-packages (vcpkg/packages/<Package>/include)
       dircopy("$abs_path/$dir", $build_include);
    } elsif ($dir =~ m|^\./vcpkg/packages/[^/]+/lib$|) {
       # Copy libs from vcpkg-packages (vcpkg/packages/<Package>/lib) 
       dircopy("$abs_path/$dir", $build_lib);
    }
}

