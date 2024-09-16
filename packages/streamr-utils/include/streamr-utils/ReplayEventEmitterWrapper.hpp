#ifndef STREAMR_UTILS_REPLAY_EVENT_EMITTER_WRAPPER_HPP
#define STREAMR_UTILS_REPLAY_EVENT_EMITTER_WRAPPER_HPP

#include "streamr-eventemitter/EventEmitter.hpp"

namespace streamr::utils {

using streamr::eventemitter::BoundEvent;
using streamr::eventemitter::EventEmitter;
using streamr::eventemitter::EventEmitterImpl;
using streamr::eventemitter::HandlerToken;

template <typename... EventTypes>
struct ReplayEventEmitterWrapper;

template <typename... EventTypes>
class ReplayEventEmitterWrapper<std::tuple<EventTypes...>>
    : public EventEmitterImpl<EventTypes, true>... {
private:
    EventEmitter<std::tuple<EventTypes...>>& mWrappedEventEmitter;
    std::vector<HandlerToken> mHandlerTokens;

    template <typename EType, typename... ATypes>
    struct HandlerSetter;
    // specialization for tuple
    template <typename EType, typename... ATypes>
    struct HandlerSetter<EType, std::tuple<ATypes...>> {
        static HandlerToken setHandler(
            EventEmitter<std::tuple<EventTypes...>>& emitter,
            ReplayEventEmitterWrapper& wrapper) {
            return emitter.template on<EType>([&wrapper](ATypes... args) {
                wrapper.template emit<EType>(args...);
            });
        }
    };

public:
    // Make the inherited methods visible to the templating system
    using EventEmitterImpl<EventTypes, true>::on...;
    using EventEmitterImpl<EventTypes, true>::once...;
    using EventEmitterImpl<EventTypes, true>::off...;
    using EventEmitterImpl<EventTypes, true>::listenerCount...;
    using EventEmitterImpl<EventTypes, true>::removeAllListeners...;
    using EventEmitterImpl<EventTypes, true>::emit...;

    ReplayEventEmitterWrapper() = delete;

    // Constructor for wrapping an existing EventEmitter to provide replay
    // functionality.
    // Usage example:
    // EventEmitter<std::tuple<MyEvent, MyOtherEvent>> eventEmitter;
    // ReplayEventEmitterWrapper<std::tuple<MyEvent, MyOtherEvent>>
    // replayEventEmitter(eventEmitter);

    explicit ReplayEventEmitterWrapper(
        EventEmitter<std::tuple<EventTypes...>>& wrappedEventEmitter)
        : mWrappedEventEmitter(wrappedEventEmitter) {
        // listen to all events of the wrapped event emitter and emit them
        // through this

        (mHandlerTokens.push_back(
             HandlerSetter<EventTypes, typename EventTypes::ArgumentTypes>::
                 setHandler(mWrappedEventEmitter, *this)),
         ...);
    }

    void destroy() {
        // remove all our listeners from the wrapped event emitter
        for (auto& token : mHandlerTokens) {
            (mWrappedEventEmitter.template off<EventTypes>(token), ...);
        }
        removeAllListeners();
    }

    virtual ~ReplayEventEmitterWrapper() = default;

    template <typename EventType>
    [[nodiscard]] BoundEvent<EventEmitter<std::tuple<EventTypes...>>, EventType>
    event() {
        return BoundEvent<EventEmitter<std::tuple<EventTypes...>>, EventType>(
            *this);
    }

    /**
     * @brief Remove all listeners for all event types.
     *
     */

    template <typename T = std::nullopt_t>
    void removeAllListeners() {
        // Use C++ 17 fold expression to remove all listeners for all event
        // types https://www.foonathan.net/2020/05/fold-tricks/
        (EventEmitterImpl<EventTypes, true>::template removeAllListeners<
             EventTypes>(),
         ...);
    }
};

// Inline helper function for wrapping an EventEmitter in a
// ReplayEventEmitterWrapper
template <typename EventTypesTuple>
inline ReplayEventEmitterWrapper<EventTypesTuple>
createReplayEventEmitterWrapper(EventEmitter<EventTypesTuple>& eventEmitter) {
    return ReplayEventEmitterWrapper<EventTypesTuple>(eventEmitter);
}

template <typename EventTypesTuple>
inline std::shared_ptr<ReplayEventEmitterWrapper<EventTypesTuple>>
makeReplayEventEmitterWrapper(EventEmitter<EventTypesTuple>& eventEmitter) {
    return std::make_shared<ReplayEventEmitterWrapper<EventTypesTuple>>(
        eventEmitter);
}

} // namespace streamr::utils

#endif // STREAMR_UTILS_REPLAY_EVENT_EMITTER_WRAPPER_HPP
