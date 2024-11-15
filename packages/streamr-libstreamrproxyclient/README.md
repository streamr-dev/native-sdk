# Libstreamrproxyclient

Libstreamrproxyclient is a portable native library for publishing messages in the Streamr network using the proxy publishing pattern. The library exposes a simple C API that can be used from from a variety of programming languages such as C, C++, Kotlin and Swift.

The library is available for the following platforms:

- Linux (x86_64, arm64)
- macOS (x86_64, arm64)
- iOS (arm64)
- Android (arm64)

# Pre-compiled binaries

You can download pre-compiled binaries of the library from the following links:
- [x64-linux] (https://github.com/streamr-dev/native-sdk/packages/streamr-libstreamrproxyclient/dist/streamrproxyclient-x64-linux-1.0.0.tgz) 
- [arm64-linux] (https://github.com/streamr-dev/native-sdk/packages/streamr-libstreamrproxyclient/dist/streamrproxyclient-arm64-linux-1.0.0.tgz)
- [x64-osx] (https://github.com/streamr-dev/native-sdk/packages/streamr-libstreamrproxyclient/dist/streamrproxyclient-x64-osx-1.0.0.tgz)
- [arm64-osx] (https://github.com/streamr-dev/native-sdk/packages/streamr-libstreamrproxyclient/dist/streamrproxyclient-arm64-osx-1.0.0.tgz)
- [arm64-ios] (https://github.com/streamr-dev/native-sdk/packages/streamr-libstreamrproxyclient/dist/streamrproxyclient-arm64-ios-1.0.0.tgz)
- [arm64-android] (https://github.com/streamr-dev/native-sdk/packages/streamr-libstreamrproxyclient/dist/streamrproxyclient-arm64-android-1.0.0.tgz)

# Installation for other platforms

Download the pre-compiled binary for your target platform, unpack the .tgz file, and add the include folder to the include search path of your project, and add the lib/Debug or lib/Release folder to the shared library search path of your project.

# Building from source

The library is built as a part of the Streamr Native SDK monorepo. To build the library, follow the instructions in the [README](https://github.com/streamr-dev/native-sdk/blob/main/README.md) of the Streamr Native SDK.

# Getting started

The best way to get is started by studying and trying out the example apps in the examples folder. The folder [examples/unix](examples/unix) contains an example app written in C++ that works on macOS and Linux, [examples/ios](examples/ios) an example app for iOS written in Swift, and [examples/android](examples/android) an example app for Android written in Kotlin.

# Limitations of the current release

* The messages are not singned nor encrypted, so they cannot be seen by network-monorepo based standard Streamr clients.

# API Documentation

### `uint64_t proxyClientNew(Error** errors, uint64_t* numErrors, const char* ownEthereumAddress, const char* streamPartId)`

Create a new proxy client.

#### Parameters:
- `errors`: The array of errors or NULL.
- `numErrors`: The number of errors.
- `ownEthereumAddress`: The Ethereum address of the client in format `0x1234567890123456789012345678901234567890`.
- `streamPartId`: The stream part id in format `0xa000000000000000000000000000000000000000#01`.

#### Returns:
- The handle of the created client.

### `void proxyClientDelete(Error** errors, uint64_t* numErrors, uint64_t clientHandle)`

Delete a proxy client.

#### Parameters:
- `errors`: The array of errors or NULL.
- `numErrors`: The number of errors.
- `clientHandle`: The client handle of the client to delete.

### `uint64_t proxyClientConnect(Error** errors, uint64_t* numErrors, uint64_t clientHandle, const Proxy* proxies, uint64_t numProxies)`

Connect a proxy client to a list of proxies.

#### Parameters:
- `errors`: The array of errors or NULL.
- `numErrors`: The number of errors.
- `clientHandle`: The client handle of the client to connect.
- `proxies`: The array of proxies.
- `numProxies`: The number of proxies.

#### Returns:
- The number of proxies connected to.

### `void proxyClientDisconnect(Error** errors, uint64_t* numErrors, uint64_t clientHandle)`

Disconnect a proxy client from all proxies.

#### Parameters:
- `errors`: The array of errors or NULL.
- `numErrors`: The number of errors.
- `clientHandle`: The client handle of the client to disconnect.

### `uint64_t proxyClientPublish(Error** errors, uint64_t* numErrors, uint64_t clientHandle, const char* content, uint64_t contentLength)`

Publish a message to the stream.

#### Parameters:
- `errors`: The array of errors or NULL.
- `numErrors`: The number of errors.
- `clientHandle`: The client handle of the client to publish.
- `content`: The content to publish.
- `contentLength`: The length of the content.

#### Returns:
- The number of proxies to which the message was published to.








