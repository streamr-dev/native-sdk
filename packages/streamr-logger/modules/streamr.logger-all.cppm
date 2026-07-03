// Coarse façade partition over ALL public headers of streamr-logger.
// One partition (instead of one per header) keeps the number of
// module units — and thus repeated global-module-fragment parses
// of the expensive header stacks — minimal during the façade
// stage (measured at the Phase 2.4 bench checkpoint). Per-header
// granularity returns at consolidation if needed.
module;

#include "streamr-logger/Logger.hpp"
#include "streamr-logger/LoggerImpl.hpp"
#include "streamr-logger/SLogger.hpp"
#include "streamr-logger/StreamrLogLevel.hpp"

export module streamr.logger:all;

export namespace streamr::logger {

using streamr::logger::envLogLevelName;
using streamr::logger::Logger;

} // namespace streamr::logger

export namespace streamr::logger {

using streamr::logger::LoggerImpl;

} // namespace streamr::logger

export namespace streamr::logger {

using streamr::logger::SLogger;

} // namespace streamr::logger

export namespace streamr::logger::streamrloglevel {

using streamr::logger::streamrloglevel::Debug;
using streamr::logger::streamrloglevel::Error;
using streamr::logger::streamrloglevel::Fatal;
using streamr::logger::streamrloglevel::Info;
using streamr::logger::streamrloglevel::Trace;
using streamr::logger::streamrloglevel::Warn;

} // namespace streamr::logger::streamrloglevel

export namespace streamr::logger {

using streamr::logger::getStreamrLogLevelByName;
using streamr::logger::getStreamrLogLevelValue;
using streamr::logger::LevelGetter;
using streamr::logger::StreamrLogLevel;
using streamr::logger::StreamrLogLevelConcept;
using streamr::logger::systemDefaultLogLevel;

} // namespace streamr::logger
