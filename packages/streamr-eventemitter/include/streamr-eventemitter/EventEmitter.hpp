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
template <typename EmitterEventType>
class EventEmitterImpl {
private:
    // Immutable reference to a Handler.
    // This is needed because there is no way of checking the validity
    // of iterators returned by std::list, and removing a handler twice would
    // crash the program if iterators were used.
    class HandlerReference {
    private:
        size_t mId;
        explicit HandlerReference(size_t id) : mId(id) {}

    public:
        static HandlerReference create() {
            // Use a "magic static"
            // https://blog.mbedded.ninja/programming/languages/c-plus-plus/magic-statics/
            static size_t counter = 1;
            static std::mutex handlerReferenceCounterMutex;
            std::lock_guard<std::mutex> lock(handlerReferenceCounterMutex);
            return HandlerReference(counter++);
        }
        static HandlerReference createNonExistent() {
            return HandlerReference(0);
        }
        [[nodiscard]] size_t getId() const { return mId; }
    };

    std::list<typename EmitterEventType::Handler> mEventHandlers;
    std::mutex mMutex;

public:
    template <
        MatchingEventType<EmitterEventType> EventType,
        MatchingCallbackType<EmitterEventType> CallbackType>

    HandlerReference on(const CallbackType& callback, bool once = false) {
        std::lock_guard guard{mMutex};
        typename EmitterEventType::Handler::HandlerFunction handlerFunction =
            callback;
        // silently ignore null callbacks
        if (!handlerFunction) {
            return HandlerReference::createNonExistent();
        }
        auto handlerReference = HandlerReference::create();
        typename EmitterEventType::Handler handler(
            handlerFunction, handlerReference.getId(), once);

        mEventHandlers.push_back(handler);
        return handlerReference;
    }

    template <
        MatchingEventType<EmitterEventType> EventType,
        MatchingCallbackType<EmitterEventType> CallbackType>
    HandlerReference once(const CallbackType& callback) {
        return this->on<EventType, CallbackType>(callback, true);
    }

    template <MatchingEventType<EmitterEventType> EventType>
    void off(HandlerReference handlerReference) {
        std::lock_guard guard{mMutex};

        mEventHandlers.remove_if(
            [&handlerReference](
                const typename EmitterEventType::Handler& handler) {
                return handler.getId() == handlerReference.getId();
            });
    }

    template <MatchingEventType<EmitterEventType> EventType>
    uint32_t listenerCount() {
        std::lock_guard guard{mMutex};
        return mEventHandlers.size();
    }

    template <MatchingEventType<EmitterEventType> EventType>
    void removeAllListeners() {
        std::lock_guard guard{mMutex};
        mEventHandlers.clear();
    }

    template <
        MatchingEventType<EmitterEventType> EventType,
        typename... EventArgs>
    void emit(EventArgs&&... args) {
        std::lock_guard guard{mMutex};

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
// inherit from all of them
template <typename... EventTypes>
struct EventEmitter<std::tuple<EventTypes...>>
    : public EventEmitterImpl<EventTypes>... {
    // Make the inherited methods visible to the templating system
    using EventEmitterImpl<EventTypes>::on...;
    using EventEmitterImpl<EventTypes>::off...;
    using EventEmitterImpl<EventTypes>::listenerCount...;
    using EventEmitterImpl<EventTypes>::removeAllListeners...;
    using EventEmitterImpl<EventTypes>::emit...;

    // Use C++ 17 fold expression to remove all listeners for all event types
    // https://en.cppreference.com/w/cpp/language/fold

    template <typename T = std::nullopt_t>
    void removeAllListeners() {
        (EventEmitterImpl<EventTypes>::template removeAllListeners<
             EventTypes>(),
         ...);
    }
};

} // namespace streamr::eventemitter

#endif
