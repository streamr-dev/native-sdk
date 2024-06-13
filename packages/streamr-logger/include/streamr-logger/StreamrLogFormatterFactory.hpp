#ifndef STREAMER_LOGGER_STANDARDLOGHANDLERFACTORY_H
#define STREAMER_LOGGER_STANDARDLOGHANDLERFACTORY_H

#include <folly/logging/StandardLogHandlerFactory.h>
#include "StreamrLogFormatter.hpp"

namespace streamr::logger {

class StreamrLogFormatterFactory
    : public folly::StandardLogHandlerFactory::FormatterFactory {
   public:
    bool processOption(
        folly::StringPiece name, folly::StringPiece value) override {
        return false;
    }

    std::shared_ptr<folly::LogFormatter> createFormatter(
        const std::shared_ptr<folly::LogWriter>&) override {
        return std::make_shared<StreamrLogFormatter>();
    }
};
};

#endif