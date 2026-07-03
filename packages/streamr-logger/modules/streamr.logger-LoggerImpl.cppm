// Façade partition over streamr-logger/LoggerImpl.hpp (see the
// streamr.eventemitter partitions for the pattern rationale).
module;

#include "streamr-logger/LoggerImpl.hpp"

export module streamr.logger:LoggerImpl;

export namespace streamr::logger {

using streamr::logger::LoggerImpl;

} // namespace streamr::logger
