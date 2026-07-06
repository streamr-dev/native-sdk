#include <chrono>
#include <cstddef>
#include <future>
#include <iostream>
#include <list>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <vector>
#include <gtest/gtest.h>

import streamr.eventemitter.EventEmitter;

using streamr::eventemitter::Event;
using streamr::eventemitter::EventEmitter;
using streamr::eventemitter::HandlerToken;
using streamr::eventemitter::ReplayEventEmitter;

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

TEST_F(EventEmitterTest, EachListenerReceivesItsOwnCopyOfMoveSensitiveArgs) {
    // Regression test: the emit loop must hand each listener its own copy
    // of the arguments. It used to std::forward the arguments into every
    // invocation, so the first listener move-constructed its by-value
    // parameter from the emit arguments and every later listener saw a
    // moved-from (empty) value.
    struct Payload : Event<std::vector<std::byte>> {};
    using Events = std::tuple<Payload>;

    EventEmitter<Events> eventEmitter;

    std::vector<size_t> receivedSizes;
    std::mutex receivedSizesMutex;
    auto record = [&receivedSizes,
                   &receivedSizesMutex](const std::vector<std::byte>& data) {
        std::lock_guard lock(receivedSizesMutex);
        receivedSizes.push_back(data.size());
    };
    eventEmitter.on<Payload>(record);
    eventEmitter.on<Payload>(record);
    eventEmitter.on<Payload>(record);

    constexpr size_t payloadSize = 4;
    constexpr std::byte fillByte{0x07};
    std::vector<std::byte> data(payloadSize, fillByte);
    eventEmitter.emit<Payload>(data);

    ASSERT_EQ(receivedSizes.size(), 3U);
    for (const auto size : receivedSizes) {
        EXPECT_EQ(size, payloadSize);
    }
}

TEST_F(
    EventEmitterTest,
    ReplayEmitterReplaysLatestEventExactlyOnceToLateListener) {
    struct Greeting : Event<std::string> {};
    using Events = std::tuple<Greeting>;

    ReplayEventEmitter<Events> eventEmitter;
    eventEmitter.emit<Greeting>("stored");

    int callCount = 0;
    std::string lastMessage;
    eventEmitter.on<Greeting>([&callCount, &lastMessage](std::string message) {
        ++callCount;
        lastMessage = std::move(message);
    });

    EXPECT_EQ(callCount, 1);
    EXPECT_EQ(lastMessage, "stored");
}

TEST_F(
    EventEmitterTest,
    ReplayEmitterHoldsNoHandlerMutexWhileInvokingReplayHandler) {
    // The replayed handler must be invoked with NO internal EventEmitter
    // mutex held, otherwise a handler that takes a user lock deadlocks
    // ABBA against another thread that holds that user lock and calls
    // off() (which needs the handler mutex). A same-thread re-entrancy
    // test cannot detect this — the handler mutex is recursive — so this
    // uses a second thread: from inside the replay it calls
    // listenerCount(), which needs the handler mutex. If the replay held
    // that mutex, the second thread would block until the replay
    // returned, but the replay waits on the second thread — a deadlock,
    // caught here as the 2 s timeout rather than a genuine hang.
    struct Ping : Event<> {};
    using Events = std::tuple<Ping>;

    ReplayEventEmitter<Events> eventEmitter;
    eventEmitter.emit<Ping>();

    bool replayInvoked = false;
    std::future_status otherThreadStatus = std::future_status::timeout;
    eventEmitter.on<Ping>([&]() {
        replayInvoked = true;
        auto future = std::async(std::launch::async, [&eventEmitter]() {
            return eventEmitter.listenerCount<Ping>();
        });
        otherThreadStatus = future.wait_for(std::chrono::seconds(2));
        if (otherThreadStatus == std::future_status::ready) {
            [[maybe_unused]] auto count = future.get();
        }
    });

    EXPECT_TRUE(replayInvoked);
    EXPECT_EQ(otherThreadStatus, std::future_status::ready);
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
        std::cout << "listener1: " << message << '\n';
    });

    eventEmitter.on<Greeting>([](std::string_view message) -> void {
        std::cout << "listener2: " << message << '\n';
    });

    ASSERT_EQ(eventEmitter.listenerCount<Greeting>(), 2);
}

TEST_F(EventEmitterTest, TestOff) {
    struct Greeting : Event<std::string_view> {};
    using Events = std::tuple<Greeting>;

    EventEmitter<Events> eventEmitter;

    auto listener1 = [](std::string_view message) -> void {
        std::cout << "listener1: " << message << '\n';
    };

    auto listener2 = [](std::string_view message) -> void {
        std::cout << "listener2: " << message << '\n';
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
        std::cout << "listener1: " << message << '\n';
    };

    auto listener2 = [](std::string_view message) -> void {
        std::cout << "listener2: " << message << '\n';
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
        std::cout << "listener1: " << message << '\n';
    };

    auto listener2 = [](std::string_view message) -> void {
        std::cout << "listener2: " << message << '\n';
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

TEST_F(EventEmitterTest, TestEmitWithNoListeners) {
    struct Greeting : Event<std::string_view> {};

    using Events = std::tuple<Greeting>;
    EventEmitter<Events> eventEmitter;

    // Emit an event when there are no listeners
    eventEmitter.emit<Greeting>("Hello, world!");

    // Ensure that no listeners are present
    ASSERT_EQ(eventEmitter.listenerCount<Greeting>(), 0);
}

TEST_F(EventEmitterTest, TestOffCalledTwice) {
    struct Greeting : Event<std::string_view> {};

    using Events = std::tuple<Greeting>;
    EventEmitter<Events> eventEmitter;

    auto listener1 = [](std::string_view message) -> void {
        std::cout << "listener1: " << message << '\n';
    };

    auto listener1Reference = eventEmitter.on<Greeting>(listener1);

    ASSERT_EQ(eventEmitter.listenerCount<Greeting>(), 1);

    eventEmitter.off<Greeting>(listener1Reference);
    ASSERT_EQ(eventEmitter.listenerCount<Greeting>(), 0);

    // Call off() again on the same listener
    eventEmitter.off<Greeting>(listener1Reference);
    ASSERT_EQ(eventEmitter.listenerCount<Greeting>(), 0);
}

TEST_F(EventEmitterTest, ListenerCanRemoveItself) {
    struct Shutdown : Event<> {};
    using Events = std::tuple<Shutdown>;

    EventEmitter<Events> eventEmitter;
    HandlerToken* token = nullptr;

    auto listener = [&eventEmitter, &token]() -> void {
        eventEmitter.off<Shutdown>(*token);
    };

    auto handlerToken = eventEmitter.on<Shutdown>(listener);
    token = &handlerToken;
    eventEmitter.emit<Shutdown>();
    ASSERT_EQ(eventEmitter.listenerCount<Shutdown>(), 0);
}

constexpr int longRunningHandlerSleepTimeSeconds = 1;
constexpr int removalDelayMs = 500;

TEST_F(
    EventEmitterTest,
    RemoveAllListenersCalledWhileExecutingLongRunningHandler) {
    struct LongRunningEvent : Event<> {};
    using Events = std::tuple<LongRunningEvent>;

    EventEmitter<Events> eventEmitter;

    auto longRunningHandler = []() -> void {
        std::this_thread::sleep_for(
            std::chrono::seconds(longRunningHandlerSleepTimeSeconds));
    };

    auto handlerToken = eventEmitter.on<LongRunningEvent>(longRunningHandler);

    std::thread removalThread([&eventEmitter]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(removalDelayMs));
        eventEmitter.removeAllListeners<LongRunningEvent>();
    });

    eventEmitter.emit<LongRunningEvent>();

    removalThread.join();

    ASSERT_EQ(eventEmitter.listenerCount<LongRunningEvent>(), 0);
}

TEST_F(EventEmitterTest, OffCalledWhileExecutingLongRunningHandler) {
    struct LongRunningEvent : Event<> {};
    using Events = std::tuple<LongRunningEvent>;

    EventEmitter<Events> eventEmitter;
    HandlerToken* token = nullptr;

    auto longRunningHandler = [&token]() -> void {
        std::this_thread::sleep_for(
            std::chrono::seconds(longRunningHandlerSleepTimeSeconds));
    };

    auto handlerToken = eventEmitter.on<LongRunningEvent>(longRunningHandler);
    token = &handlerToken;

    std::thread interruptingThread([&eventEmitter, &token]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(removalDelayMs));
        eventEmitter.off<LongRunningEvent>(*token);
    });

    eventEmitter.emit<LongRunningEvent>();

    interruptingThread.join();

    ASSERT_EQ(eventEmitter.listenerCount<LongRunningEvent>(), 0);
}

// This test fails if emitting loop is not locked with a mutex
TEST_F(EventEmitterTest, EventsAreReceivedInOrderEvenIfListenersAreSlow) {
    struct TestEvent : Event<int> {};
    using Events = std::tuple<TestEvent>;

    EventEmitter<Events> eventEmitter;

    std::promise<void> promiseA1;
    std::promise<void> promiseA2;
    std::promise<void> promiseB1;
    std::promise<void> promiseB2;

    std::atomic<int> listenerACount(0);

    auto listenerA =
        [&promiseA1, &promiseA2, &listenerACount](int /* eventNum */) -> void {
        if (listenerACount.fetch_add(1) == 0) {
            // Make this listener call last more than 1 second
            // for the first event received
            std::this_thread::sleep_for(
                std::chrono::seconds(longRunningHandlerSleepTimeSeconds));
            promiseA1.set_value();
        } else {
            // Apply no delay for the second event
            promiseA2.set_value();
        }
    };

    std::mutex listenerBEventOrderMutex;
    std::vector<int> listenerBEventOrder;

    auto listenerB = [&promiseB1,
                      &promiseB2,
                      &listenerBEventOrderMutex,
                      &listenerBEventOrder](int eventNum) -> void {
        if (eventNum == 1) {
            std::cout << "event1 arrived at listener B" << '\n';
            std::lock_guard<std::mutex> lock(listenerBEventOrderMutex);
            listenerBEventOrder.push_back(eventNum);
            promiseB1.set_value();
        } else {
            // Without mutex lock, this event will arrive first
            // because there is a long delay for the firt event
            // at listenerA
            std::cout << "event2 arrived at listener B" << '\n';
            std::lock_guard<std::mutex> lock(listenerBEventOrderMutex);
            listenerBEventOrder.push_back(eventNum);
            promiseB2.set_value();
        }
    };

    eventEmitter.on<TestEvent>(listenerA);
    eventEmitter.on<TestEvent>(listenerB);

    std::thread thread1([&eventEmitter]() { eventEmitter.emit<TestEvent>(1); });

    std::thread thread2([&eventEmitter]() {
        constexpr int sleepDurationMs = 100;
        std::this_thread::sleep_for(std::chrono::milliseconds(sleepDurationMs));
        eventEmitter.emit<TestEvent>(2);
    });

    promiseA1.get_future().get();
    promiseA2.get_future().get();
    promiseB1.get_future().get();
    promiseB2.get_future().get();

    thread1.join();
    thread2.join();

    ASSERT_EQ(listenerACount.load(), 2);
    ASSERT_EQ(listenerBEventOrder, std::vector<int>({1, 2}));
}

TEST_F(EventEmitterTest, TestEventMethod) {
    struct Greeting : Event<std::string_view> {};
    using Events = std::tuple<Greeting>;

    EventEmitter<Events> eventEmitter;

    std::promise<std::string_view> promise1;

    auto boundEvent = eventEmitter.event<Greeting>();
    boundEvent.getEmitter().template on<decltype(boundEvent)::EventType>(
        [&promise1](std::string_view message) -> void {
            promise1.set_value(message);
        });

    eventEmitter.emit<Greeting>("Hello, world!");
    ASSERT_EQ(promise1.get_future().get(), "Hello, world!");
}
