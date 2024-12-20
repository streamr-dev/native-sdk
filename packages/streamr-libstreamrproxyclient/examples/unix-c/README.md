# Libstreamrproxyclient C example app for Unix-like operating systems

This is a simple example app that demonstrates how to use the libstreamrproxyclient shared library directly in the C language to publish messages to the Streamr network on Linux and macOS. The example app broadcasts a "Hello from libstreamrproxyclient!" message to a Streamr stream that can be viewed at https://streamr.network/hub/streams/0xd2078dc2d780029473a39ce873fc182587be69db%2Flow-level-client/live-data 

# Prerequisites

- A recent version of CMake
- A recent C compiler (clang on macOS, gcc on Linux)

# Building

- Compile the native-sdk project to generate the libstreamrproxyclient shared library, or get the pre-compiled library from a Streamr Native SDK distribution bundle. By default, the CMakeLists.txt script will look for the library at '${CMAKE_SOURCE_DIR}/../../dist/${ARCH_DIR}/lib/${CMAKE_BUILD_TYPE})'

```bash
./install.sh
```

# Running

- To publish a message to the Streamr network, run the example app:

```bash
./run.sh
```bash

- You should be able to see the publised message in the Streamr Hub Web UI at https://streamr.network/hub/streams/0xd2078dc2d780029473a39ce873fc182587be69db%2Flow-level-client/live-data

