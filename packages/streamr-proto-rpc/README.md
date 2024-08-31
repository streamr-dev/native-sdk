# Streamr Proto-RPC

This is the C++ implementation of the [Streamr Proto-RPC library](https://github.com/streamr-dev/network/tree/main/packages/proto-rpc) originally written in TypeScript. 

Streamr Proto-RPC is a transport-independent [protobuf](https://developers.google.com/protocol-buffers) RPC library that can be used as a binary alternative for JSON-RPC. 

## Introduction

Streamr proto-rpc is intended for situations where one would normally use JSON-RPC, but would like to switch to a binary protocol with autogenerated, typesafe code.  

Unlike existing protobuf RPC frameworks such as gRPC, gRPC-web and twirp, proto-rpc is totally transport-independent; it simply produces `Uint8Arrays` and transporting them to the recipient is up to you! You can use Websockets, peer-to-peer networks, WebRTC, UDP, or TCP for the transport, just to name a few.

## Differences from the typescript version

* Generates its own clients and servers using an own protoc plugin
* Does not emit events for outgoing messages (this was a redundant feature)
* outgoingMessageListener has been universally reanmed to outgoinMessageCallback
* Server-side timeouts are not supported

## Usage Examples

Usage examples can be found in the [examples](examples) directory. Example [hello](examples/hello/hello.cpp) demonstrates a simple RPC call between a client and a server. Example [routed-hello](examples/routed-hello/routedhello.cpp) demonstrates how to pass context information through the RPC stack to route messages to the correct recipient.

## Advanced topics

### Passing context information (eg. for routing)

You can pass context information through the RpcCommunicator between the clients, RPC methods and the event handlers. This is
especially useful in case you wish to use a single RpcCommunicator as a server for multiple clients, and need to figure out
where to route the Rpc messagesoutput by the RpcCommunicator. 

For a complete code example of passing context information, see [routed-hello](examples/routed-hello/routedhello.cpp)

### Notifications

Unlike gRPC, proto-rpc supports JSON-RPC style notifications (RPC functions that return nothing). 

- In the .proto service definitions, the notification functions need to have `google.protobuf.Empty` as their return type.

```proto
service WakeUpRpcService {
    rpc wakeUp (WakeUpRequest) returns (google.protobuf.Empty);
}
```

### Errors

All standard errors in the library can be found the [src/Errors.hpp](include/streamr-proto-rpc/Errors.hpp) file.

- `RpcTimeout` is thrown by default whenever the client or server times out its operations.
- `RpcRequestError` is thrown by the server to be sent as an error response back to the client if the request fails.
- `FailedToParse` is thrown whenever parsing a protobuf message fails.
- `FailedToSerialize` is thrown whenever serializing a protobuf message fails.
- `UnknownRcMethod` is thrown whenever a server receives an RPC call for a method that it has not registered

### Timeouts

Client side timeouts can be set along side requests via the ProtoCallContext parameter. By default the client side timeout is `5000` milliseconds.