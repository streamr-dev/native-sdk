#ifndef STREAMR_EVENTEMITTER_HPP
#define STREAMR_EVENTEMITTER_HPP

#include <cstddef>
#include <functional>
#include <list>
#include <map>
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

        Handler() = default;
        Handler(const Handler&) = default;
        Handler& operator=(const Handler&) = default;
        Handler(Handler&&) = default;
        Handler& operator=(Handler&&) = default;

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

// Immutable reference to a Handler.
// This is needed because there is no way of checking the validity
// of iterators returned by std::list, and removing a handler twice would
// crash the program if iterators were used.
class HandlerToken {
private:
    size_t mId;
    explicit HandlerToken(size_t id) : mId(id) {}

public:
    HandlerToken() : mId(0) {} // create a non-existent token by default
    HandlerToken(const HandlerToken&) = default;
    HandlerToken& operator=(const HandlerToken&) = default;
    HandlerToken(HandlerToken&&) = default;
    HandlerToken& operator=(HandlerToken&&) = default;

    static HandlerToken create() {
        // Use a "magic static"
        // https://blog.mbedded.ninja/programming/languages/c-plus-plus/magic-statics/
        static size_t counter = 1;
        static std::mutex handlerReferenceCounterMutex;
        std::lock_guard<std::mutex> lock{handlerReferenceCounterMutex};
        return HandlerToken(counter++);
    }
    [[nodiscard]] size_t getId() const { return mId; }
};

// Each event type gets generated its own EventEmitterImpl
template <typename EmitterEventType>
class EventEmitterImpl {
private:
    std::recursive_mutex mEventHandlersMutex;
    std::list<typename EmitterEventType::Handler> mEventHandlers;

    // This mutex ensures that there is only one emit loop executing
    // at a time, and that events get emitted in the correct order

    std::mutex mEmitLoopMutex;

    // Keep the copies of event handlers
    // of the loops that are currently executing in a data structure
    // so that other threads can remove them

    std::mutex mExecutingEmitLoopHandlersMutex;
    std::map<size_t, typename EmitterEventType::Handler>
        mExecutingEmitLoopHandlers;

    // this if called by off()
    void removeHandlerFromExecutingEmitLoops(HandlerToken token) {
        std::lock_guard guard{mExecutingEmitLoopHandlersMutex};
        mExecutingEmitLoopHandlers.erase(token.getId());
    }

    void createExecutingEmitLoopHandlersMap() {
        std::lock_guard guard{mExecutingEmitLoopHandlersMutex};
        mExecutingEmitLoopHandlers.clear();
        for (const auto& handler : mEventHandlers) {
            mExecutingEmitLoopHandlers[handler.getId()] = handler;
        }
    }

    std::optional<typename EmitterEventType::Handler>
    popHandlerFromExecutingEmitLoopHandlersMap() {
        std::lock_guard guard{mExecutingEmitLoopHandlersMutex};

        if (mExecutingEmitLoopHandlers.empty()) {
            return std::nullopt;
        }

        auto handlerIterator = mExecutingEmitLoopHandlers.begin();
        auto handler = handlerIterator->second;

        mExecutingEmitLoopHandlers.erase(handlerIterator);

        return handler;
    }

public:
    /**
     * @brief Add an event listener to the event emitter.
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
        std::lock_guard guard{mEventHandlersMutex};
        typename EmitterEventType::Handler::HandlerFunction handlerFunction =
            callback;
        // silently ignore null callbacks
        if (!handlerFunction) {
            return HandlerToken(); // return a non-existent token
        }
        auto handlerReference = HandlerToken::create();
        typename EmitterEventType::Handler handler(
            handlerFunction, handlerReference.getId(), once);

        mEventHandlers.push_back(handler);
        return handlerReference;
    }

    /**
     * @brief Add an event listener that will be removed after the first time it
     * is called.
     *
     * @tparam EventType The type of the event to listen for.
     * @tparam CallbackType (automatically deduced) The type of the callback
     * function to be called when the event is emitted.
     * @param callback The callback function to be called when the event is
     * emitted.
     * @return HandlerToken
     */
    template <
        MatchingEventType<EmitterEventType> EventType,
        MatchingCallbackType<EmitterEventType> CallbackType>
    HandlerToken once(const CallbackType& callback) {
        return this->on<EventType, CallbackType>(callback, true);
    }

    /**
     * @brief Remove an event listener from the event emitter.
     *
     * @tparam EventType The type of the event to remove the listener from.
     * @param handlerReference The token of the listener to remove.
     */

    template <MatchingEventType<EmitterEventType> EventType>
    void off(HandlerToken handlerReference) {
        std::lock_guard guard{mEventHandlersMutex};

        mEventHandlers.remove_if(
            [&handlerReference](
                const typename EmitterEventType::Handler& handler) {
                return handler.getId() == handlerReference.getId();
            });
        removeHandlerFromExecutingEmitLoops(handlerReference);
    }

    /**
     * @brief Get the number of listeners for an event.
     *
     * @tparam EventType The type of the event to get the listener count for.
     * @return size_t The number of listeners for the event.
     */

    template <MatchingEventType<EmitterEventType> EventType>
    size_t listenerCount() {
        std::lock_guard guard{mEventHandlersMutex};
        return mEventHandlers.size();
    }

    /**
     * @brief Remove all listeners for an event type.
     *
     * @tparam EventType The type of the event to remove all listeners for.
     */

    template <MatchingEventType<EmitterEventType> EventType>
    void removeAllListeners() {
        std::lock_guard guard{mEventHandlersMutex};
        mEventHandlers.clear();
    }

    /**
     * @brief Emit an event to the event emitter.
     *
     * @tparam EventType The type of the event to emit.
     * @tparam EventArgs (automatically deduced) The types of the arguments to
     * emit the event with.
     * @param args The arguments to emit the event with.
     */

    template <
        MatchingEventType<EmitterEventType> EventType,
        typename... EventArgs>
    void emit(EventArgs&&... args) {
        std::lock_guard guard{mEmitLoopMutex};
        createExecutingEmitLoopHandlersMap();

        while (auto ret = popHandlerFromExecutingEmitLoopHandlersMap()) {
            auto handler = ret.value();
            std::invoke(handler, std::forward<EventArgs>(args)...);
            if (handler.isOnce()) {
                std::lock_guard guard{mEventHandlersMutex};
                mEventHandlers.remove(handler);
            }
        }
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

template <typename... EventTypes>
class EventEmitter<std::tuple<EventTypes...>>
    : public EventEmitterImpl<EventTypes>... { // inherit from EventEmitterImpl
                                               // for each EventType

public:
    // Make the inherited methods visible to the templating system
    using EventEmitterImpl<EventTypes>::on...;
    using EventEmitterImpl<EventTypes>::off...;
    using EventEmitterImpl<EventTypes>::listenerCount...;
    using EventEmitterImpl<EventTypes>::removeAllListeners...;
    using EventEmitterImpl<EventTypes>::emit...;

    /**
     * @brief Remove all listeners for all event types.
     *
     */

    template <typename T = std::nullopt_t>
    void removeAllListeners() {
        // Use C++ 17 fold expression to remove all listeners for all event
        // types https://www.foonathan.net/2020/05/fold-tricks/
        (EventEmitterImpl<EventTypes>::template removeAllListeners<
             EventTypes>(),
         ...);
    }
};

} // namespace streamr::eventemitter

#endif
