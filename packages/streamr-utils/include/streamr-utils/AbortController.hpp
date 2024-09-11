#ifndef STREAMR_UTILS_ABORT_CONTROLLER_HPP
#define STREAMR_UTILS_ABORT_CONTROLLER_HPP

#include <folly/CancellationToken.h>
#include "streamr-eventemitter/EventEmitter.hpp"

namespace streamr::utils {

using streamr::eventemitter::Event;
using streamr::eventemitter::EventEmitter;

namespace abortsignalevents {
struct Aborted : Event<> {};
} // namespace abortsignalevents

using abortsignalevents::Aborted;
using AbortSignalEvents = std::tuple<Aborted>;

class AbortSignal : public EventEmitter<AbortSignalEvents> {
    friend class AbortController;

public:
    bool aborted; // NOLINT
    std::string reason; // NOLINT
    folly::CancellationToken getCancellationToken() {
        return cancelSource.getToken();
    }

private:
    folly::CancellationSource cancelSource;

    void doAbort() {
        aborted = true;
        this->emit<Aborted>();
        cancelSource.requestCancellation();
    }

    void doAbort(std::string_view reason) {
        this->reason = reason;
        doAbort();
    }
};

class AbortController {
private:
    AbortSignal signal;

public:
    void abort() { signal.doAbort(); }
    void abort(std::string_view reason) { signal.doAbort(reason); }
    AbortSignal& getSignal() { return signal; }
};

} // namespace streamr::utils
#endif // STREAMR_UTILS_ABORT_CONTROLLER_HPP