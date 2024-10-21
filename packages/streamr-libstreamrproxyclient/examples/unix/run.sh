#!/bin/bash

set -e

PROXY_URL="ws://95.216.15.80:44211"
PROXY_ETHEREUM_ADDRESS="0xd0d14b38d1f6b59d3772a63d84ece0a79e6e1c1f"

STREAM_PART_ID="0xd2078dc2d780029473a39ce873fc182587be69db/low-level-client#0"

echo "Running the publisher example: ./build/publisherexample $PROXY_URL $PROXY_ETHEREUM_ADDRESS $STREAM_PART_ID"
# Run the publisher example with the proxy address
./build/publisherexample $PROXY_URL $PROXY_ETHEREUM_ADDRESS $STREAM_PART_ID
