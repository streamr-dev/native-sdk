# StreamrProxyClient Android Library

Android library module that provides native Streamr client functionality through JNI bindings.

## Features
- Native Streamr client integration
- Publishing to multiple proxies

## Installation

1. Add the library to your project's `settings.gradle.kts`:

include(":app")
include (":StreamrProxyClient")
project(":StreamrProxyClient").projectDir = File(<Path to the library module>/StreamrProxyClient")

2. Add the dependency in your app's `build.gradle.kts`:

dependencies {
    ... Other dependencies
    // Add this line:
    implementation(project(":StreamrProxyClient"))
}

## Usage

### Basic Example

```kotlin
import network.streamr.proxyclient.StreamrProxyAddress
import network.streamr.proxyclient.StreamrProxyClient

fun publishDataExample() {
    // Initialize the client
    val client = StreamrProxyClient(
        ethereumAddress = "0x1234567890123456789012345678901234567890",
        streamPartId = "0xd2078dc2d780029473a39ce873fc182587be69db/low-level-client#0"
    )

    // Connect to a proxy
    val proxyAddress = StreamrProxyAddress(
        websocketUrl = "ws://95.216.15.80:44211",
        ethereumAddress = "0xd0d14b38d1f6b59d3772a63d84ece0a79e6e1c1f"
    )

    val connectionResult = client.connect(listOf(proxyAddress))
    // Check connection status
    if (connectionResult.numConnected > 0) {
        // Publish a message
        val publishResult = client.publish(
            content = "Hello Streamr!",
            ethereumPrivateKey = "your-private-key"
        )
        if (publishResult.numConnected > 0) {
            println("Message published successfully!")
        }
    }
    // Don't forget to close the client when done
    client.close()
}

```

### Example of Error Handling for the construct of StreamrProxyClient

```kotlin
import network.streamr.proxyclient.ProxyClientException
import network.streamr.proxyclient.StreamrError
import network.streamr.proxyclient.StreamrProxyClient

fun handleProxyClientException() {
    try {
        val client = StreamrProxyClient(
            ethereumAddress = "invalid-address",
            streamPartId = "valid-stream-id"
        )
    } catch (e: ProxyClientException) {
        when (val error = e.error) {
            is StreamrError.InvalidEthereumAddress -> {
                println(error.message)
            }
            is StreamrError.InvalidStreamPartId -> {
                println(error.message)
            }
            else -> {
                println("Unexpected error: ${error.message}")
            }
        }
    }
}
```

### Example of Error Handling for connect 

```kotlin
import network.streamr.proxyclient.StreamrError
import network.streamr.proxyclient.StreamrProxyAddress
import network.streamr.proxyclient.StreamrProxyClient

fun handleConnectErrors(client: StreamrProxyClient) {
    // Try to connect with various potential issues
    val result = client.connect(
        listOf(
            StreamrProxyAddress(
                websocketUrl = "ws://non.existent.proxy.url",
                ethereumAddress = "0x1234567890123456789012345678901234567890"
            )
        )
    )
    // Check connection results
    if (result.numConnected > 0) {
        println("Successfully connected to ${result.numConnected} proxies")
        println("Connected proxies: ${result.successful.map { it.websocketUrl }}")
    } else if (result.failed.isNotEmpty()) {
        // Handle failed connections
        result.failed.forEach { failure ->
            when (val error = failure.error) {
                is StreamrError.ProxyConnectionFailed -> {
                    println("Failed to connect to ${failure.proxy.websocketUrl}: ${error.message}")
                }
                is StreamrError.InvalidProxyUrl -> {
                    println("Invalid proxy URL ${failure.proxy.websocketUrl}: ${error.message}")
                }
                is StreamrError.NoProxiesDefined -> {
                    println("No proxies defined: ${error.message}")
                }
                is StreamrError.InvalidEthereumAddress -> {
                    println("Invalid Ethereum address ${failure.proxy.ethereumAddress}: ${error.message}")
                }
                else -> {
                    println("Unexpected error with ${failure.proxy.websocketUrl}: ${error.message}")
                }
            }
        }
    } else {
        println("No connections attempted")
    }
}

```
