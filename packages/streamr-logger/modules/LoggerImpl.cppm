// Module streamr.logger.LoggerImpl
// CONSOLIDATED from the former header
// streamr-logger/LoggerImpl.hpp (MODERNIZATION.md Phase 2.6):
// this file is now the source of truth.
module;


export module streamr.logger.LoggerImpl;

import std;

import streamr.logger.StreamrLogLevel;

export namespace streamr::logger {

// Interface for the logger implementations

class LoggerImpl {
public:
    virtual ~LoggerImpl() = default;

    virtual void init(StreamrLogLevel defaultLogLevel) = 0;

    virtual void sendLogMessage(
        StreamrLogLevel logLevel,
        std::string_view msg,
        std::string_view metadata,
        const std::source_location& location) = 0;
};

} // namespace streamr::logger