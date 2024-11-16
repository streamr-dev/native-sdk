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

### 1. Extract Package
- Terminal: tar -xzf streamrproxyclient-arm64-ios-2.0.0.tgz

### 2. Configure Device
- Open Xcode
- Go to `Product -> Destination -> Manage Run Destinations`
- Connect a physical iOS device
- Select the device as your run destination

### 3. Add Swift Package
- In Xcode, select `File -> Add Package Dependencies`
- Click `Add Local...`
- Navigate to the extracted `StreamrProxyClient` folder
- Select `StreamrProxyClient` for your app target

### 4. Add Required Framework
- Select your app target
- Go to `General` tab
- Under `Frameworks, Libraries, and Embedded Content`:
  - Click `+`
  - Search for `libc++abi.tbd`
  - Click `Add`
  - Click `+` (For some reason, it is not added under `Frameworks, Libraries, and Embedded Content` when adding the first time, it only adds in under Frameworks)
  - Search for `libc++abi.tbd`
  - Click `Add`

### 5. Configure C++ Settings
- Select your app target
- Go to `Build Settings` tab
- Find and set:
  - `C++ Language Dialect` = `GNU++23`
  - `C++ and Objective-C Interoperability` = `C++/Objective-C`

## Quick Start

```swift
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
    let publishResult = client.publish(
        content: "test message",
        ethereumPrivateKey: "0x..."
    )
}
```

## API Documentation

StreamrProxyClient provides a Swift interface to the C++ StreamrProxyClient library. The interface is defined in the StreamrProxyClientAPI protocol.

The API is documented below. For more information how to use the API, see integration tests in the example app in the repository: https://github.com/streamr-dev/native-sdk/blob/cpp-swift-ios-wrappers-packages-and-apps/packages/streamr-libstreamrproxyclient/examples/ios/LocationShare/LocationShareTests/ProxyClientTests.swift or the example app itself.

```swift
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
```

