#include "streamr-utils/runAndWaitForEvents.hpp"
#include <chrono>
#include <thread>
#include <tuple>
#include <gtest/gtest.h>
#include <folly/experimental/coro/Timeout.h>
#include "streamr-eventemitter/EventEmitter.hpp"

using streamr::eventemitter::Event;
using streamr::eventemitter::EventEmitter;
using namespace std::chrono_literals;

using streamr::utils::runAndWaitForEvents;

TEST(RunAndWaitForEventsTest, BasicTest) {
    struct HelloEvent : Event<std::string> {};

    using TestEvents = std::tuple<HelloEvent>;

    EventEmitter<TestEvents> emitter;

    EXPECT_NO_THROW(runAndWaitForEvents(
        {[&emitter]() { emitter.emit<HelloEvent>("kiva"); }},
        std::tuple{emitter.event<HelloEvent>()}));
}

TEST(RunAndWaitForEventsTest, PassesIfEventIsEmittedWithSmallDelay) {
    struct HelloEvent : Event<std::string> {};

    using TestEvents = std::tuple<HelloEvent>;

    EventEmitter<TestEvents> emitter;

    EXPECT_NO_THROW(runAndWaitForEvents(
        {[&emitter]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            emitter.emit<HelloEvent>("kiva");
        }},
        std::tuple{emitter.event<HelloEvent>()},
        1000ms));
}

TEST(RunAndWaitForEventsTest, TimeoutThrows) {
    struct HelloEvent : Event<std::string> {};

    using TestEvents = std::tuple<HelloEvent>;

    EventEmitter<TestEvents> emitter;

    EXPECT_THROW(
        runAndWaitForEvents(
            {[&emitter]() {
                std::this_thread::sleep_for(2000ms);
                emitter.emit<HelloEvent>("kiva");
            }},
            std::tuple{emitter.event<HelloEvent>()},
            10ms),
        folly::FutureTimeout);
}

TEST(RunAndWaitForEventsTest, MultipleEvents) {
    struct HelloEvent : Event<std::string> {};
    struct GoodbyeEvent : Event<std::string> {};

    using TestEvents = std::tuple<HelloEvent, GoodbyeEvent>;

    EventEmitter<TestEvents> emitter;

    EXPECT_NO_THROW(runAndWaitForEvents(
        {[&emitter]() {
            emitter.emit<HelloEvent>("kiva");
            emitter.emit<GoodbyeEvent>("kiva");
        }},
        std::tuple{
            emitter.event<HelloEvent>(), emitter.event<GoodbyeEvent>()}));
}

TEST(RunAndWaitForEventsTest, MultipleEventsTimeoutIfOneDoesNotHappen) {
    struct HelloEvent : Event<std::string> {};
    struct GoodbyeEvent : Event<std::string> {};

    using TestEvents = std::tuple<HelloEvent, GoodbyeEvent>;

    EventEmitter<TestEvents> emitter;

    EXPECT_THROW(
        runAndWaitForEvents(
            {[&emitter]() {
                std::this_thread::sleep_for(20ms);
                emitter.emit<HelloEvent>("kiva");
            }},
            std::tuple{
                emitter.event<HelloEvent>(), emitter.event<GoodbyeEvent>()},
            10ms),
        folly::FutureTimeout);
}
