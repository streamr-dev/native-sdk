#! /bin/bash

# Print out whole command line
echo "Copying files for python wrapper: $0 $@"

# Check if all required arguments are provided
if [ "$#" -ne 4 ]; then
    echo "Usage: $0 SOURCE_DIR VCPKG_TARGET_TRIPLET CURRENT_LIST_DIR"
    echo "  SOURCE_DIR: Directory containing the .so or .dylib file"
    echo "  VCPKG_TARGET_TRIPLET: Target triplet (e.g. x64-linux)"
    echo "  CURRENT_LIST_DIR: Current list directory"
    echo "  STREAMRPROXYCLIENT_VERSION: StreamrProxyClient version"
    exit 1
fi


# Take the source directory containing the .so or .dylib file as an argument
SOURCE_DIR=$1
# Take the target triplet as an argument
VCPKG_TARGET_TRIPLET=$2
# Take the current list directory as an argument
CURRENT_LIST_DIR=$3
# Take the StreamrProxyClient version as an argument
STREAMRPROXYCLIENT_VERSION=$4

#find out if the file is a .so or .dylib file by checking the file extension of the files in the source directory
FILE_TYPE=$(ls $SOURCE_DIR | grep -o '\.so\|\.dylib' | head -n 1)

SOURCE_FILE=$SOURCE_DIR/libstreamrproxyclient$FILE_TYPE
DEST_FILE=$CURRENT_LIST_DIR/wrappers/python/src/streamrproxyclient/libstreamrproxyclient$FILE_TYPE
echo "Copying $SOURCE_FILE to $DEST_FILE"
# Copy the .so or .dylib file to the dist directory
cp -L $SOURCE_FILE $DEST_FILE  2>/dev/null || :

# Install needed python packages

python -m pip install pip
python -m pip install hatch hatchling pip twine build

# Copy the dylib to the package

cd $CURRENT_LIST_DIR/wrappers/python/

# Replace the version in the pyproject.toml file
pip install toml

python -c "
import toml
import sys

config = toml.load('pyproject.toml')
config['project']['version'] = '$STREAMRPROXYCLIENT_VERSION'
with open('pyproject.toml', 'w') as f:
    toml.dump(config, f)
"
rm -rf dist
python -m build

# Upload the package to test.pypi.org

python -m twine upload --verbose dist/* || :

#remove the dylib from the package

rm -f $DEST_FILE
