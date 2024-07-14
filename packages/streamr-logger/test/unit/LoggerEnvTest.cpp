#include "streamr-logger/Logger.hpp"
#include "streamr-logger/StreamrLogLevel.hpp"
using Logger = streamr::logger::Logger;
namespace streamrloglevel = streamr::logger::streamrloglevel;
using FollyLoggerImpl = streamr::logger::detail::FollyLoggerImpl;

class LogWriterMock : public folly::LogWriter {
private:
    bool mIsCalled{false};

public:
    void writeMessage(
        folly::StringPiece /* buf */, uint32_t /* flags */) override {
        mIsCalled = true;
    }

    [[nodiscard]] bool getIsCalled() const { return mIsCalled; }
    [[nodiscard]] bool ttyOutput() const override { return true; }

    void flush() override {}
};

class LoggerEnvTest {
private:
    std::shared_ptr<LogWriterMock> mock = std::make_shared<LogWriterMock>();
    std::shared_ptr<FollyLoggerImpl> tmpFollyLoggerImpl =
        std::make_shared<FollyLoggerImpl>(mock);

public:
    bool testInfoLogWritten() {
        Logger logger{
            std::string(""), streamrloglevel::Info{}, tmpFollyLoggerImpl};
        logger.info("Testi");
        return mock->getIsCalled();
    }

    bool testFatalLogWritten() {
        Logger logger{
            std::string(""), streamrloglevel::Info{}, tmpFollyLoggerImpl};
        logger.fatal("Testi");
        return mock->getIsCalled();
    }

    bool testErrorLogWritten() {
        Logger logger{
            std::string(""), streamrloglevel::Info{}, tmpFollyLoggerImpl};
        logger.error("Testi");
        return mock->getIsCalled();
    }

    bool testInfoLogWrittenWhenDefaultLogLevelIsWarn() {
        Logger logger{
            std::string(""), streamrloglevel::Warn{}, tmpFollyLoggerImpl};
        logger.info("Testi");
        return mock->getIsCalled();
    }
};

int main(int /* argc */, char* argv[]) {
    LoggerEnvTest loggerEnvTest;
    if (argv[1] == std::string("1")) {
        return static_cast<int>(loggerEnvTest.testInfoLogWritten());
    }
    if (argv[1] == std::string("2")) {
        return static_cast<int>(!loggerEnvTest.testFatalLogWritten());
    }
    if (argv[1] == std::string("3")) {
        return static_cast<int>(loggerEnvTest.testErrorLogWritten());
    }
    if (argv[1] == std::string("4")) {
        return static_cast<int>(
            loggerEnvTest.testInfoLogWrittenWhenDefaultLogLevelIsWarn());
    }
    return 1;
}
