#!/bin/bash

set -e

FILES=$(find . -type d \( -name src -o -name include -o -name test \) ! -path '*/build/*' ! -path '*/android/*' ! -path '*/android-library-module/*' ! -path '*/ios/*' ! -path '*/proto/*' -print0 | xargs -0 -I{} find {} -type f \( -name "*.hpp" -o -name "*.cpp" \) -print0 | xargs -0 echo)
echo "Running clangd-tidy on $FILES"

# -j 1: this package's implementation imports many module interfaces,
# and analysing several such files concurrently exhausted memory on the
# 2-core CI runners (clangd dies mid-stream: "Invalid header end").
# The package has only a handful of files, so serial analysis costs
# seconds.
clangd-tidy -j 1 -p ./build $FILES

echo "Running clang-format --dry-run on $FILES"
../../run-clang-format.py $FILES
