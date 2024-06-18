#ifndef STREAMER_LOGGER_STREAMRWRITERFACTORY_HPP
#define STREAMER_LOGGER_STREAMRWRITERFACTORY_HPP

#include <folly/logging/StreamHandlerFactory.h>

namespace streamr::logger {

class StreamrWriterFactory : public folly::StreamHandlerFactory::WriterFactory {
private:
    std::shared_ptr<folly::LogWriter> logWriter_ = nullptr;

public:
    StreamrWriterFactory() : logWriter_{nullptr} {}

    explicit StreamrWriterFactory(std::shared_ptr<folly::LogWriter> logWriter)
        : logWriter_{std::move(logWriter)} {}

    std::shared_ptr<folly::LogWriter> createWriter() override {
        if (logWriter_) {
            return logWriter_;
        }
        return folly::StreamHandlerFactory::WriterFactory::createWriter();
    }
};

}; // namespace streamr::logger

#endif