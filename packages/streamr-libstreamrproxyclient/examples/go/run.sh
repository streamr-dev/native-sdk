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

set -e

PROXY_URL="ws://95.216.15.80:44211"
PROXY_ETHEREUM_ADDRESS="0xd0d14b38d1f6b59d3772a63d84ece0a79e6e1c1f"

STREAM_PART_ID="0xd2078dc2d780029473a39ce873fc182587be69db/low-level-client#0"

echo "Running the publisher example with parameters: $PROXY_URL $PROXY_ETHEREUM_ADDRESS $STREAM_PART_ID"
# Run the publisher example with the proxy address

go run . $PROXY_URL $PROXY_ETHEREUM_ADDRESS $STREAM_PART_ID
