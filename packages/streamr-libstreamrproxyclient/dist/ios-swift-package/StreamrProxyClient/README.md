# StreamrProxyClient

A Swift package that provides a client interface for connecting to Streamr Network via proxy nodes. This package is part of the Streamr Network ecosystem and enables easy integration with Streamr's data streaming capabilities.

## Features

- Connect to Streamr Network through proxy nodes
- Publish messages to Streamr streams
- Handle multiple proxy connections
- Automatic error handling and recovery
- Thread-safe operations
- Native Swift implementation with C++ bindings

## Installation for iOS (arm64)

1. **Package Extraction**

bash
tar -xzf streamrproxyclient-arm64-ios-2.0.0.tgz

2. **Add Package to Xcode Project**
   - Open your Xcode project
   - Go to `File -> Add Package Dependencies`
   - Click `Add Local...`
   - Navigate to and select the extracted `StreamrProxyClient` folder

3. **Configure Framework Dependencies**
   - Select your app target in Xcode
   - Go to the `General` tab
   - Scroll to `Frameworks, Libraries, and Embedded Content`
   - Click `+` and add:
     * StreamProxyClient
     * libc++abi.tbd

4. **Configure Build Settings**
   - Select your app target
   - Go to `Build Settings` tab
   - Configure C++ settings:
     * Set `C++ Language Dialect` to `GNU++23`
     * Set `C++ and Objective-C Interoperability` to `C++/Objective-C`

5. **Device Configuration**
   - Connect a physical iOS device
   - Select it as your run destination (simulator is not supported)


## Quick Start

import StreamrProxyClient

// Initialize client
let client = try StreamrProxyClient(
  ownEthereumAddress: "0x1234567890123456789012345678901234567890",
streamPartId: "0xd2078dc2d780029473a39ce873fc182587be69db/low-level-client#0"
)
// Create proxy configuration
let proxy = StreamrProxyAddress(
  websocketUrl: "ws://95.216.15.80:44211",
  ethereumAddress: "0xd0d14b38d1f6b59d3772a63d84ece0a79e6e1c1f"
)

// Connect to proxy
let connectResult = client.connect(proxies: [proxy])
// Check connection result
if connectResult.numConnected > 0 {
  // Publish message
  let publishResult = client.publish(content: "test message", ethereumPrivateKey: "0x...")
}

## API Documentation

StreamrProxyClient provides a Swift interface to the C++ StreamrProxyClient library. The interface is defined in the StreamrProxyClientAPI protocol.

The API is documented below. For more information how to use the API, see integration tests in the example app in the repository: https://github.com/streamr-dev/native-sdk-last/native-sdk/packages/streamr-libstreamrproxyclient/examples/ios/LocationShare/LocationShareTests/ProxyClientTests.swift or the example app itself.


/// Protocol defining the Streamr Proxy Client API interface
public protocol StreamrProxyClientAPI {
    /// Initializes a new Streamr Proxy Client
    /// - Parameters:
    ///   - ownEthereumAddress: The Ethereum address of the client (format: 0x...)
    ///   - streamPartId: The stream part identifier (format: 0x...#01)
    /// - Throws: StreamrError if initialization fails
    init(ownEthereumAddress: String, streamPartId: String) throws
    
    /// Connects to the specified proxy nodes
    /// - Parameter proxies: Array of proxy addresses to connect to
    /// - Returns: StreamrProxyResult containing connection status, error details, and proxy node details
    func connect(proxies: [StreamrProxyAddress]) -> StreamrProxyResult
    
    /// Publishes a message to the stream
    /// - Parameters:
    ///   - content: The message content to publish
    ///   - ethereumPrivateKey: Optional private key for message signing
    /// - Returns: StreamrProxyResult containing publish status, error details, and proxy node details
    func publish(content: String, ethereumPrivateKey: String?) -> StreamrProxyResult
}


