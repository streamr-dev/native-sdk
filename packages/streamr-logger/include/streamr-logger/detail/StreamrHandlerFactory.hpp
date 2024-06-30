#ifndef STREAMER_LOGGER_STREAMHANDLERFACTORY_HPP
#define STREAMER_LOGGER_STREAMHANDLERFACTORY_HPP

#include <folly/logging/StandardLogHandler.h>
#include <folly/logging/StandardLogHandlerFactory.h>
#include <folly/logging/StreamHandlerFactory.h>
#include "StreamrLogFormatterFactory.hpp"
#include "StreamrWriterFactory.hpp"

namespace streamr::logger::detail {

class StreamrHandlerFactory : public folly::StreamHandlerFactory {
private:
    StreamrWriterFactory* mWriterFactory;

public:
    StreamrHandlerFactory() = default;
    ~StreamrHandlerFactory() override = default;
    explicit StreamrHandlerFactory(StreamrWriterFactory* writerFactory)
        : mWriterFactory{writerFactory} {}

    std::shared_ptr<folly::LogHandler> createHandler(
        const Options& options) override {
        StreamrLogFormatterFactory formatterFactory;
        return folly::StandardLogHandlerFactory::createHandler(
            getType(), mWriterFactory, &formatterFactory, options);
    }
};

} // namespace streamr::logger::detail

#endif