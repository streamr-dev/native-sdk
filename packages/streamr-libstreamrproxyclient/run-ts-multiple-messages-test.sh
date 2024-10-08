#!/bin/bash

set -e

#Install the server
cd ../streamr-trackerless-network/test/integration/ts-integration
./install.sh


#Run the server

./run-server-for-tests.sh

cd -

#Run the tests 
EXPECTED_OUTPUT="Received message: Hello from libstreamrproxyclient! m1"
EXPECTED_OUTPUT_2="Received message: Hello from libstreamrproxyclient! m2"
EXPECTED_OUTPUT_3="Received message: Hello from libstreamrproxyclient! m3"

LOG_LEVEL=trace ./build/streamr-streamrproxyclient-test-end-to-end --gtest_filter=PublishToTsServerTest.ProxyPublishMultipleMessages

# Check if the server log contains the desired outputs
if grep -q "$EXPECTED_OUTPUT" ../streamr-trackerless-network/test/integration/ts-integration/output.log && \
   grep -q "$EXPECTED_OUTPUT_2" ../streamr-trackerless-network/test/integration/ts-integration/output.log && \
   grep -q "$EXPECTED_OUTPUT_3" ../streamr-trackerless-network/test/integration/ts-integration/output.log; then
    echo "Server received the expected messages: $EXPECTED_OUTPUT, $EXPECTED_OUTPUT_2, $EXPECTED_OUTPUT_3"
else
    echo "Error: Server did not output the expected message."
    kill $(cat ../streamr-trackerless-network/test/integration/ts-integration/server.pid)
    exit 1
fi

#Kill the server

kill $(cat ../streamr-trackerless-network/test/integration/ts-integration/server.pid)
exit 0