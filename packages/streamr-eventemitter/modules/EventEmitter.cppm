// Module streamr.eventemitter.EventEmitter
// CONSOLIDATED from the former header
// streamr-eventemitter/EventEmitter.hpp (MODERNIZATION.md Phase 2.6):
// this file is now the source of truth.
module;

#include <algorithm>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <list>
#include <map>
#include <mutex>
#include <optional>
#include <thread>
#include <tuple>
#include <type_traits>
#include <vector>

export module streamr.eventemitter.EventEmitter;

namespace streamr::eventemitter::detail {

// Number of event-handler invocations currently on THIS thread's call
// stack, across ALL emitter instances and event types. offAndWait() and
// removeAllListenersAndWait() block on in-flight invocations only when
// this is zero: a wait issued from inside a handler can deadlock,
// because handlers routinely remove each other's listeners from within
// handler context. The proven cycle (Layer1ScaleTest, 2026-07-13):
// thread A ran a connection's Data handler (handshake success ->
// stopHandshaker -> off<Disconnected>) while thread B ran the same
// connection's Disconnected handler (close tie-break -> off<Data>) —
// each waited for the other's invocation to end, forever. Depth must be
// global (not per-emitter): the cycle spans emitter instances.
// With the depth guard, every blocked waiter runs no handler itself, so
// wait edges only point from non-handler threads to handler-running
// threads and no cycle can form among them.
inline thread_local size_t handlerInvocationDepth = 0;

struct InvocationDepthGuard {
    InvocationDepthGuard() { ++handlerInvocationDepth; }
    ~InvocationDepthGuard() { --handlerInvocationDepth; }
    InvocationDepthGuard(const InvocationDepthGuard&) = delete;
    InvocationDepthGuard& operator=(const InvocationDepthGuard&) = delete;
    InvocationDepthGuard(InvocationDepthGuard&&) = delete;
    InvocationDepthGuard& operator=(InvocationDepthGuard&&) = delete;
};

} // namespace streamr::eventemitter::detail

export namespace streamr::eventemitter {

// Event class that all events must inherit from.
// Usage example:
// struct MyEvent : Event<int, std::string> {};

template <typename... HandlerArgumentTypes>
struct Event {
    using ArgumentTypes = std::tuple<HandlerArgumentTypes...>;
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

template <typename... HandlerArgumentTypes>
class StoredEvent;

template <typename... HandlerArgumentTypes>
class StoredEvent<std::tuple<HandlerArgumentTypes...>>
    : Event<HandlerArgumentTypes...> {
private:
    Event<HandlerArgumentTypes...>::ArgumentTypes arguments;

public:
    explicit StoredEvent(HandlerArgumentTypes... args)
        : arguments(
              std::make_tuple(std::forward<HandlerArgumentTypes>(args)...)) {}

    // explicit StoredEvent(std::tuple<HandlerArgumentTypes...> args)
    //     : arguments(std::move(args)) {}

    [[nodiscard]] const Event<HandlerArgumentTypes...>::ArgumentTypes&
    getArguments() const {
        return arguments;
    }
};

template <typename T, typename EmitterEventType>
concept MatchingEventType = std::is_same_v<T, EmitterEventType>;

template <typename T, typename EmitterEventType>
concept MatchingCallbackType = std::
    is_assignable_v<typename EmitterEventType::Handler::HandlerFunction, T>;

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
template <
    typename EmitterEventType,
    bool ReplayLatestEventToNewListeners = false>
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

    // Invocations currently RUNNING, per handler id (one entry per
    // invoking thread; replays and the emit loop may overlap). Guarded
    // by mExecutingEmitLoopHandlersMutex. offAndWait() blocks on
    // mInvocationFinished until the removed handler has no in-flight
    // invocation on another thread, so a listener's owner may free the
    // captured state right after offAndWait() returns — without this, a
    // handler mid-invocation on the emit thread dereferences freed
    // memory (seen as ListeningRpcCommunicator crashes when a transport
    // Message raced the communicator's destroy()).
    std::map<size_t, std::vector<std::thread::id>> mInFlightInvocations;
    std::condition_variable mInvocationFinished;

    // Call under mExecutingEmitLoopHandlersMutex.
    [[nodiscard]] bool hasInFlightInvocationOnOtherThread(
        size_t handlerId) const {
        const auto it = mInFlightInvocations.find(handlerId);
        if (it == mInFlightInvocations.end()) {
            return false;
        }
        return std::ranges::any_of(
            it->second, [](const std::thread::id& threadId) {
                return threadId != std::this_thread::get_id();
            });
    }

    // Call under mExecutingEmitLoopHandlersMutex.
    [[nodiscard]] bool hasAnyInFlightInvocationOnOtherThread() const {
        return std::ranges::any_of(mInFlightInvocations, [](const auto& entry) {
            return std::ranges::any_of(
                entry.second, [](const std::thread::id& threadId) {
                    return threadId != std::this_thread::get_id();
                });
        });
    }

    void beginInvocation(size_t handlerId) {
        std::lock_guard guard{mExecutingEmitLoopHandlersMutex};
        mInFlightInvocations[handlerId].push_back(std::this_thread::get_id());
    }

    void endInvocation(size_t handlerId) {
        {
            std::lock_guard guard{mExecutingEmitLoopHandlersMutex};
            const auto it = mInFlightInvocations.find(handlerId);
            if (it != mInFlightInvocations.end()) {
                auto& threads = it->second;
                const auto threadIt =
                    std::ranges::find(threads, std::this_thread::get_id());
                if (threadIt != threads.end()) {
                    threads.erase(threadIt);
                }
                if (threads.empty()) {
                    mInFlightInvocations.erase(it);
                }
            }
        }
        mInvocationFinished.notify_all();
    }

    std::optional<StoredEvent<typename EmitterEventType::ArgumentTypes>>
        mLatestEvent;
    std::mutex mLatestEventMutex;

    void invokeLatestEvent(
        typename EmitterEventType::Handler& handler,
        typename EmitterEventType::ArgumentTypes args) {
        std::apply(handler, std::move(args));
    }

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

        // Marked in-flight atomically with the pop, so off() sees the
        // handler either in the pending snapshot or in-flight — never
        // in the gap between the two.
        mInFlightInvocations[handler.getId()].push_back(
            std::this_thread::get_id());

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
        typename EmitterEventType::Handler::HandlerFunction handlerFunction =
            callback;
        // silently ignore null callbacks
        if (!handlerFunction) {
            return HandlerToken{}; // return a non-existent token
        }

        auto handlerReference = HandlerToken::create();
        typename EmitterEventType::Handler handler(
            handlerFunction, handlerReference.getId(), once);

        if constexpr (ReplayLatestEventToNewListeners) {
            // The replay-check and the registration are one atomic step
            // under mEventHandlersMutex, matching emit()'s latest-store +
            // snapshot (also under mEventHandlersMutex): a listener that
            // registers concurrently with an emit either lands in that
            // emit's snapshot (live dispatch, and it observed no stored
            // event here) or observes the stored event and replays it
            // (and is absent from that snapshot) — never neither, never
            // both.
            //
            // The replayed handler is invoked AFTER the mutex is
            // released. It is user code that may take its own locks and
            // call back into on()/off(); invoking it under
            // mEventHandlersMutex is an ABBA deadlock against a thread
            // that holds such a user lock and calls off() (which needs
            // mEventHandlersMutex). This is deliberately NOT serialized
            // with emit() through mEmitLoopMutex: taking mEmitLoopMutex
            // here would self-deadlock when a handler runs on()/once()
            // during an emit on the same thread. The cost is that, under
            // a concurrent emit, a brand-new listener could observe a
            // newer live event before its replay of the older stored one;
            // the ReplayEventEmitter is only used for terminal one-shot
            // events (a pending connection's single Connected xor
            // Disconnected), where no such ordering arises.
            std::optional<typename EmitterEventType::ArgumentTypes>
                replayArguments;
            {
                std::lock_guard guard{mEventHandlersMutex};
                {
                    std::lock_guard latestGuard{mLatestEventMutex};
                    if (mLatestEvent.has_value()) {
                        replayArguments = mLatestEvent.value().getArguments();
                    }
                }
                // a once-listener satisfied by the replay is never
                // registered at all
                if (!(once && replayArguments.has_value())) {
                    mEventHandlers.push_back(handler);
                }
            }
            if (replayArguments.has_value()) {
                this->beginInvocation(handler.getId());
                try {
                    detail::InvocationDepthGuard depthGuard;
                    invokeLatestEvent(
                        handler, std::move(replayArguments.value()));
                } catch (...) {
                    this->endInvocation(handler.getId());
                    throw;
                }
                this->endInvocation(handler.getId());
                if (once) {
                    return HandlerToken{};
                }
            }
            return handlerReference;
        }

        std::lock_guard guard{mEventHandlersMutex};
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
        {
            std::lock_guard guard{mEventHandlersMutex};
            mEventHandlers.remove_if(
                [&handlerReference](
                    const typename EmitterEventType::Handler& handler) {
                    return handler.getId() == handlerReference.getId();
                });
        }
        // Also remove the pending copy an executing emit loop may still
        // hold. NOTE: an invocation of this handler that already STARTED
        // on another thread may still be running when off() returns; an
        // owner that frees the handler's captures right after off() must
        // use offAndWait() instead.
        this->removeHandlerFromExecutingEmitLoops(handlerReference);
    }

    /**
     * @brief Remove an event listener and wait until it is no longer
     * running, so the caller may free the listener's captured state
     * immediately afterwards.
     *
     * @details Waits out an invocation already running on ANOTHER
     * thread. The wait is skipped when called from inside any event
     * handler invocation (on any emitter): handlers legitimately remove
     * each other's listeners from handler context, and blocking there
     * deadlocks — two threads, each inside a handler the other wants
     * removed, would wait on each other forever (seen between a
     * connection's Data and Disconnected handlers in the Layer1 scale
     * tests). The caller must also not hold any lock that this event's
     * handlers can take, or the wait deadlocks on that lock (seen
     * between Endpoint's state mutex and a PendingConnection Connected
     * handler).
     *
     * @tparam EventType The type of the event to remove the listener from.
     * @param handlerReference The token of the listener to remove.
     */

    template <MatchingEventType<EmitterEventType> EventType>
    void offAndWait(HandlerToken handlerReference) {
        {
            std::lock_guard guard{mEventHandlersMutex};
            mEventHandlers.remove_if(
                [&handlerReference](
                    const typename EmitterEventType::Handler& handler) {
                    return handler.getId() == handlerReference.getId();
                });
        }
        // Remove the pending snapshot copy and wait out an invocation
        // already running on another thread. NB: the wait must not hold
        // mEventHandlersMutex: the running handler may need it
        // (once-removal, on()/off() reentry).
        std::unique_lock lock{mExecutingEmitLoopHandlersMutex};
        mExecutingEmitLoopHandlers.erase(handlerReference.getId());
        // Wait only from non-handler context (see the method comment and
        // detail::handlerInvocationDepth).
        if (detail::handlerInvocationDepth == 0) {
            mInvocationFinished.wait(lock, [this, &handlerReference]() {
                return !this->hasInFlightInvocationOnOtherThread(
                    handlerReference.getId());
            });
        }
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
     * @brief Remove all listeners for an event type and wait until none
     * of them is still running — same guarantee and same caller
     * constraints as offAndWait(), for every handler at once.
     *
     * @tparam EventType The type of the event to remove all listeners for.
     */

    template <MatchingEventType<EmitterEventType> EventType>
    void removeAllListenersAndWait() {
        {
            std::lock_guard guard{mEventHandlersMutex};
            mEventHandlers.clear();
        }
        // Also cancel invocations an in-progress emit loop has not started
        // yet — after this method returns, no handler may start.
        std::unique_lock lock{mExecutingEmitLoopHandlersMutex};
        mExecutingEmitLoopHandlers.clear();
        // Wait only from non-handler context — same deadlock-avoidance
        // rule as offAndWait().
        if (detail::handlerInvocationDepth == 0) {
            mInvocationFinished.wait(lock, [this]() {
                return !this->hasAnyInFlightInvocationOnOtherThread();
            });
        }
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
    void emit(EventArgs... args) {
        std::lock_guard guard{mEmitLoopMutex};
        {
            // The latest-event store and the handler snapshot must form one
            // atomic step with respect to on(): on() checks mLatestEvent and
            // registers the handler while holding mEventHandlersMutex, so a
            // listener registered concurrently with this emit either makes
            // it into the snapshot (live dispatch) or observes the stored
            // event (replay) — never neither, never both. mEventHandlersMutex
            // is only ever taken inside mEmitLoopMutex here, matching the
            // once-handler removal below, so no new lock order is introduced.
            std::lock_guard handlersGuard{mEventHandlersMutex};
            if constexpr (ReplayLatestEventToNewListeners) {
                StoredEvent<typename EmitterEventType::ArgumentTypes>
                storedEvent((args)...);
                std::lock_guard latestGuard{mLatestEventMutex};
                mLatestEvent = std::move(storedEvent);
            }
            createExecutingEmitLoopHandlersMap();
        }

        while (auto ret = popHandlerFromExecutingEmitLoopHandlersMap()) {
            auto handler = ret.value();
            // Pass the arguments as lvalues, NOT std::forward: every
            // handler must receive its own copy. Forwarding here moved
            // the arguments into the first handler's by-value parameters,
            // leaving the second and later handlers with moved-from
            // values (an empty vector for a Data<std::vector<std::byte>>
            // event, for instance). The per-handler copy is the intended
            // fan-out semantics; Handler::operator() then moves the copy
            // on into the stored std::function.
            try {
                detail::InvocationDepthGuard depthGuard;
                std::invoke(handler, args...);
            } catch (...) {
                this->endInvocation(handler.getId());
                throw;
            }
            this->endInvocation(handler.getId());
            if (handler.isOnce()) {
                std::lock_guard guard{mEventHandlersMutex};
                mEventHandlers.remove(handler);
            }
        }
    }
};

template <typename BoundEmitterType, typename BoundEventType>
class BoundEvent {
public:
    using EventType = BoundEventType;
    using EmitterType = BoundEmitterType;

private:
    EmitterType& mEventEmitter;

public:
    explicit BoundEvent(EmitterType& eventEmitter)
        : mEventEmitter(eventEmitter) {}

    [[nodiscard]] EmitterType& getEmitter() const { return mEventEmitter; }
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
    EventEmitter() = default;
    virtual ~EventEmitter() = default;

    // Make the inherited methods visible to the templating system

    using EventEmitterImpl<EventTypes>::on...;
    using EventEmitterImpl<EventTypes>::once...;
    using EventEmitterImpl<EventTypes>::off...;
    using EventEmitterImpl<EventTypes>::offAndWait...;
    using EventEmitterImpl<EventTypes>::listenerCount...;
    using EventEmitterImpl<EventTypes>::removeAllListeners...;
    using EventEmitterImpl<EventTypes>::removeAllListenersAndWait...;
    using EventEmitterImpl<EventTypes>::emit...;

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
        (EventEmitterImpl<EventTypes>::template removeAllListeners<
             EventTypes>(),
         ...);
    }

    /**
     * @brief Remove all listeners for all event types and wait until none
     * of them is still running (see
     * EventEmitterImpl::removeAllListenersAndWait for the caller
     * constraints).
     *
     */

    template <typename T = std::nullopt_t>
    void removeAllListenersAndWait() {
        (EventEmitterImpl<EventTypes>::template removeAllListenersAndWait<
             EventTypes>(),
         ...);
    }
};

// ReplayEventEmitter is an EventEmitter that replays the latest event to new
// listeners the same way as ReplaySubject in Rx.

template <typename... EventTypes>
struct ReplayEventEmitter;

template <typename... EventTypes>
class ReplayEventEmitter<std::tuple<EventTypes...>>
    : public EventEmitterImpl<EventTypes, true>... { // inherit from
                                                     // EventEmitterImpl
public:
    ReplayEventEmitter() = default;
    virtual ~ReplayEventEmitter() = default;

    // Make the inherited methods visible to the templating system

    using EventEmitterImpl<EventTypes, true>::on...;
    using EventEmitterImpl<EventTypes, true>::once...;
    using EventEmitterImpl<EventTypes, true>::off...;
    using EventEmitterImpl<EventTypes, true>::offAndWait...;
    using EventEmitterImpl<EventTypes, true>::listenerCount...;
    using EventEmitterImpl<EventTypes, true>::removeAllListeners...;
    using EventEmitterImpl<EventTypes, true>::removeAllListenersAndWait...;
    using EventEmitterImpl<EventTypes, true>::emit...;

    template <typename EventType>
    [[nodiscard]] BoundEvent<
        ReplayEventEmitter<std::tuple<EventTypes...>>,
        EventType>
    event() {
        return BoundEvent<
            ReplayEventEmitter<std::tuple<EventTypes...>>,
            EventType>(*this);
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

    /**
     * @brief Remove all listeners for all event types and wait until none
     * of them is still running (see
     * EventEmitterImpl::removeAllListenersAndWait for the caller
     * constraints).
     *
     */

    template <typename T = std::nullopt_t>
    void removeAllListenersAndWait() {
        (EventEmitterImpl<EventTypes, true>::template removeAllListenersAndWait<
             EventTypes>(),
         ...);
    }
};

} // namespace streamr::eventemitter
