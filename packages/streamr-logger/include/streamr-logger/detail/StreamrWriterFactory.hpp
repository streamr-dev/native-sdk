#ifndef STREAMER_LOGGER_STREAMRWRITERFACTORY_HPP
#define STREAMER_LOGGER_STREAMRWRITERFACTORY_HPP

#include <folly/logging/StreamHandlerFactory.h>

namespace streamr::logger::detail {

class StreamrWriterFactory : public folly::StreamHandlerFactory::WriterFactory {
private:
    std::shared_ptr<folly::LogWriter> mLogWriter = nullptr;

public:
    StreamrWriterFactory() : mLogWriter{nullptr} {}

    explicit StreamrWriterFactory(std::shared_ptr<folly::LogWriter> logWriter)
        : mLogWriter{std::move(logWriter)} {}

    std::shared_ptr<folly::LogWriter> createWriter() override {
        if (mLogWriter) {
            return mLogWriter;
        }
        return folly::StreamHandlerFactory::WriterFactory::createWriter();
    }
};

} // namespace streamr::logger::detail

#endif