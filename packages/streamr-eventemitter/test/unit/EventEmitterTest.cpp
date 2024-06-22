#include <cstddef>
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

TEST_F(EventEmitterTest, TestEmitMultipleListeners) {
    struct Greeting : Event<std::string_view> {};
    using Events = std::tuple<Greeting>;

    EventEmitter<Events> eventEmitter;

    std::promise<std::string_view> promise1;
    std::promise<std::string_view> promise2;

    eventEmitter.on<Greeting>([&promise1](std::string_view message) -> void {
        promise1.set_value(message);
    });

    eventEmitter.on<Greeting>([&promise2](std::string_view message) -> void {
        promise2.set_value(message);
    });

    eventEmitter.emit<Greeting>("Hello, world!");

    ASSERT_EQ(promise1.get_future().get(), "Hello, world!");
    ASSERT_EQ(promise2.get_future().get(), "Hello, world!");
}

TEST_F(EventEmitterTest, TestEmitFromMultipleThreads) {
    constexpr int numEvents = 8;

    struct Greeting : Event<> {};
    using Events = std::tuple<Greeting>;

    EventEmitter<Events> eventEmitter;

    std::vector<size_t> eventCounters(numEvents);
    std::mutex countersMutex;

    for (auto& eventCounter : eventCounters) {
        eventEmitter.on<Greeting>([&eventCounter, &countersMutex]() -> void {
            std::lock_guard<std::mutex> lock(countersMutex);
            eventCounter++;
        });
    }

    std::list<std::future<void>> asyncTasks;
    for (int i = 0; i < numEvents; ++i) {
        asyncTasks.push_back(std::async(std::launch::async, [&eventEmitter]() {
            eventEmitter.emit<Greeting>();
        }));
    }

    // Wait for all tasks to finish
    for (auto& task : asyncTasks) {
        task.get();
    }

    for (auto& eventCounter : eventCounters) {
        ASSERT_EQ(eventCounter, numEvents);
    }
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

    auto listener1Reference = eventEmitter.on<Greeting>(listener1);
    eventEmitter.on<Greeting>(listener2);

    ASSERT_EQ(eventEmitter.listenerCount<Greeting>(), 2);

    eventEmitter.off<Greeting>(listener1Reference);

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

    auto listener1Reference = eventEmitter.on<Greeting>(listener1);

    ASSERT_EQ(eventEmitter.listenerCount<Greeting>(), 1);

    eventEmitter.off<Greeting>(listener1Reference);
    ASSERT_EQ(eventEmitter.listenerCount<Greeting>(), 0);

    // Call off() again on the same listener
    eventEmitter.off<Greeting>(listener1Reference);
    ASSERT_EQ(eventEmitter.listenerCount<Greeting>(), 0);
}
