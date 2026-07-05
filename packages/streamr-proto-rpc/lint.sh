#!/bin/bash

set -e

FILES=$(find . -type d \( -name src -o -name include -o -name test \) ! -path '*/build/*' ! -path '*/proto/*' -print0 | xargs -0 -I{} find {} -type f \( -name "*.hpp" -o -name "*.cpp" \) ! -name "PluginCodeGenerator.hpp" -print0 | xargs -0 echo)
echo "Running clangd-tidy on $FILES"

# test/unit/RpcCommunicatorTest.cpp is excluded from clangd-tidy
# (owner-approved selective disabling, Phase 2.6 consolidation): on the
# Linux CI runners clangd fails to unify std::string between the file's
# preamble and the module BMIs it imports (the known "preamble/BMI
# std-type unification" failure class from the consolidation decision
# memo — spurious "cannot initialize object parameter of type 'const
# std::basic_string<char>' with an expression of type 'std::string'" on
# every gtest assertion). The macOS clangd of the same major version
# accepts the file, and the COMPILER accepts it on every platform;
# clang-format still checks it. Revisit on each clangd release.
TIDY_FILES=$(echo "$FILES" | tr ' ' '\n' | grep -v 'test/unit/RpcCommunicatorTest.cpp' | tr '\n' ' ')
clangd-tidy -p ./build $TIDY_FILES

echo "Running clang-format --dry-run on $FILES"
../../run-clang-format.py $FILES

# CONSOLIDATED (MODERNIZATION.md Phase 2.6): the include/ tree is gone —
# the code lives in the module interface units, which are linted with
# clangd-tidy as the source of truth. (The generated RPC stub module
# units live outside modules/ — under test/proto and examples/*/proto —
# and are excluded from linting like all generated code.)
MODULE_FILES=$(find ./modules -type f -name "*.cppm" 2>/dev/null | xargs echo)
if [ -n "$MODULE_FILES" ]; then
    echo "Running clangd-tidy on $MODULE_FILES"
    clangd-tidy -p ./build $MODULE_FILES

    echo "Running clang-format --dry-run on $MODULE_FILES"
    ../../run-clang-format.py $MODULE_FILES
fi
