#ifndef STREAMR_EVENTEMITTER_HPP
#define STREAMR_EVENTEMITTER_HPP

#include <cstddef>
#include <functional>
#include <list>
#include <mutex>
#include <optional>
#include <tuple>
#include <type_traits>

namespace streamr::eventemitter {

// Event class that all events must inherit from.
// Usage example:
// struct MyEvent : Event<int, std::string> {};

template <typename... HandlerArgumentTypes>
struct Event {
    class Handler {
    public:
        using HandlerFunction = std::function<void(HandlerArgumentTypes...)>;

    private:
        HandlerFunction mHandlerFunction;
        size_t mId;
        bool mOnce;

    public:
        explicit Handler(
            HandlerFunction callback, size_t handlerId, bool once = false)
            : mHandlerFunction(callback), mId(handlerId), mOnce(once) {}

        void operator()(HandlerArgumentTypes... args) {
            mHandlerFunction(std::forward<HandlerArgumentTypes>(args)...);
        }

        bool operator==(const Handler& other) const { return mId == other.mId; }

        [[nodiscard]] bool isOnce() const { return mOnce; }

        [[nodiscard]] size_t getId() const { return mId; }
    };
};

template <typename T, typename EmitterEventType>
concept MatchingEventType = std::is_same<T, EmitterEventType>::value;

template <typename T, typename EmitterEventType>
concept MatchingCallbackType =
    std::is_assignable<typename EmitterEventType::Handler::HandlerFunction, T>::
        value;

// Each event type gets generated its own EventEmitterImpl
template <typename EmitterEventType, typename HostEventEmitter>
class EventEmitterImpl {
private:
    // Immutable reference to a Handler.
    // This is needed because there is no way of checking the validity
    // of iterators returned by std::list, and removing a handler twice would
    // crash the program if iterators were used.
    class HandlerToken {
    private:
        size_t mId;
        explicit HandlerToken(size_t id) : mId(id) {}

    public:
        static HandlerToken create() {
            // Use a "magic static"
            // https://blog.mbedded.ninja/programming/languages/c-plus-plus/magic-statics/
            static size_t counter = 1;
            static std::mutex handlerReferenceCounterMutex;
            std::lock_guard<std::mutex> lock(handlerReferenceCounterMutex);
            return HandlerToken(counter++);
        }
        static HandlerToken createNonExistent() { return HandlerToken(0); }
        [[nodiscard]] size_t getId() const { return mId; }
    };

    std::list<typename EmitterEventType::Handler> mEventHandlers;
    std::mutex& getMutex() {
        // CRTP design pattern:
        // https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern
        // Get the shared mutex from the HostEventEmitter
        return static_cast<HostEventEmitter*>(this)->getMutex();
    }

public:
    /**
     * @brief Adds an event listener to the event emitter.
     * @tparam EventType The type of the event to listen for.
     * @tparam CallbackType (automatically deduced) The type of the callback
     * function to be called when the event is emitted.
     * @param callback The callback function to be called when the event is
     * emitted.
     * @param once Whether the listener should be removed after the first time
     * it is called.
     * @return A token that can be used to remove the listener.
     * @details Usage: eventEmitter.on<MyEvent>(myCallbackLambda);
     */
    template <
        MatchingEventType<EmitterEventType> EventType,
        MatchingCallbackType<EmitterEventType> CallbackType>
    HandlerToken on(const CallbackType& callback, bool once = false) {
        std::lock_guard guard{getMutex()};
        typename EmitterEventType::Handler::HandlerFunction handlerFunction =
            callback;
        // silently ignore null callbacks
        if (!handlerFunction) {
            return HandlerToken::createNonExistent();
        }
        auto handlerReference = HandlerToken::create();
        typename EmitterEventType::Handler handler(
            handlerFunction, handlerReference.getId(), once);

        mEventHandlers.push_back(handler);
        return handlerReference;
    }

    template <
        MatchingEventType<EmitterEventType> EventType,
        MatchingCallbackType<EmitterEventType> CallbackType>
    HandlerToken once(const CallbackType& callback) {
        return this->on<EventType, CallbackType>(callback, true);
    }

    template <MatchingEventType<EmitterEventType> EventType>
    void off(HandlerToken handlerReference) {
        std::lock_guard guard{getMutex()};

        mEventHandlers.remove_if(
            [&handlerReference](
                const typename EmitterEventType::Handler& handler) {
                return handler.getId() == handlerReference.getId();
            });
    }

    template <MatchingEventType<EmitterEventType> EventType>
    size_t listenerCount() {
        std::lock_guard guard{getMutex()};
        return mEventHandlers.size();
    }

    template <MatchingEventType<EmitterEventType> EventType>
    void removeAllListeners() {
        std::lock_guard guard{getMutex()};
        mEventHandlers.clear();
    }

    template <
        MatchingEventType<EmitterEventType> EventType,
        typename... EventArgs>
    void emit(EventArgs&&... args) {
        std::lock_guard guard{getMutex()};

        // invoke the event on currentHandlers
        for (auto& handler : mEventHandlers) {
            std::invoke(handler, std::forward<EventArgs>(args)...);
        }

        // remove the "once" type handlers that we just handled
        mEventHandlers.remove_if(
            [](const typename EmitterEventType::Handler& handler) {
                return handler.isOnce();
            });
    }
};

// Disable default specialization
template <typename... EventTypes>
struct EventEmitter;

// Only provide specialization for std::tuple<EventTypes...>
// Usage example:
// using Events = std::tuple<MyEvent, MyOtherEvent>;
// EventEmitter<Events> eventEmitter;

// Mixin design pattern: generate EventEmitterImpl for each EventType and
// inherit from them all.
// Curiously recurring template pattern (CRTP):
// https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern
// pass the EventEmitter type as the HostEventEmitter to the EventEmitterImpls
// to allow the EventEmitterImpls to access the same mutex.
template <typename... EventTypes>
class EventEmitter<std::tuple<EventTypes...>>
    : public EventEmitterImpl< // inherit from EventEmitterImpl for each
                               // EventType
          EventTypes,
          EventEmitter<std::tuple<EventTypes...>>>... { // pass own type as
                                                        // HostEventEmitter
private:
    // Mutex shared by all EventEmitterImpls
    std::mutex mMutex;

    // Define an alias for own type to make the code below more readable
    using SelfType = EventEmitter<std::tuple<EventTypes...>>;

public:
    // Accessor for EventEmitterImpls
    std::mutex& getMutex() { return mMutex; }

    // Make the inherited methods visible to the templating system
    using EventEmitterImpl<EventTypes, SelfType>::on...;
    using EventEmitterImpl<EventTypes, SelfType>::off...;
    using EventEmitterImpl<EventTypes, SelfType>::listenerCount...;
    using EventEmitterImpl<EventTypes, SelfType>::removeAllListeners...;
    using EventEmitterImpl<EventTypes, SelfType>::emit...;

    // Use C++ 17 fold expression to remove all listeners for all event types
    // https://www.foonathan.net/2020/05/fold-tricks/

    template <typename T = std::nullopt_t>
    void removeAllListeners() {
        (EventEmitterImpl<EventTypes, SelfType>::template removeAllListeners<
             EventTypes>(),
         ...);
    }
};

} // namespace streamr::eventemitter

#endif
