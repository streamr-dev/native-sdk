#include "streamr-logger/Logger.hpp"
#include "streamr-logger/SLogger.hpp"
#include "streamr-logger/StreamrLogLevel.hpp"

using Logger = streamr::logger::Logger;
using SLogger = streamr::logger::SLogger;
namespace streamrloglevel = streamr::logger::streamrloglevel;

class LoggerExample {
private:
    constexpr static int loopCount = 2;
    Logger& mLogger;

    struct MyDataStruct {
        std::string name;
        int value;
    };

public:
    LoggerExample() : mLogger(Logger::instance()) {}

    void doSomething() {
        SLogger::trace("Logging something using the static function: trace");
        SLogger::debug("Logging something using the static function: debug");
        SLogger::info("Logging something using the static function: info");
        SLogger::warn("Logging something using the static function: warn");
        SLogger::error("Logging something using the static function: error");
        SLogger::fatal("Logging something using the static function: fatal");

        mLogger.debug("Logging something using a member reference");

        Logger localLogger("MyLoopLogger", streamrloglevel::Info{});
        for (int i = 0; i < loopCount; i++) {
            localLogger.info(
                "Logging something using logger with context bindings", i);
        }

        Logger localLogger2(
            {{"jsonKey", "jsonValue"}}, streamrloglevel::Info{});
        for (int i = 0; i < loopCount; i++) {
            localLogger2.warn(
                "Logging something using logger with context bindings given as JSON",
                i);
        }

        auto data = MyDataStruct{"count", loopCount};

        Logger localLogger3(data, streamrloglevel::Info{});
        for (int i = 0; i < loopCount; i++) {
            localLogger3.error(
                "Logging something using logger with context bindings given as struct",
                i);
        }

        Logger localLogger4({{"contextData", data}}, streamrloglevel::Info{});
        for (int i = 0; i < loopCount; i++) {
            localLogger4.fatal(
                "Logging something using logger with context bindings given as JSON  that contains a struct",
                i);
        }

        Logger::instance().trace(
            "Logging something using the static instance with metadata given as a struct",
            data);

        mLogger.debug(
            "Logging something using a member reference with metadata given as JSON that contains a struct",
            {{"data", data}});
    }
};

int main() {
    LoggerExample loggerExample;
    loggerExample.doSomething();
    return 0;
}