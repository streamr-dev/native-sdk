#ifndef STREAMER_LOGGER_STREAMRWRITERFACTORY_H
#define STREAMER_LOGGER_STREAMRWRITERFACTORY_H

#include <folly/logging/StreamHandlerFactory.h>

namespace streamr::logger {

class StreamrWriterFactory : public folly::StreamHandlerFactory::WriterFactory {
   public:
    StreamrWriterFactory() : logWriter_{nullptr} {}

    StreamrWriterFactory(std::shared_ptr<folly::LogWriter> logWriter)
        : logWriter_{logWriter} {}

    std::shared_ptr<folly::LogWriter> createWriter() override {
        if (logWriter_) {
            return logWriter_;
        } else {
            return folly::StreamHandlerFactory::WriterFactory::createWriter();
        }
    }

   private:
    std::shared_ptr<folly::LogWriter> logWriter_ = nullptr;
};

}; // namespace streamr::logger

#endif