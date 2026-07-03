// Façade partition over streamr-logger/SLogger.hpp (see the
// streamr.eventemitter partitions for the pattern rationale).
module;

#include "streamr-logger/SLogger.hpp"

export module streamr.logger:SLogger;

export namespace streamr::logger {

using streamr::logger::SLogger;

} // namespace streamr::logger
