#include <future>
#include <string_view>
#include <gtest/gtest.h>

#include "streamr-eventemitter/EventEmitter.hpp"

using streamr::eventemitter::Event;
using streamr::eventemitter::EventEmitter;

class EventEmitterTest : public ::testing::Test {
protected:
    void SetUp() override {}
};

TEST_F(EventEmitterTest, TestEmit) {
    struct Greeting : Event<std::string_view> {};
    using Events = std::tuple<Greeting>;

    EventEmitter<Events> eventEmitter;

    std::promise<std::string_view> promise;

    eventEmitter.on<Greeting>([&promise](std::string_view message) -> void {
        promise.set_value(message);
    });

    eventEmitter.emit<Greeting>("Hello, world!");
    ASSERT_EQ(promise.get_future().get(), "Hello, world!");
}

TEST_F(EventEmitterTest, TestListenerCount) {
    struct Greeting : Event<std::string_view> {};
    using Events = std::tuple<Greeting>;

    EventEmitter<Events> eventEmitter;

    eventEmitter.on<Greeting>([](std::string_view message) -> void {
        std::cout << "listener1: " << message << std::endl;
    });

    eventEmitter.on<Greeting>([](std::string_view message) -> void {
        std::cout << "listener2: " << message << std::endl;
    });

    ASSERT_EQ(eventEmitter.listenerCount<Greeting>(), 2);
}

TEST_F(EventEmitterTest, TestOff) {
    struct Greeting : Event<std::string_view> {};
    using Events = std::tuple<Greeting>;

    EventEmitter<Events> eventEmitter;

    auto listener1 = [](std::string_view message) -> void {
        std::cout << "listener1: " << message << std::endl;
    };

    auto listener2 = [](std::string_view message) -> void {
        std::cout << "listener2: " << message << std::endl;
    };

    eventEmitter.on<Greeting>(listener1);
    eventEmitter.on<Greeting>(listener2);

    ASSERT_EQ(eventEmitter.listenerCount<Greeting>(), 2);

    eventEmitter.off<Greeting>(listener1);

    ASSERT_EQ(eventEmitter.listenerCount<Greeting>(), 1);
}

TEST_F(EventEmitterTest, TestOnce) {
    struct Greeting : Event<std::string_view> {};
    using Events = std::tuple<Greeting>;

    EventEmitter<Events> eventEmitter;

    std::promise<std::string_view> promise;

    eventEmitter.once<Greeting>([&promise](std::string_view message) -> void {
        promise.set_value(message);
    });

    eventEmitter.emit<Greeting>("Hello, once!");
    ASSERT_EQ(promise.get_future().get(), "Hello, once!");

    // Check that the listener was removed
    ASSERT_EQ(eventEmitter.listenerCount<Greeting>(), 0);
}

TEST_F(EventEmitterTest, TestRemoveAllListeners) {
    struct Greeting : Event<std::string_view> {};

    using Events = std::tuple<Greeting>;
    EventEmitter<Events> eventEmitter;

    auto listener1 = [](std::string_view message) -> void {
        std::cout << "listener1: " << message << std::endl;
    };

    auto listener2 = [](std::string_view message) -> void {
        std::cout << "listener2: " << message << std::endl;
    };

    eventEmitter.on<Greeting>(listener1);
    eventEmitter.on<Greeting>(listener2);

    ASSERT_EQ(eventEmitter.listenerCount<Greeting>(), 2);

    eventEmitter.removeAllListeners<Greeting>();

    ASSERT_EQ(eventEmitter.listenerCount<Greeting>(), 0);
}

TEST_F(EventEmitterTest, TestRemoveAllListenersWithoutEventType) {
    struct Greeting : Event<std::string_view> {};
    struct Farewell : Event<std::string_view> {};

    using Events = std::tuple<Greeting, Farewell>;
    EventEmitter<Events> eventEmitter;

    auto listener1 = [](std::string_view message) -> void {
        std::cout << "listener1: " << message << std::endl;
    };

    auto listener2 = [](std::string_view message) -> void {
        std::cout << "listener2: " << message << std::endl;
    };

    eventEmitter.on<Greeting>(listener1);
    eventEmitter.on<Farewell>(listener2);

    ASSERT_EQ(eventEmitter.listenerCount<Greeting>(), 1);
    ASSERT_EQ(eventEmitter.listenerCount<Farewell>(), 1);

    eventEmitter.removeAllListeners();

    ASSERT_EQ(eventEmitter.listenerCount<Greeting>(), 0);
    ASSERT_EQ(eventEmitter.listenerCount<Farewell>(), 0);
}

TEST_F(EventEmitterTest, TestNullSafetyOfEventHandlerPointers) {
    struct Greeting : Event<std::string_view> {};

    using Events = std::tuple<Greeting>;
    EventEmitter<Events> eventEmitter;

    std::function<void(std::string_view)> listener1 = nullptr;

    eventEmitter.on<Greeting>(listener1);
    eventEmitter.emit<Greeting>("Hello, world!");
}

TEST_F(EventEmitterTest, TestOffCalledTwice) {
    struct Greeting : Event<std::string_view> {};

    using Events = std::tuple<Greeting>;
    EventEmitter<Events> eventEmitter;

    auto listener1 = [](std::string_view message) -> void {
        std::cout << "listener1: " << message << std::endl;
    };

    eventEmitter.on<Greeting>(listener1);

    ASSERT_EQ(eventEmitter.listenerCount<Greeting>(), 1);

    eventEmitter.off<Greeting>(listener1);
    ASSERT_EQ(eventEmitter.listenerCount<Greeting>(), 0);

    // Call off() again on the same listener
    eventEmitter.off<Greeting>(listener1);
    ASSERT_EQ(eventEmitter.listenerCount<Greeting>(), 0);
}

TEST_F(EventEmitterTest, TestSameHandlerAddedOnlyOnce) {
    struct Greeting : Event<std::string_view> {};

    using Events = std::tuple<Greeting>;
    EventEmitter<Events> eventEmitter;

    auto listener1 = [](std::string_view message) -> void {
        std::cout << "listener1: " << message << std::endl;
    };

    eventEmitter.on<Greeting>(listener1);
    ASSERT_EQ(eventEmitter.listenerCount<Greeting>(), 1);

    // Try to add the same listener again
    eventEmitter.on<Greeting>(listener1);
    // The count should still be 1 as the same listener cannot be added twice
    ASSERT_EQ(eventEmitter.listenerCount<Greeting>(), 1);
}
