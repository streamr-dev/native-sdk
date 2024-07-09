#include "streamr-logger/Logger.hpp"
#include "streamr-logger/StreamrLogLevel.hpp"
using Logger = streamr::logger::Logger;
namespace streamrloglevel = streamr::logger::streamrloglevel;
using FollyLoggerImpl = streamr::logger::detail::FollyLoggerImpl;

class LogWriterMock : public folly::LogWriter {
private:
    int mIsCalled{0};

public:
    void writeMessage(folly::StringPiece /* buf */, uint32_t /* flags */) override {
        mIsCalled = 1;
    }

    [[nodiscard]] int getIsCalled() const { return mIsCalled; }
    [[nodiscard]] bool ttyOutput() const override { return true; }

    void flush() override {}
};

class LoggerEnvTest {
private:
    constexpr static int loopCount = 2;
    Logger& mLogger;
    std::shared_ptr<LogWriterMock> mock = std::make_shared<LogWriterMock>();
    std::shared_ptr<FollyLoggerImpl> tmpFollyLoggerImpl =
        std::make_shared<FollyLoggerImpl>(mock);

    struct MyDataStruct {
        std::string name;
        int value;
    };

public:
    LoggerEnvTest() : mLogger(Logger::instance()) {}

    int testInfoLogWritten() {
        Logger logger{
            std::string(""), streamrloglevel::Info{}, tmpFollyLoggerImpl};
        logger.info("Testi");
        if (mock->getIsCalled()) {
            return 1;
        }
        return 0;
    }

    int testFatalLogWritten() {
        Logger logger{
            std::string(""), streamrloglevel::Info{}, tmpFollyLoggerImpl};
        logger.fatal("Testi");
        if (mock->getIsCalled()) {
            return 0;
        } 
        return 1;
    }

    int testErrorLogWritten() {
        Logger logger{
            std::string(""), streamrloglevel::Info{}, tmpFollyLoggerImpl};
        logger.error("Testi");
        if (mock->getIsCalled()) {
            return 1;
        } 
        return 0;
    }
};

int main(int /* argc */, char* argv[]) {
    LoggerEnvTest loggerEnvTest;
    if (argv[1] == std::string("1")) {
        return loggerEnvTest.testInfoLogWritten();
    } 
    if (argv[1] == std::string("2")) {
        return loggerEnvTest.testFatalLogWritten();
    } 
    if (argv[1] == std::string("3")) {
        return loggerEnvTest.testErrorLogWritten();
    }
    return 1;
}
