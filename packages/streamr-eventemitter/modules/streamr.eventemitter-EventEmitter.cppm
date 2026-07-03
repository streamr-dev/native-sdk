// Façade partition over streamr-eventemitter/EventEmitter.hpp. The header
// is included in the global module fragment — its entities stay attached to
// the global module, so `import` and `#include` consumers can be mixed
// ODR-safely during the migration — and the public names are re-exported.
module;

#include "streamr-eventemitter/EventEmitter.hpp"

export module streamr.eventemitter:EventEmitter;

export namespace streamr::eventemitter {

using streamr::eventemitter::BoundEvent;
using streamr::eventemitter::Event;
using streamr::eventemitter::EventEmitter;
using streamr::eventemitter::EventEmitterImpl;
using streamr::eventemitter::HandlerToken;
using streamr::eventemitter::MatchingCallbackType;
using streamr::eventemitter::MatchingEventType;
using streamr::eventemitter::ReplayEventEmitter;
using streamr::eventemitter::StoredEvent;

} // namespace streamr::eventemitter
