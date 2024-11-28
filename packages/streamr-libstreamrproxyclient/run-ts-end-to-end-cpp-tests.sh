#!/bin/bash

set -e

#Install the server
cd ../streamr-trackerless-network/test/integration/ts-integration
./install.sh


#Run the server

./run-server-for-tests.sh

cd -

#Run the tests 
EXPECTED_OUTPUT="Received message: Hello from libstreamrproxyclient!"
LOG_LEVEL=trace ./build/streamr-streamrproxyclient-test-end-to-end --gtest_filter=PublishToTsServerCppTest.ProxyPublish

# Check if the server log contains the desired output
if grep -q "$EXPECTED_OUTPUT" ../streamr-trackerless-network/test/integration/ts-integration/output.log; then
    echo "Server received the expected message: $EXPECTED_OUTPUT"
else
    echo "Error: Server did not output the expected message."
    kill $(cat ../streamr-trackerless-network/test/integration/ts-integration/server.pid)
    exit 1
fi

#Kill the server

kill $(cat ../streamr-trackerless-network/test/integration/ts-integration/server.pid)
exit 0
