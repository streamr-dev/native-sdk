# Libstreamrproxyclient example app for Unix-like operating systems

This is a simple example app that demonstrates how to use the libstreamrproxyclient library to publish messages to the Streamr network on Linux and macOS. The exaple app broadcasts a "Hello World" message to the local proxy server every 15 seconds. ***As the local proxy server uses a private entrypoint running in the cloud to join a private Streamr Network, you will not be able to see the messages in the Streamr production Network, but you can see the messages on other computers that have joined the same private network.***

# Prerequisites

- A recent version of CMake
- A recent C++ compiler (clang++ on macOS, g++ on Linux)

***Note: you do not need to build the Streamr Native SDK to build this example app. The pre-compiled libstreamrproxyclient library is included in the Streamr Native SDK distribution bundle.***

# Building

```bash
./install.sh
```

The CMake build system will link the example app against the correct pre-compiled libstreamrproxyclient library for your target platform.

# Running

Open the directory `native-sdk/packages/streamr-trackerless-network/test/integration/ts-integration/` in another terminal window, install and run the streamr proxy server: 

```bash
./install.sh
./run.sh
```bash

Run the example app and see the messages in the proxy server console.

```bash
./run.sh
```

You can also run the proxy server on another computer at the same time to see the Streamr Network propagating the messages.
