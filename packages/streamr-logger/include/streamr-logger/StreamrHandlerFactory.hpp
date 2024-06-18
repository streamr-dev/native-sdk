#ifndef STREAMER_LOGGER_STREAMHANDLERFACTORY_HPP
#define STREAMER_LOGGER_STREAMHANDLERFACTORY_HPP

#include <folly/logging/StandardLogHandler.h>
#include <folly/logging/StandardLogHandlerFactory.h>
#include <folly/logging/StreamHandlerFactory.h>
#include "StreamrLogFormatterFactory.hpp"
#include "StreamrWriterFactory.hpp"

namespace streamr::logger {

class StreamrHandlerFactory : public folly::StreamHandlerFactory {
private:
    StreamrWriterFactory* writerFactory_;

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
};
}; // namespace streamr::logger

#endif