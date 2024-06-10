#include <string>
#include <gtest/gtest.h>
#include <streamr-logger/Logger.hpp>
#include <folly/logging/LogCategory.h>
#include <folly/logging/LogMessage.h>

class LoggerTest : public testing::Test {};

using folly::LoggerDB;
using folly::LogLevel;
using streamr::logger::Logger;
using streamr::logger::StreamrLogFormatter;
using namespace std::chrono;

class StreamrLogFormatterTest : public testing::Test {
    // using streamr::logger::StreamrLogFormatter;
    void SetUp() override {
      const auto ymd2 = year_month_day(2024y, January, 31d);
      tp = sys_days{ymd2};
    }

   protected:
    StreamrLogFormatter formatter_{};
    system_clock::time_point tp;
};

TEST_F(StreamrLogFormatterTest, traceNoTruncate) {
    StreamrLogFormatter::StreamrLogMessage msg = {
        tp, "Filename.cpp", 100, folly::LogLevel::DBG, "Message"};
    EXPECT_EQ(
        formatter_.formatMessageInStreamrStyle(msg),
        "\x1B[90mTRACE\x1B[0m [2024-01-31T02:00:00.0] (Filename.cpp: 100                   ): \x1B[36mMessage\x1B[0m\n");
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
        "\x1B[90mTRACE\x1B[0m [2024-01-31T02:00:00.0] (1234567890123456789012345678901: 100): \x1B[36mMessage\x1B[0m\n");
}

TEST_F(StreamrLogFormatterTest, debugNoTruncate) {
    StreamrLogFormatter::StreamrLogMessage msg = {
        tp, "Filename.cpp", 101010, folly::LogLevel::DBG0, "Message"};
    EXPECT_EQ(
        formatter_.formatMessageInStreamrStyle(msg),
        "\x1B[34mDEBUG\x1B[0m [2024-01-31T02:00:00.0] (Filename.cpp: 101010                ): \x1B[36mMessage\x1B[0m\n");
}

TEST_F(StreamrLogFormatterTest, infoNoTruncate) {
    StreamrLogFormatter::StreamrLogMessage msg = {
        tp, "Filename.cpp", 101010, folly::LogLevel::INFO, "Message"};
    EXPECT_EQ(
        formatter_.formatMessageInStreamrStyle(msg),
        "\x1B[32mINFO\x1B[0m [2024-01-31T02:00:00.0] (Filename.cpp: 101010                ): \x1B[36mMessage\x1B[0m\n");
}

TEST_F(StreamrLogFormatterTest, warnoNoTruncate) {
    StreamrLogFormatter::StreamrLogMessage msg = {
        tp, "Filename.cpp", 101010, folly::LogLevel::WARN, "Message"};
    EXPECT_EQ(
        formatter_.formatMessageInStreamrStyle(msg),
        "\x1B[33mWARN\x1B[0m [2024-01-31T02:00:00.0] (Filename.cpp: 101010                ): \x1B[36mMessage\x1B[0m\n");
}

TEST_F(StreamrLogFormatterTest, errorNoTruncate) {
    StreamrLogFormatter::StreamrLogMessage msg = {
        tp, "Filename.cpp", 101010, folly::LogLevel::ERR, "Message"};
    EXPECT_EQ(
        formatter_.formatMessageInStreamrStyle(msg),
        "\x1B[31mERROR\x1B[0m [2024-01-31T02:00:00.0] (Filename.cpp: 101010                ): \x1B[36mMessage\x1B[0m\n");
}

TEST_F(StreamrLogFormatterTest, fatalNoTruncate) {
    StreamrLogFormatter::StreamrLogMessage msg = {
        tp, "Filename.cpp", 101010, folly::LogLevel::FATAL, "Message"};
    EXPECT_EQ(
        formatter_.formatMessageInStreamrStyle(msg),
        "\x1B[1;41mFATAL\x1B[0m [2024-01-31T02:00:00.0] (Filename.cpp: 101010                ): \x1B[36mMessage\x1B[0m\n");
}

TEST_F(LoggerTest, initialization) {
    auto logger = Logger::get();
}

TEST_F(LoggerTest, NoLogLevelEnvVariableSet) {
    auto logger = Logger::get();
    unsetenv("LOG_LEVEL");
    // No log message written
    EXPECT_EQ(logger.log(LogLevel::INFO, "Testi"), false);
}

TEST_F(LoggerTest, LogLevelSetWronglyInEnvVariable) {
    auto logger = Logger::get();
    setenv("LOG_LEVEL", "NotFound", 1);
    // No log message written
    EXPECT_EQ(logger.log(LogLevel::INFO, "Testi"), false);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToLowestLevel) {
    auto logger = Logger::get();
    setenv("LOG_LEVEL", "trace", 1);
    // Log written
    EXPECT_EQ(logger.log(LogLevel::DBG, "Testi"), true);
}

TEST_F(LoggerTest, RootLogLevelSetToHigherNewValue) {
    auto logger = Logger::get();
    setenv("LOG_LEVEL", "trace", 1);
    setenv("LOG_LEVEL", "fatal", 1);
    // Log not written
    EXPECT_EQ(logger.log(LogLevel::INFO, "Testi"), true);
}

TEST_F(LoggerTest, LoggerDBIntializationWithDefaultLogMinLevel) {
    LoggerDB db{LoggerDB::TESTING};
    Logger logger{Logger(db)};

    // Check that minimum root log level set
    EXPECT_EQ(db.getCategory("")->getLevel(), LogLevel::MIN_LEVEL);
}

TEST_F(LoggerTest, LoggerDBIntializationWithStreamrHandler) {
    LoggerDB db{LoggerDB::TESTING};
    Logger logger{Logger(db)};
    // Check that minimum root log level set
    EXPECT_EQ(db.getCategory("")->getHandlers().size(), 1);
}

TEST_F(LoggerTest, LoggerDBIntializationWithoutStreamrHandler) {
    LoggerDB db{LoggerDB::TESTING};
    Logger logger{Logger(db, false)};
    // Check that minimum root log level set
    EXPECT_EQ(db.getCategory("")->getHandlers().size(), 0);
}