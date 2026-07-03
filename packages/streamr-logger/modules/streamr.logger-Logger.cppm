// Façade partition over streamr-logger/Logger.hpp (see the streamr.eventemitter
// partitions for the pattern rationale).
module;

#include "streamr-logger/Logger.hpp"

export module streamr.logger:Logger;

export namespace streamr::logger {

using streamr::logger::envLogLevelName;
using streamr::logger::Logger;

} // namespace streamr::logger
