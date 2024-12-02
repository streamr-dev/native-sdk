#! /bin/bash

BASE_DIR=$(pwd)

if [[ "$OSTYPE" == "darwin"* ]] && [[ "$(uname -m)" == "arm64" ]]; then
    LIB_PATH=$(realpath $BASE_DIR/../../dist/arm64-osx/lib/Debug)
    export DYLD_LIBRARY_PATH=$LIB_PATH:$DYLD_LIBRARY_PATH
    export CGO_LDFLAGS="-L$LIB_PATH"
fi
if [[ "$OSTYPE" == "darwin"* ]] && [[ "$(uname -m)" == "x86_64" ]]; then
    LIB_PATH=$(realpath $BASE_DIR/../../dist/x64-osx/lib/Debug)
    export DYLD_LIBRARY_PATH=$LIB_PATH:$DYLD_LIBRARY_PATH
    export CGO_LDFLAGS="-L$LIB_PATH"
fi
if [[ "$OSTYPE" == "linux-gnu"* ]] && [[ "$(uname -m)" == "x86_64" ]]; then
    LIB_PATH=$(realpath $BASE_DIR/../../dist/x64-linux/lib/Debug)
    export LD_LIBRARY_PATH=$LIB_PATH:$LD_LIBRARY_PATH
    export CGO_LDFLAGS="-L$LIB_PATH"
fi
if [[ "$OSTYPE" == "linux-gnu"* ]] && [[ "$(uname -m)" == "aarch64" ]]; then
    LIB_PATH=$(realpath $BASE_DIR/../../dist/arm64-linux/lib/Debug)
    export LD_LIBRARY_PATH=$LIB_PATH:$LD_LIBRARY_PATH
    export CGO_LDFLAGS="-L$LIB_PATH"
fi

go test -test.run TestPublishToServer