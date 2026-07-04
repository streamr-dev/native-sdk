#!/bin/bash

set -e

FILES=$(find . -type d \( -name src -o -name include -o -name test \) ! -path '*/build/*' ! -path '*/android/*' ! -path '*/android-library-module/*' ! -path '*/ios/*' ! -path '*/proto/*' -print0 | xargs -0 -I{} find {} -type f \( -name "*.hpp" -o -name "*.cpp" \) -print0 | xargs -0 echo)
echo "Running clangd-tidy on $FILES"

# src/streamrproxyclient.cpp is excluded from clangd-tidy
# (owner-approved selective disabling, Phase 2.6 consolidation): the
# file imports module interfaces from four packages, and building that
# module graph inside a single clangd exhausts memory on the 2-core
# 7 GB CI runners even with -j 1 (clangd dies mid-protocol,
# "Invalid header end"). The compiler builds the file on every
# platform and clang-format still checks it; revisit when CI runners
# grow or clangd's module memory use shrinks.
TIDY_FILES=$(echo "$FILES" | tr ' ' '\n' | grep -v 'src/streamrproxyclient.cpp' | tr '\n' ' ')
clangd-tidy -j 1 -p ./build $TIDY_FILES

echo "Running clang-format --dry-run on $FILES"
../../run-clang-format.py $FILES
