#ifndef STREAMR_EVENTEMITTER_HPP
#define STREAMR_EVENTEMITTER_HPP

#include <cstddef>
#include <functional>
#include <iostream>
#include <list>
#include <mutex>
#include <optional>
#include <tuple>
#include <type_traits>

namespace streamr::eventemitter {

template <class... HandlerArgumentTypes>
struct Event {
    // Generate an Event for all passed HandlerArgumentTypes
    // using Handler = std::function<void(HandlerArgumentTypes...)>;
    class Handler {
    public:
        using HandlerFunction = std::function<void(HandlerArgumentTypes...)>;

    private:
        HandlerFunction handlerFunction;
        bool once;
        size_t id;

    public:
        template <typename CallbackType>
            requires(std::is_assignable<HandlerFunction, CallbackType>::value)
        explicit Handler(CallbackType& callback, bool once = false)
            : handlerFunction(callback), once(once) {
            id = reinterpret_cast<size_t>(&callback);

            std::cout << "Handler created with id: " << id << std::endl;
        }

        bool operator==(const Handler& other) const { return id == other.id; }

        void operator()(HandlerArgumentTypes... args) {
            handlerFunction(std::forward<HandlerArgumentTypes>(args)...);
        }
        [[nodiscard]] bool isOnce() const { return once; }
    };
};
/*
template <typename EventType, typename EmitterEventType>
concept SameAsEmitterEventType = std::is_same<EventType,
EmitterEventType>::value;
*/
// Each event type gets generated its own  EventEmitterImpl
template <typename EmitterEventType>
class EventEmitterImpl {
private:
    std::list<typename EmitterEventType::Handler> eventHandlers;
    std::mutex mutex;

public:
    template <typename EventType, typename CallbackType>
        requires(
            std::is_same<EventType, EmitterEventType>::value &&
            std::is_assignable<
                typename EmitterEventType::Handler::HandlerFunction,
                CallbackType>::value)
    void on(const CallbackType& handlerFunction) {
        std::lock_guard guard{mutex};
        typename EmitterEventType::Handler handler(handlerFunction);
        this->eventHandlers.push_back(handler);
    }

    template <typename EventType, typename CallbackType>
        requires(
            std::is_same<EventType, EmitterEventType>::value &&
            std::is_assignable<
                typename EmitterEventType::Handler::HandlerFunction,
                CallbackType>::value)
    void once(const CallbackType& handlerFunction) {
        std::lock_guard guard{mutex};
        typename EmitterEventType::Handler handler(handlerFunction, true);
        this->eventHandlers.push_back(handler);
    }

    template <typename EventType, typename CallbackType>
        requires(
            std::is_same<EventType, EmitterEventType>::value &&
            std::is_assignable<
                typename EmitterEventType::Handler::HandlerFunction,
                CallbackType>::value)
    void off(const CallbackType& handlerFunctionToRemove) {
        std::lock_guard guard{mutex};
        typename EmitterEventType::Handler handlerToRemove(
            handlerFunctionToRemove);
        this->eventHandlers.remove(handlerToRemove);
    }

    template <typename EventType>
        requires(std::is_same<EventType, EmitterEventType>::value)
    uint32_t listenerCount() {
        std::lock_guard guard{mutex};
        return this->eventHandlers.size();
    }

    template <typename EventType>
        requires(std::is_same<EventType, EmitterEventType>::value)
    void removeAllListeners() {
        std::lock_guard guard{mutex};
        this->eventHandlers.clear();
    }

    template <typename EventType, typename... EventArgs>
        requires(std::is_same<EventType, EmitterEventType>::value)
    void emit(EventArgs&&... args) {
        std::list<typename EmitterEventType::Handler> currentHandlers;

        { // Take copy of eventHandlers at the time of emitting
            std::lock_guard guard{mutex};
            currentHandlers = this->eventHandlers;
            // remove the "once" type handlers that we are going to handle
            this->eventHandlers.remove_if(
                [](const typename EmitterEventType::Handler& handler) {
                    return handler.isOnce();
                });
        }
        // envoke the event on currentHandlers
        for (auto& handler : currentHandlers) {
            std::invoke(handler, std::forward<EventArgs>(args)...);
        }
    }
};

// Disable default specialization
template <typename... EventTypes>
struct EventEmitter;

// Only provide specialization for std::tuple<EventTypes...>
template <typename... EventTypes>

// Generate EventEmitterImpl for each EventType and inherit from all of them
struct EventEmitter<std::tuple<EventTypes...>>
    : public EventEmitterImpl<EventTypes>... {
    using EventEmitterImpl<EventTypes>::on...;
    using EventEmitterImpl<EventTypes>::off...;
    using EventEmitterImpl<EventTypes>::listenerCount...;
    using EventEmitterImpl<EventTypes>::removeAllListeners...;
    using EventEmitterImpl<EventTypes>::emit...;

    template <typename T = std::nullopt_t>
    void removeAllListeners() {
        (EventEmitterImpl<EventTypes>::template removeAllListeners<
             EventTypes>(),
         ...);
    }
};

} // namespace streamr::eventemitter

#endif