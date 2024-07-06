#include "streamr-logger/Logger.hpp"
#include "streamr-logger/StreamrLogLevel.hpp"

using Logger = streamr::logger::Logger;
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
        Logger::instance().info("Logging something using the static instance");

        mLogger.info("Logging something using a member reference");

        Logger localLogger("MyLoopLogger", streamrloglevel::Info{});
        for (int i = 0; i < loopCount; i++) {
            localLogger.info(
                "Logging something using logger with context bindings", i);
        }

        Logger localLogger2(
            {{"jsonKey", "jsonValue"}}, streamrloglevel::Info{});
        for (int i = 0; i < loopCount; i++) {
            localLogger2.info(
                "Logging something using logger with context bindings given as JSON",
                i);
        }

        auto data = MyDataStruct{"count", loopCount};
        Logger::instance().info(
            "Logging something using the static instance with metadata given as a struct",
            data);

        mLogger.info(
            "Logging something using a member reference with metadata given as JSON that contains a struct",
            {{"data", data}});
    }
};

int main() {
    LoggerExample loggerExample;
    loggerExample.doSomething();
    return 0;
}