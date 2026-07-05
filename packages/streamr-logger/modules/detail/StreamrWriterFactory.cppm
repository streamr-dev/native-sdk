// Module streamr.logger.StreamrWriterFactory
// CONSOLIDATED from the former header
// streamr-logger/detail/StreamrWriterFactory.hpp (MODERNIZATION.md Phase 2.6):
// this file is now the source of truth.
module;

#include <memory>
#include <utility>
#include <folly/logging/StreamHandlerFactory.h>

export module streamr.logger.StreamrWriterFactory;

export namespace streamr::logger::detail {

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