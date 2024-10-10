#!/bin/bash

set -e


PROXY_URL="ws://127.0.0.1:44211"
# Read proxy server ethereum address from file
PROXY_ETHEREUM_ADDRESS=$(cat ../../../streamr-trackerless-network/test/integration/ts-integration/proxyEthereumAddress.txt)
STREAM_PART_ID="0xd7278f1e4a946fa7838b5d1e0fe50c5725fb23de/nativesdktest#01"

echo "Running the publisher example: ./build/publisherexample $PROXY_URL $PROXY_ETHEREUM_ADDRESS $STREAM_PART_ID"
# Run the publisher example with the proxy address
./build/publisherexample $PROXY_URL $PROXY_ETHEREUM_ADDRESS $STREAM_PART_ID
