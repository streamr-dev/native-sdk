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
    int isCalled; // NOLINT

    LogWriterMock() : isCalled{0} {}

    void writeMessage(folly::StringPiece buf, uint32_t /* flags */) override {
        std::cout << buf.toString();
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

private:
    StreamrLogFormatter formatter_;
    std::chrono::system_clock::time_point tp;
};

class LoggerTest : public testing::Test {
    void SetUp() override {
        logWriterMock = std::make_shared<LogWriterMock>();
        // logger = Logger::get(logWriterMock);
    }

protected:
    Logger& logger{  // NOLINT
        Logger::get(logWriterMock)}; // NOLINT
    std::shared_ptr<LogWriterMock> logWriterMock; // NOLINT
};
/*
TEST_F(StreamrLogFormatterTest, traceNoTruncate) {
     StreamrLogFormatter::StreamrLogMessage msg = {
        tp, "Filename.cpp", 100, folly::LogLevel::DBG, "Message"};
    EXPECT_EQ(
        formatter_.formatMessageInStreamrStyle(msg),
        "\x1B[90mTRACE\x1B[0m [2024-01-31T02:00:00.0] (Filename.cpp: 100 ):
\x1B[36mMessage\x1B[0m\n");
}

TEST_F(StreamrLogFormatterTest, traceTruncate) {
    StreamrLogFormatter::StreamrLogMessage msg = {
        tp,
        "1234567890123456789012345678901234567890.cpp",
        100,
        folly::LogLevel::DBG,
        "Message"};
    EXPECT_EQ(
        formatter_.formatMessageInStreamrStyle(msg),
        "\x1B[90mTRACE\x1B[0m [2024-01-31T02:00:00.0]
(1234567890123456789012345678901: 100): \x1B[36mMessage\x1B[0m\n");
}

TEST_F(StreamrLogFormatterTest, debugNoTruncate) {
    StreamrLogFormatter::StreamrLogMessage msg = {
        tp, "Filename.cpp", 101010, folly::LogLevel::DBG0, "Message"};
    EXPECT_EQ(
        formatter_.formatMessageInStreamrStyle(msg),
        "\x1B[34mDEBUG\x1B[0m [2024-01-31T02:00:00.0] (Filename.cpp: 101010 ):
\x1B[36mMessage\x1B[0m\n");
}

TEST_F(StreamrLogFormatterTest, infoNoTruncate) {
    StreamrLogFormatter::StreamrLogMessage msg = {
        tp, "Filename.cpp", 101010, folly::LogLevel::INFO, "Message"};
    EXPECT_EQ(
        formatter_.formatMessageInStreamrStyle(msg),
        "\x1B[32mINFO\x1B[0m [2024-01-31T02:00:00.0] (Filename.cpp: 101010 ):
\x1B[36mMessage\x1B[0m\n");
}

TEST_F(StreamrLogFormatterTest, warnoNoTruncate) {
    StreamrLogFormatter::StreamrLogMessage msg = {
        tp, "Filename.cpp", 101010, folly::LogLevel::WARN, "Message"};
    EXPECT_EQ(
        formatter_.formatMessageInStreamrStyle(msg),
        "\x1B[33mWARN\x1B[0m [2024-01-31T02:00:00.0] (Filename.cpp: 101010 ):
\x1B[36mMessage\x1B[0m\n");
}

TEST_F(StreamrLogFormatterTest, errorNoTruncate) {
    StreamrLogFormatter::StreamrLogMessage msg = {
        tp, "Filename.cpp", 101010, folly::LogLevel::ERR, "Message"};
    EXPECT_EQ(
        formatter_.formatMessageInStreamrStyle(msg),
        "\x1B[31mERROR\x1B[0m [2024-01-31T02:00:00.0] (Filename.cpp: 101010 ):
\x1B[36mMessage\x1B[0m\n");
}

TEST_F(StreamrLogFormatterTest, fatalNoTruncate) {
    // Cannot use FATAL in Folly because it aborts, CRITICAL is converted to
FATAL StreamrLogFormatter::StreamrLogMessage msg = { tp, "Filename.cpp", 101010,
folly::LogLevel::CRITICAL, "Message"}; EXPECT_EQ(
        formatter_.formatMessageInStreamrStyle(msg),
        "\x1B[1;41mFATAL\x1B[0m [2024-01-31T02:00:00.0] (Filename.cpp: 101010 ):
\x1B[36mMessage\x1B[0m\n");
}

TEST_F(LoggerTest, NoLogLevelEnvVariableSet) {
    unsetenv("LOG_LEVEL");

    SLOG_INFO("Testi");
    // No log message written
    EXPECT_EQ(logWriterMock->isCalled, 0);
}

TEST_F(LoggerTest, LogLevelSetWronglyInEnvVariable) {
    unsetenv("NotFound");

    SLOG_INFO("Testi");
    // No log message written
    EXPECT_EQ(logWriterMock->isCalled, 0);
}

// Env variable log level set to trace

TEST_F(LoggerTest, LogLevelEnvVariableSetToTraceWithTraceLogMsg) {
    setenv("LOG_LEVEL", "trace", 1);

    SLOG_TRACE("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 1);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToTraceWithDebugLogMsg) {
    setenv("LOG_LEVEL", "trace", 1);

    SLOG_DEBUG("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 1);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToTraceWithInfoLogMsg) {
    setenv("LOG_LEVEL", "trace", 1);

    SLOG_INFO("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 1);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToTracelWithWarnLogMsg) {
    setenv("LOG_LEVEL", "trace", 1);

    SLOG_WARN("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 1);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToTraceWithErrorLogMsg) {
    setenv("LOG_LEVEL", "trace", 1);

    SLOG_ERROR("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 1);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToTraceWithFatalLogMsg) {
    setenv("LOG_LEVEL", "trace", 1);

    SLOG_FATAL("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 1);
}

// Env variable log level set to debug

TEST_F(LoggerTest, LogLevelEnvVariableSetToDebugWithTraceLogMsg) {
    setenv("LOG_LEVEL", "debug", 1);

    SLOG_TRACE("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 0);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToDebugWithDebugLogMsg) {
    setenv("LOG_LEVEL", "debug", 1);

    SLOG_DEBUG("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 1);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToDebugWithInfoLogMsg) {
    setenv("LOG_LEVEL", "debug", 1);

    SLOG_INFO("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 1);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToDebugWithWarnLogMsg) {
    setenv("LOG_LEVEL", "debug", 1);

    SLOG_WARN("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 1);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToDebugWithErrorLogMsg) {
    setenv("LOG_LEVEL", "debug", 1);

    SLOG_ERROR("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 1);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToDebugWithFatalLogMsg) {
    setenv("LOG_LEVEL", "debug", 1);

    SLOG_FATAL("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 1);
}

// Env variable log level set to info

TEST_F(LoggerTest, LogLevelEnvVariableSetToInfoWithTraceLogMsg) {
    setenv("LOG_LEVEL", "info", 1);

    SLOG_TRACE("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 0);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToInfoWithDebugLogMsg) {
    setenv("LOG_LEVEL", "info", 1);

    SLOG_DEBUG("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 0);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToInfoWithInfoLogMsg) {
    setenv("LOG_LEVEL", "info", 1);

    SLOG_INFO("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 1);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToInfoWithWarnLogMsg) {
    setenv("LOG_LEVEL", "info", 1);

    SLOG_WARN("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 1);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToInfoWithErrorLogMsg) {
    setenv("LOG_LEVEL", "info", 1);

    SLOG_ERROR("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 1);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToInfoWithFatalLogMsg) {
    setenv("LOG_LEVEL", "info", 1);

    SLOG_FATAL("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 1);
}

// Env variable log level set to warn

TEST_F(LoggerTest, LogLevelEnvVariableSetToWarnWithTraceLogMsg) {
    setenv("LOG_LEVEL", "warn", 1);

    SLOG_TRACE("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 0);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToWarnWithDebugLogMsg) {
    setenv("LOG_LEVEL", "warn", 1);

    SLOG_DEBUG("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 0);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToWarnWithInfoLogMsg) {
    setenv("LOG_LEVEL", "warn", 1);

    SLOG_INFO("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 0);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToWarnWithWarnLogMsg) {
    setenv("LOG_LEVEL", "warn", 1);

    SLOG_WARN("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 1);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToWarnWithErrorLogMsg) {
    setenv("LOG_LEVEL", "warn", 1);

    SLOG_ERROR("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 1);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToWarnWithFatalLogMsg) {
    setenv("LOG_LEVEL", "warn", 1);

    SLOG_FATAL("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 1);
}

// Env variable log level set to error

TEST_F(LoggerTest, LogLevelEnvVariableSetErrorWithTraceLogMsg) {
    setenv("LOG_LEVEL", "error", 1);

    SLOG_TRACE("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 0);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToErrorWithDebugLogMsg) {
    setenv("LOG_LEVEL", "error", 1);

    SLOG_DEBUG("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 0);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToErrorWithInfoLogMsg) {
    setenv("LOG_LEVEL", "error", 1);

    SLOG_INFO("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 0);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToErrorWithWarnLogMsg) {
    setenv("LOG_LEVEL", "error", 1);

    SLOG_WARN("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 0);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToErrorWithErrorLogMsg) {
    setenv("LOG_LEVEL", "error", 1);

    SLOG_ERROR("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 1);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToErrorWithFatalLogMsg) {
    setenv("LOG_LEVEL", "error", 1);

    SLOG_FATAL("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 1);
}

// Env variable log level set to fatal

TEST_F(LoggerTest, LogLevelEnvVariableSetFatalWithTraceLogMsg) {
    setenv("LOG_LEVEL", "fatal", 1);

    SLOG_TRACE("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 0);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToFatalWithDebugLogMsg) {
    setenv("LOG_LEVEL", "fatal", 1);

    SLOG_DEBUG("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 0);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToFatalWithInfoLogMsg) {
    setenv("LOG_LEVEL", "fatal", 1);

    SLOG_INFO("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 0);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToFatalWithWarnLogMsg) {
    setenv("LOG_LEVEL", "fatal", 1);

    SLOG_WARN("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 0);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToFatalWithErrorLogMsg) {
    setenv("LOG_LEVEL", "fatal", 1);

    SLOG_ERROR("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 0);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToFatalWithFatalLogMsg) {
    setenv("LOG_LEVEL", "fatal", 1);

    SLOG_FATAL("Testi");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 1);
}
*/

TEST_F(LoggerTest, LogLevelEnvVariableSetToFatalWithFatalLogMsg) {
    setenv("LOG_LEVEL", "trace", 1);

    logger.warn("TestMessage", "Optionalparam");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 1);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToFatalWithFatalLogMsg2) {
    setenv("LOG_LEVEL", "trace", 1);

    logger.warn("TestMessage");
    // Log written
    EXPECT_EQ(logWriterMock->isCalled, 1);
}


TEST_F(LoggerTest, LogLevelEnvVariableSetToFatalWithFatalLogMsg3) {
    setenv("LOG_LEVEL", "trace", 1);

    auto lgr{Logger(
        StreamrLogLevel::INFO,  "ContextBindingText", logWriterMock)};
    lgr.warn("TESTIII", "Text");

    //  void info(const std::string& msg, std::optional<T> metadata) {

    // logger.info<std::string>("TestMessage", "Optionalparam");
    //  Log written
    EXPECT_EQ(logWriterMock->isCalled, 1);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToFatalWithFatalLogMsg7) {
    setenv("LOG_LEVEL", "trace", 1);

    auto lgr{Logger(
        StreamrLogLevel::INFO,  "ContextBindingText", logWriterMock)};
    lgr.warn("TESTIII", "Text");


    EXPECT_EQ(logWriterMock->isCalled, 1);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToFatalWithFatalLogMsg5) {
    setenv("LOG_LEVEL", "trace", 1);

    auto lgr{Logger()};
    lgr.warn("TESTIII", "Text");

    //  void info(const std::string& msg, std::optional<T> metadata) {

    // logger.info<std::string>("TestMessage", "Optionalparam");
    //  Log written
    EXPECT_EQ(logWriterMock->isCalled, 1);
}
