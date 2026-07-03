// Façade partition over streamr-logger/StreamrLogLevel.hpp (see the
// streamr.eventemitter partitions for the pattern rationale).
module;

#include "streamr-logger/StreamrLogLevel.hpp"

export module streamr.logger:StreamrLogLevel;

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
