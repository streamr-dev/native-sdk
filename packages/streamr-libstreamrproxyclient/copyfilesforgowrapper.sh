#!/bin/bash
# Print out whole command line
echo "Copying files for go wrapper: $0 $@"

# Check if all required arguments are provided
if [ "$#" -ne 3 ]; then
    echo "Usage: $0 SOURCE_DIR VCPKG_TARGET_TRIPLET CURRENT_LIST_DIR"
    echo "  SOURCE_DIR: Directory containing the .so or .dylib file"
    echo "  VCPKG_TARGET_TRIPLET: Target triplet (e.g. x64-linux)"
    echo "  CURRENT_LIST_DIR: Current list directory"
    exit 1
fi


# Take the source directory containing the .so or .dylib file as an argument
SOURCE_DIR=$1
# Take the target triplet as an argument
VCPKG_TARGET_TRIPLET=$2
# Take the current list directory as an argument
CURRENT_LIST_DIR=$3

#find out if the file is a .so or .dylib file by checking the file extension of the files in the source directory
FILE_TYPE=$(ls $SOURCE_DIR | grep -o '\.so\|\.dylib' | head -n 1)

# create the destination directory if it doesn't exist
mkdir -p $CURRENT_LIST_DIR/wrappers/go/dist/$VCPKG_TARGET_TRIPLET/lib/Release/

SOURCE_FILE=$SOURCE_DIR/libstreamrproxyclient$FILE_TYPE
DEST_FILE=$CURRENT_LIST_DIR/wrappers/go/dist/$VCPKG_TARGET_TRIPLET/lib/Release/libstreamrproxyclient$FILE_TYPE
# Copy the .so or .dylib file to the dist directory
cp -L $SOURCE_FILE $DEST_FILE 2>/dev/null || :

cd $CURRENT_LIST_DIR/wrappers/go/

# Install git-filter-repo
#pip install  git-filter-repo

# Remove git history of the DEST_FILE
#git filter-repo --strip-blobs-bigger-than 100M --refs $DEST_FILE

# Add the DEST_FILE to the submodule git repository
echo "Adding library file to git..."
git add $DEST_FILE 2>/dev/null || :

echo "Committing library file changes..."
git commit -m "Add latest version of $DEST_FILE" 2>/dev/null || :

echo "Force pushing library changes to submodule repository..."
git push origin --force 2>/dev/null || :

cd -

echo "Updating main repository with new submodule version..."
git add $CURRENT_LIST_DIR/wrappers/go 2>/dev/null || :
git commit -m "Update gowrapper submodule to latest version" 2>/dev/null || :
git push --no-verify origin master 2>/dev/null || :

