#!/bin/bash

set -e

SRCFILES=$(find src -type f \( -name "*.hpp" -o -name "*.cpp" \) -not -path '*/proto/*' | sort | uniq | tr '\n' ' ')

if [ -n "$SRCFILES" ]; then
    echo "Running clangd-tidy on $SRCFILES"
    ../../clangd-tidy/clangd-tidy -p ./build $SRCFILES

    echo "Running clang-format --dry-run on $SRCFILES"
    ../../run-clang-format.py $SRCFILES
fi

TESTFILES=$(find test -type f \( -name "*.hpp" -o -name "*.cpp" \) -not -path '*/ts-integration/*' | sort | uniq | tr '\n' ' ')
echo "Running clangd-tidy on $TESTFILES"

../../clangd-tidy/clangd-tidy -p ./build $TESTFILES

echo "Running clang-format --dry-run on $TESTFILES"
../../run-clang-format.py $TESTFILES

INCLUDEFILES=$(find include -type f \( -name "*.hpp" -o -name "*.cpp" \) -not -path '*/proto/*' | sort | uniq | tr '\n' ' ')
echo "Running clangd-tidy on $INCLUDEFILES"

../../clangd-tidy/clangd-tidy -p ./build $INCLUDEFILES

echo "Running clang-format --dry-run on $INCLUDEFILES"
../../run-clang-format.py $INCLUDEFILES
