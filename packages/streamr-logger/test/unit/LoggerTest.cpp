#include <string>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <streamr-logger/Logger.hpp>
#include <streamr-logger/StreamrLogFormatter.hpp>
#include <folly/logging/LogCategory.h>
#include <folly/logging/LogMessage.h>

using streamr::logger::Logger;
// using streamr::logger::StreamrHandlerFactory;
using streamr::logger::StreamrLogFormatter;
using streamr::logger::detail::StreamrLogLevel;
using namespace std::chrono; // NOLINT

struct LogWriterMock : public folly::LogWriter {
public:
    std::string buffer; // NOLINT
    int isCalled; // NOLINT

    LogWriterMock() : isCalled{0} {}

    void writeMessage(folly::StringPiece buf, uint32_t /* flags */) override {
        std::cout << buf.toString();
        buffer = buf.toString();
        isCalled = 1;
    }
    bool ttyOutput() const override { return true; } // NOLINT
    void flush() override {}
};

class StreamrLogFormatterTest : public testing::Test {
    // using streamr::logger::StreamrLogFormatter;
    void SetUp() override {
        const auto ymd2 =
            std::chrono::year_month_day(2024y, std::chrono::January, 31d);
        tp = std::chrono::sys_days{ymd2};
    }

protected:
    const unsigned int lineNumber2{100}; // NOLINT
    const unsigned int lineNumber{101010}; // NOLINT
    StreamrLogFormatter formatter_; // NOLINT
    std::chrono::system_clock::time_point tp; // NOLINT
};

class LoggerTest : public testing::Test {
protected:
    std::shared_ptr<LogWriterMock> logWriterMock =
        std::make_shared<LogWriterMock>();
    Logger logger = Logger(StreamrLogLevel::INFO, "", logWriterMock);
};

TEST_F(StreamrLogFormatterTest, traceNoTruncate) {
    StreamrLogFormatter::StreamrLogMessage msg = {
        tp, "Filename.cpp", lineNumber2, folly::LogLevel::DBG, "Message"};
    EXPECT_EQ(
        formatter_.formatMessageInStreamrStyle(msg),
        "\x1B[90mTRACE\x1B[0m [2024-01-31T02:00:00.0] (Filename.cpp: 100                   ): \x1B[36mMessage\x1B[0m\n");
}

TEST_F(StreamrLogFormatterTest, traceTruncate) {
    StreamrLogFormatter::StreamrLogMessage msg = {
        tp,
        "1234567890123456789012345678901234567890.cpp",
        lineNumber2,
        folly::LogLevel::DBG,
        "Message"};
    EXPECT_EQ(
        formatter_.formatMessageInStreamrStyle(msg),
        "\x1B[90mTRACE\x1B[0m [2024-01-31T02:00:00.0] (1234567890123456789012345678901: 100): \x1B[36mMessage\x1B[0m\n");
}

TEST_F(StreamrLogFormatterTest, debugNoTruncate) {
    StreamrLogFormatter::StreamrLogMessage msg = {
        tp, "Filename.cpp", lineNumber, folly::LogLevel::DBG0, "Message"};
    EXPECT_EQ(
        formatter_.formatMessageInStreamrStyle(msg),
        "\x1B[34mDEBUG\x1B[0m [2024-01-31T02:00:00.0] (Filename.cpp: 101010                ): \x1B[36mMessage\x1B[0m\n");
}

TEST_F(StreamrLogFormatterTest, infoNoTruncate) {
    StreamrLogFormatter::StreamrLogMessage msg = {
        tp, "Filename.cpp", lineNumber, folly::LogLevel::INFO, "Message"};
    EXPECT_EQ(
        formatter_.formatMessageInStreamrStyle(msg),
        "\x1B[32mINFO\x1B[0m [2024-01-31T02:00:00.0] (Filename.cpp: 101010                ): \x1B[36mMessage\x1B[0m\n");
}

TEST_F(StreamrLogFormatterTest, warnoNoTruncate) {
    StreamrLogFormatter::StreamrLogMessage msg = {
        tp, "Filename.cpp", lineNumber, folly::LogLevel::WARN, "Message"};
    EXPECT_EQ(
        formatter_.formatMessageInStreamrStyle(msg),
        "\x1B[33mWARN\x1B[0m [2024-01-31T02:00:00.0] (Filename.cpp: 101010                ): \x1B[36mMessage\x1B[0m\n");
}

TEST_F(StreamrLogFormatterTest, errorNoTruncate) {
    StreamrLogFormatter::StreamrLogMessage msg = {
        tp, "Filename.cpp", lineNumber, folly::LogLevel::ERR, "Message"};
    EXPECT_EQ(
        formatter_.formatMessageInStreamrStyle(msg),
        "\x1B[31mERROR\x1B[0m [2024-01-31T02:00:00.0] (Filename.cpp: 101010                ): \x1B[36mMessage\x1B[0m\n");
}

TEST_F(StreamrLogFormatterTest, fatalNoTruncate) {
    // Cannot use FATAL in Folly because it aborts, CRITICAL is converted to
    // FATAL
    StreamrLogFormatter::StreamrLogMessage msg = {
        tp, "Filename.cpp", lineNumber, folly::LogLevel::CRITICAL, "Message"};
    EXPECT_EQ(
        formatter_.formatMessageInStreamrStyle(msg),
        "\x1B[1;41mFATAL\x1B[0m [2024-01-31T02:00:00.0] (Filename.cpp: 101010                ): \x1B[36mMessage\x1B[0m\n");
}

TEST_F(LoggerTest, NoLogLevelEnvVariableSetInfoLogSent) {
    unsetenv("LOG_LEVEL");

    logger.info("Testi");
    // Log message written with default info
    EXPECT_EQ(logWriterMock->isCalled, 1);
}

TEST_F(LoggerTest, NoLogLevelEnvVariableSetDebugLogNotSent) {
    unsetenv("LOG_LEVEL");

    logger.info("Testi");
    // Log message not written because default is info
    EXPECT_EQ(logWriterMock->isCalled, 1);
}

TEST_F(LoggerTest, LogLevelSetWronglyInEnvVariable) {
    unsetenv("NotFound");

    logger.info("Testi");
    // Info log message written
    EXPECT_EQ(logWriterMock->isCalled, 1);
}

// Env variable log level set to trace

TEST_F(LoggerTest, LogLevelEnvVariableSetToTraceWithTraceLogMsg) {
    setenv("LOG_LEVEL", "trace", 1);

    logger.trace("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 1);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToTraceWithDebugLogMsg) {
    setenv("LOG_LEVEL", "trace", 1);

    logger.debug("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 1);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToTraceWithInfoLogMsg) {
    setenv("LOG_LEVEL", "trace", 1);

    logger.info("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 1);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToTracelWithWarnLogMsg) {
    setenv("LOG_LEVEL", "trace", 1);

    logger.warn("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 1);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToTraceWithErrorLogMsg) {
    setenv("LOG_LEVEL", "trace", 1);

    logger.error("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 1);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToTraceWithFatalLogMsg) {
    setenv("LOG_LEVEL", "trace", 1);

    logger.fatal("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 1);
}

// Env variable log level set to debug

TEST_F(LoggerTest, LogLevelEnvVariableSetToDebugWithTraceLogMsg) {
    setenv("LOG_LEVEL", "debug", 1);

    logger.trace("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 0);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToDebugWithDebugLogMsg) {
    setenv("LOG_LEVEL", "debug", 1);

    logger.debug("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 1);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToDebugWithInfoLogMsg) {
    setenv("LOG_LEVEL", "debug", 1);

    logger.info("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 1);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToDebugWithWarnLogMsg) {
    setenv("LOG_LEVEL", "debug", 1);

    logger.warn("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 1);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToDebugWithErrorLogMsg) {
    setenv("LOG_LEVEL", "debug", 1);

    logger.error("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 1);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToDebugWithFatalLogMsg) {
    setenv("LOG_LEVEL", "debug", 1);

    logger.fatal("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 1);
}

// Env variable log level set to info

TEST_F(LoggerTest, LogLevelEnvVariableSetToInfoWithTraceLogMsg) {
    setenv("LOG_LEVEL", "info", 1);

    logger.trace("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 0);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToInfoWithDebugLogMsg) {
    setenv("LOG_LEVEL", "info", 1);

    logger.debug("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 0);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToInfoWithInfoLogMsg) {
    setenv("LOG_LEVEL", "info", 1);

    logger.info("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 1);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToInfoWithWarnLogMsg) {
    setenv("LOG_LEVEL", "info", 1);

    logger.warn("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 1);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToInfoWithErrorLogMsg) {
    setenv("LOG_LEVEL", "info", 1);

    logger.error("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 1);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToInfoWithFatalLogMsg) {
    setenv("LOG_LEVEL", "info", 1);

    logger.fatal("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 1);
}

// Env variable log level set to warn

TEST_F(LoggerTest, LogLevelEnvVariableSetToWarnWithTraceLogMsg) {
    setenv("LOG_LEVEL", "warn", 1);

    logger.trace("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 0);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToWarnWithDebugLogMsg) {
    setenv("LOG_LEVEL", "warn", 1);

    logger.debug("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 0);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToWarnWithInfoLogMsg) {
    setenv("LOG_LEVEL", "warn", 1);

    logger.info("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 0);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToWarnWithWarnLogMsg) {
    setenv("LOG_LEVEL", "warn", 1);

    logger.warn("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 1);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToWarnWithErrorLogMsg) {
    setenv("LOG_LEVEL", "warn", 1);

    logger.error("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 1);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToWarnWithFatalLogMsg) {
    setenv("LOG_LEVEL", "warn", 1);

    logger.fatal("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 1);
}

// Env variable log level set to error

TEST_F(LoggerTest, LogLevelEnvVariableSetErrorWithTraceLogMsg) {
    setenv("LOG_LEVEL", "error", 1);

    logger.trace("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 0);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToErrorWithDebugLogMsg) {
    setenv("LOG_LEVEL", "error", 1);

    logger.debug("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 0);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToErrorWithInfoLogMsg) {
    setenv("LOG_LEVEL", "error", 1);

    logger.info("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 0);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToErrorWithWarnLogMsg) {
    setenv("LOG_LEVEL", "error", 1);

    logger.warn("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 0);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToErrorWithErrorLogMsg) {
    setenv("LOG_LEVEL", "error", 1);

    logger.error("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 1);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToErrorWithFatalLogMsg) {
    setenv("LOG_LEVEL", "error", 1);

    logger.fatal("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 1);
}

// Env variable log level set to fatal

TEST_F(LoggerTest, LogLevelEnvVariableSetFatalWithTraceLogMsg) {
    setenv("LOG_LEVEL", "fatal", 1);

    logger.trace("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 0);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToFatalWithDebugLogMsg) {
    setenv("LOG_LEVEL", "fatal", 1);

    logger.debug("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 0);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToFatalWithInfoLogMsg) {
    setenv("LOG_LEVEL", "fatal", 1);

    logger.info("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 0);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToFatalWithWarnLogMsg) {
    setenv("LOG_LEVEL", "fatal", 1);

    logger.warn("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 0);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToFatalWithErrorLogMsg) {
    setenv("LOG_LEVEL", "fatal", 1);

    logger.error("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 0);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToFatalWithFatalLogMsg) {
    setenv("LOG_LEVEL", "fatal", 1);

    logger.fatal("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 1);
}

TEST_F(LoggerTest, TraceLogWithExtraArgumentText) {
    setenv("LOG_LEVEL", "trace", 1);

    logger.trace("Testi", "LogExtraArgumentText");

    EXPECT_THAT(logWriterMock->buffer, testing::HasSubstr("TRACE"));
    EXPECT_THAT(
        logWriterMock->buffer,
        testing::HasSubstr("Testi LogExtraArgumentText"));
}

TEST_F(LoggerTest, DebugLogWithExtraArgumentText) {
    setenv("LOG_LEVEL", "debug", 1);

    logger.debug("Testi", "LogExtraArgumentText");

    EXPECT_THAT(logWriterMock->buffer, testing::HasSubstr("DEBUG"));
    EXPECT_THAT(
        logWriterMock->buffer,
        testing::HasSubstr("Testi LogExtraArgumentText"));
}

TEST_F(LoggerTest, InfoLogWithExtraArgumentText) {
    setenv("LOG_LEVEL", "info", 1);

    logger.info("Testi", "LogExtraArgumentText");

    EXPECT_THAT(logWriterMock->buffer, testing::HasSubstr("INFO"));
    EXPECT_THAT(
        logWriterMock->buffer,
        testing::HasSubstr("Testi LogExtraArgumentText"));
}

TEST_F(LoggerTest, WarnLogWithExtraArgumentText) {
    setenv("LOG_LEVEL", "warn", 1);

    logger.warn("Testi", "LogExtraArgumentText");

    EXPECT_THAT(logWriterMock->buffer, testing::HasSubstr("WARN"));
    EXPECT_THAT(
        logWriterMock->buffer,
        testing::HasSubstr("Testi LogExtraArgumentText"));
}

TEST_F(LoggerTest, ErrorLogWithExtraArgumentText) {
    setenv("LOG_LEVEL", "error", 1);

    logger.error("Testi", "LogExtraArgumentText");

    EXPECT_THAT(logWriterMock->buffer, testing::HasSubstr("ERROR"));
    EXPECT_THAT(
        logWriterMock->buffer,
        testing::HasSubstr("Testi LogExtraArgumentText"));
}

TEST_F(LoggerTest, FatalLogWithExtraArgumentText) {
    setenv("LOG_LEVEL", "fatal", 1);

    logger.fatal("Testi", "LogExtraArgumentText");

    EXPECT_THAT(logWriterMock->buffer, testing::HasSubstr("FATAL"));
    EXPECT_THAT(
        logWriterMock->buffer,
        testing::HasSubstr("Testi LogExtraArgumentText"));
}

TEST(LoggerContextBindingText, FatalLogWithExtraArgumentTextAndContextBinding) {
    setenv("LOG_LEVEL", "fatal", 1);
    std::shared_ptr<LogWriterMock> tmpLogWriterMock =
        std::make_shared<LogWriterMock>();
    Logger tmpLogger =
        Logger(StreamrLogLevel::INFO, "ContextBindingText", tmpLogWriterMock);

    tmpLogger.fatal("Testi", "LogExtraArgumentText");

    EXPECT_THAT(tmpLogWriterMock->buffer, testing::HasSubstr("FATAL"));
    EXPECT_THAT(
        tmpLogWriterMock->buffer,
        testing::HasSubstr("Testi LogExtraArgumentText ContextBindingText"));
}
