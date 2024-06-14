#ifndef STREAMER_LOGGER_STREAMHANDLERFACTORY_H
#define STREAMER_LOGGER_STREAMHANDLERFACTORY_H

#include <folly/logging/StandardLogHandler.h>
#include <folly/logging/StandardLogHandlerFactory.h>
#include <folly/logging/StreamHandlerFactory.h>
#include "StreamrLogFormatterFactory.hpp"
#include "StreamrWriterFactory.hpp"

namespace streamr::logger {

class StreamrHandlerFactory : public folly::StreamHandlerFactory {
public:
    StreamrHandlerFactory() = default;
    ~StreamrHandlerFactory() override = default;
    explicit StreamrHandlerFactory(StreamrWriterFactory* writerFactory)
        : writerFactory_{writerFactory} {}

    std::shared_ptr<folly::LogHandler> createHandler(
        const Options& options) override {
        StreamrLogFormatterFactory formatterFactory;
        return folly::StandardLogHandlerFactory::createHandler(
            getType(), writerFactory_, &formatterFactory, options);
    }

private:
    StreamrWriterFactory* writerFactory_;
};
}; // namespace streamr::logger

#endif