#!/bin/bash
# Print out whole command line
echo "Copying files for go wrapper: $0 $@"

# Take the source directory containing the .so or .dylib file as an argument
SOURCE_DIR=$1
# Take the target triplet as an argument
VCPKG_TARGET_TRIPLET=$2
# Take the current list directory as an argument
CURRENT_LIST_DIR=$3

#find out if the file is a .so or .dylib file by checking the file extension of the files in the source directory
FILE_TYPE=$(ls $SOURCE_DIR | grep -o '\.so\|\.dylib')

# create the destination directory if it doesn't exist
mkdir -p $CURRENT_LIST_DIR/wrappers/go/dist/$VCPKG_TARGET_TRIPLET/lib/Release/

# Copy the .so or .dylib file to the dist directory
cp -L $SOURCE_DIR/libstreamrproxyclient$FILE_TYPE $CURRENT_LIST_DIR/wrappers/go/dist/$VCPKG_TARGET_TRIPLET/lib/Release/ 2>/dev/null || :
