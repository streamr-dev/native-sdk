// Module streamr.logger.StreamrLogFormatterFactory
// CONSOLIDATED from the former header
// streamr-logger/detail/StreamrLogFormatterFactory.hpp (MODERNIZATION.md
// Phase 2.6): this file is now the source of truth.
module;
#include <new>

#include <folly/logging/StandardLogHandlerFactory.h>

export module streamr.logger.StreamrLogFormatterFactory;

import std;

import streamr.logger.StreamrLogFormatter;

export namespace streamr::logger::detail {

class StreamrLogFormatterFactory
    : public folly::StandardLogHandlerFactory::FormatterFactory {
public:
    bool processOption(
        folly::StringPiece /* name */,
        folly::StringPiece /* value */) override {
        return false;
    }

    std::shared_ptr<folly::LogFormatter> createFormatter(
        const std::shared_ptr<folly::LogWriter>& /* logWriter */) override {
        return std::make_shared<StreamrLogFormatter>();
    }
};

} // namespace streamr::logger::detail