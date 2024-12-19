# Python wrapper for the StreamrProxyClient

This is a Python wrapper for the StreamrProxyClient C++ library. It is used to publish data to the Streamr network.

## Installation

```bash
python -m pip install libstreamrproxyclient
```
The package is distributed as a binary wheel and is available for MacOS (arm64 and x86_64) and Linux (arm64 and x86_64).

## Usage example

```python
from streamrproxyclient.libstreamrproxyclient import (
    Proxy,
    LibStreamrProxyClient,
    ProxyClient
)

proxy_ethereum_address = "0xd0d14b38d1f6b59d3772a63d84ece0a79e6e1c1f"
proxy_url = "ws://95.216.15.80:44211"
stream_part_id = "0xd2078dc2d780029473a39ce873fc182587be69db/low-level-client#0"
own_ethereum_address = "0xa5374e3c19f15e1847881979dd0c6c9ffe846bd5"
ethereum_private_key = "23bead9b499af21c4c16e4511b3b6b08c3e22e76e0591f5ab5ba8d4c3a5b1820"
 
        
with LibStreamrProxyClient() as lib:
    with ProxyClient(lib, own_ethereum_address, stream_part_id) as client:
        result = client.connect([Proxy(proxy_url, proxy_ethereum_address)])
        assert len(result.errors) == 0
        assert len(result.successful) == 1
        
        result = client.publish(b"Hello from python!", ethereum_private_key)
        assert len(result.errors) == 0
        assert len(result.successful) == 1
```

## API documentation

### Classes

#### Proxy
Contains information about a proxy node in the Streamr network.

Methods:
- `__init__(websocket_url: str, ethereum_address: str)`: Initialize a Proxy instance with websocket URL and Ethereum address
- `from_c_proxy(c_proxy)`: Create Proxy instance from C struct (internal use)

#### Error
Represents an error from the C library.

Attributes:
- `message`: Error message string
- `code`: Error code string 
- `proxy`: Associated Proxy instance if applicable

#### ProxyClientException
Exception raised when C library operations fail.

Attributes:
- `error`: The Error instance containing details

#### ProxyClientResult 
Result of proxy client operations.

Attributes:
- `errors`: List of Error instances if operation had errors
- `successful`: List of successful Proxy instances

#### LibStreamrProxyClient
Wrapper for the C library. Use as context manager.

Methods:
- `__enter__()`: Load and initialize C library
- `__exit__()`: Cleanup C library

#### ProxyClient
Main client for interacting with proxies. Use as context manager.

Methods:
- `__init__(lib: LibStreamrProxyClient, ownEthereumAddress: str, streamPartId: str)`: Initialize with library instance, Ethereum address and stream ID
- `connect(proxies: list[Proxy]) -> ProxyClientResult`: Connect to list of proxies
- `publish(data: bytes, ethereumPrivateKey: str = None) -> ProxyClientResult`: Publish data to connected proxies

### Error Codes

- `ERROR_INVALID_ETHEREUM_ADDRESS`: Invalid Ethereum address format
- `ERROR_INVALID_STREAM_PART_ID`: Invalid stream part ID format  
- `ERROR_PROXY_CLIENT_NOT_FOUND`: Proxy client instance not found
- `ERROR_INVALID_PROXY_URL`: Invalid proxy websocket URL
- `ERROR_NO_PROXIES_DEFINED`: No proxies provided
- `ERROR_PROXY_CONNECTION_FAILED`: Failed to connect to proxy
- `ERROR_PROXY_BROADCAST_FAILED`: Failed to broadcast message to proxy

