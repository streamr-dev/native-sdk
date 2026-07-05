// Module streamr.utils.AbortController
// CONSOLIDATED from the former header
// streamr-utils/AbortController.hpp (MODERNIZATION.md Phase 2.6):
// this file is now the source of truth.
module;

#include <string>
#include <string_view>
#include <tuple>
#include <folly/CancellationToken.h>

export module streamr.utils.AbortController;

import streamr.eventemitter.EventEmitter;

export namespace streamr::utils {

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
    bool aborted = false; // NOLINT
    std::string reason; // NOLINT
    [[nodiscard]] folly::CancellationToken getCancellationToken() const {
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