// Module streamr.logger.StreamrHandlerFactory
// CONSOLIDATED from the former header
// streamr-logger/detail/StreamrHandlerFactory.hpp (MODERNIZATION.md Phase 2.6):
// this file is now the source of truth.
module;
#include <new>

#include <folly/logging/StandardLogHandler.h>
#include <folly/logging/StandardLogHandlerFactory.h>
#include <folly/logging/StreamHandlerFactory.h>

export module streamr.logger.StreamrHandlerFactory;

import std;

import streamr.logger.StreamrLogFormatterFactory;
import streamr.logger.StreamrWriterFactory;

export namespace streamr::logger::detail {

class StreamrHandlerFactory : public folly::StreamHandlerFactory {
private:
    StreamrWriterFactory* mWriterFactory;
    StreamrLogFormatterFactory formatterFactory;

public:
    StreamrHandlerFactory() = default;
    ~StreamrHandlerFactory() override = default;
    explicit StreamrHandlerFactory(StreamrWriterFactory* writerFactory)
        : mWriterFactory{writerFactory} {}

    std::shared_ptr<folly::LogHandler> createHandler(
        const Options& options) override {
        return folly::StandardLogHandlerFactory::createHandler(
            getType(), mWriterFactory, &formatterFactory, options);
    }
};

} // namespace streamr::logger::detail