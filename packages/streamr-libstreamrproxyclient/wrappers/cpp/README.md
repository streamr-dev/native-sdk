# StreamrProxyClient C++ Wrapper

This is a C++ wrapper for the StreamrProxyClient library.

## Installation and usage

* Obtain the StreamrProxyClient shared library by building the [native-sdk](https://github.com/streamr-dev/native-sdk) project or by downloading the pre-built library from the [StreamrProxyClient Releases](https://github.com/streamr-dev/native-sdk/releases) page.

* Open the shared library by linking it to your project or by loading it dynamically at runtime using `dlopen`

* Include the StreamrProxyClient.hpp header file in your project.

* See [../../examples/unix-cpp](../../examples/unix-cpp) for a complete example project.

### Publishing a message
```cpp
#include <cassert>
#include <iostream>
#include <string>
#include "StreamrProxyClient.hpp"

int main() {
    // This is a widely-used test account
    const std::string ownEthereumAddress =
        "0xa5374e3c19f15e1847881979dd0c6c9ffe846bd5";
    const std::string ethereumPrivateKey =
        "23bead9b499af21c4c16e4511b3b6b08c3e22e76e0591f5ab5ba8d4c3a5b1820";
    const std::string streamPartId =
        "0xd2078dc2d780029473a39ce873fc182587be69db/low-level-client#0";

    try {
        // Create client instance
        streamrproxyclient::StreamrProxyClient client(
            ownEthereumAddress, ethereumPrivateKey, streamPartId);

        // Connect to proxy
        auto connectResult = client.connect(
            {{.websocketUrl = "ws://95.216.15.80:44211",
              .ethereumAddress =
                  "0xd0d14b38d1f6b59d3772a63d84ece0a79e6e1c1f"}});
        assert(connectResult.errors.empty());

        // Publish message
        const std::string message = "Hello from libstreamrproxyclient C++!";

        auto publishResult = client.publish(message);
        assert(publishResult.errors.empty());

        std::cout << ownEthereumAddress << " published message \"" << message
                  << "\" to " << publishResult.successful.size()
                  << " proxies\n";

    } catch (const streamrproxyclient::StreamrProxyError& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
```
## API Documentation

### StreamrProxyClient Class

Main client class for interacting with Streamr proxies.

#### Constructor
```cpp
StreamrProxyClient(
    const std::string& ownEthereumAddress,
    const std::string& ethereumPrivateKey,
    const std::string& streamPartId)
```
Creates a new StreamrProxyClient instance.

Parameters:
- `ownEthereumAddress` - Ethereum address of the client
- `ethereumPrivateKey` - Private key for the Ethereum address
- `streamPartId` - Stream part ID

Throws: `StreamrProxyError` if client creation fails

#### Methods

##### connect()
```cpp
StreamrProxyResult connect(const std::vector<StreamrProxyAddress>& proxies)
```
Connects to specified proxies.

Parameters:
- `proxies` - Vector of proxy addresses to connect to

Returns: Result containing the proxies that the operation was successful on and the errors that occurred during the operation

##### publish()
```cpp
StreamrProxyResult publish(const std::string& message)
```
Publishes a message through connected proxies.

Parameters:
- `message` - Message to publish

Returns: Result containing the proxies that the operation was successful on and the errors that occurred during the operation

### StreamrProxyError : public std::runtime_error

Represents an error that occurred during a proxy operation.

Members:
- `StreamrProxyAddress proxy` - Proxy address where the error occurred
- `StreamrProxyErrorCode code` - Error code

Methods:
- `std::string what()` - Returns the error message

### StreamrProxyErrorCode variant type

- `ErrorInvalidEthereumAddress` - Indicates an invalid Ethereum address
- `ErrorInvalidStreamPartId` - Indicates an invalid stream part ID
- `ErrorProxyClientNotFound` - Indicates proxy client was not found
- `ErrorInvalidProxyUrl` - Indicates an invalid proxy URL
- `ErrorNoProxiesDefined` - Indicates no proxies were defined
- `ErrorProxyConnectionFailed` - Indicates proxy connection failed
- `ErrorProxyBroadcastFailed` - Indicates proxy broadcast failed

### Structures

#### StreamrProxyAddress
Represents a Streamr proxy address.

Members:
- `std::string websocketUrl` - WebSocket URL of the proxy
- `std::string ethereumAddress` - Ethereum address of the proxy

#### StreamrProxyResult
Contains the results of a proxy operation.

Members:
- `std::vector<StreamrProxyAddress> successful` - List of proxies that the operation was successful on
- `std::vector<StreamrProxyError> errors` - List of errors that occurred during the operation



