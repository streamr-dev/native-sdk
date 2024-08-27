# Streamr EventEmitter

Streamr EventEmitter is a C++20 adaptation of the [eventemitter3](https://www.npmjs.com/package/eventemitter3) typescript API as a single-file, zero-dependency header-only library. It allows for following a familiar coding style when porting typescript codebases to C++. Streamr EventEmitter is single-threaded but thread-safe. 

## Differences from the original typescript API

* Event types are defined as structs or classes that inherit from `Event` as C++ does not have literal types of typescript.
* Removing a listener is done by saving the token retuned by the `on` or `once` method, and calling `off` with it. (Pointers to lambdas seem to work differently on different compilers, so it is not possible to use function pointers as removal tokens) 
* The following methods of the original [typescript API](https://www.npmjs.com/package/eventemitter3/index.d.ts) are not supported:

- `eventNames()` (a debugging method - use a debugger instead)
- `listeners()` (a debugging method - use a debugger instead)
- `addListener()` (a duplicate name for `on()`)
- `removeListener()` (a duplicate name for `off()`)

## Usage

Include the `EventEmitter.hpp` header and add using declarations for `Event` and `EventEmitter` for convenience.

```cpp
#include <streamr-eventemitter/EventEmitter.hpp>

using streamr::eventemitter::Event;
using streamr::eventemitter::EventEmitter;
```

**Define your event types** as structs or classes inherited from `Event`. Place the parameter types of the listener of each event in the `Event`'s template parameters. Finally, group the events together in a `std::tuple` type.

```cpp
struct Message : Event<std::string_view> {};
struct Error : Event<std::string_view, uint64_t> {};

using MyEvents = std::tuple<Message, Error>;
```

**Inherit your class from `EventEmitter` with your event types.**
```cpp
class MyClass : public EventEmitter<MyEvents> {};
```

You can also create stand-alone `EventEmitter` instances with your event types.

```cpp
EventEmitter<MyEvents> myEventEmitter;
```

**Add listeners to your event emitter.** Use lambdas or std::bind to create callbacks.
The parameters of the callback must be the same as the template parameters of the event type.

```cpp
myEventEmitter.on<Message>([](std::string_view message) {
    std::cout << "Received message: " << message << std::endl;
});
```

You can also add listeners that will be removed after the first time they are called.

```cpp
myEventEmitter.once<Message>([](std::string_view message) {
    std::cout << "Received message: " << message << std::endl;
});
```

If you want to remove the listener later, save the return value of the `on` or `once` method, and call `off` with it.

```cpp
auto listenerToken = myEventEmitter.on<Message>([](std::string_view message) {
    std::cout << "Received message: " << message << std::endl;
});

myEventEmitter.off<Message>(listenerToken);
```

You can also remove all listeners for a specific event type, or all listeners for all event types.

```cpp
myEventEmitter.removeAllListeners<Message>();
myEventEmitter.removeAllListeners();
```

The method `listenerCount` returns the number of listeners for a specific event type.

```cpp
size_t count = myEventEmitter.listenerCount<Message>();
```

**Emit events.**

```cpp
myEventEmitter.emit<Message>("Hello, world!");
myEventEmitter.emit<Error>("Not found", 404);
```

## Implementation

Because C++ does not have the [literal types of typescript](https://www.typescriptlang.org/docs/handbook/literal-types.html), events are implemented as stucts or classes that inherit from `Event`. This keeps the implementation type-safe and allows compact definition of the event types together with the listener argument types of the format `struct MyEvent: Event<int, bool> {}`. Event types supported by a certain `EventEmitter` instance are grouped together as a `std::tuple<EventType...>` of the event types as in `using MyEvents = std::tuple<Message, Error>`. 

`EventEmitter<std::tuple<EventTypes...>>` is implemented as a template class that takes an `std::tuple` of event types as its template parameter. Following the Mixin design pattern, `EventEmitter` template creates a `EventEmitterImpl<EventType>` for each event type in the tuple and inherits from them all to implement methods with the signatures of the `on<EventType>()` format. This design keeps the implementation of `EventEmitterImpl` very simple, as it only needs to handle events of one event type.   

Thread safety is ensured by protecting the internal data structures of `EventEmitterImpl` with two mutexes. The locks on these mutexes are kept as short as strictly necessary to protect the data structures. Additionally, the event-emitting loop of `EventEmitterImpl` is protected by a third mutex to ensure that the events of a single event type are emitted in order to all listeners even if some listeners take a long time to execute their callback. (See the test case `EventsAreReceivedInOrderEvenIfListenersAreSlow` in [EventEmitterTest.cpp](test/unit/EventEmitterTest.cpp) for an example of how this works.)






