#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <streamr-logger/StreamrLogFormatter.hpp>
using streamr::logger::StreamrLogFormatter;
using namespace std::chrono; // NOLINT

class StreamrLogFormatterTest : public testing::Test {
    void SetUp() override {
        const auto ymd2 =
            std::chrono::year_month_day(2024y, std::chrono::January, 05d);
        tp = std::chrono::sys_days{ymd2};
    }

protected:
    const unsigned int lineNumber2{100}; // NOLINT
    const unsigned int lineNumber{101010}; // NOLINT
    streamr::logger::StreamrLogFormatter formatter_; // NOLINT
    std::chrono::system_clock::time_point tp; // NOLINT
};

TEST_F(StreamrLogFormatterTest, traceNoTruncate) {
    StreamrLogFormatter::StreamrLogMessage msg = {
        tp, "Filename.cpp", lineNumber2, folly::LogLevel::DBG, "Message"};

    EXPECT_THAT(
        formatter_.formatMessageInStreamrStyle(msg),
        testing::ContainsRegex(
            "90mTRACE.+0m \\[2024\\-01\\-0.T..:00:00\\.0\\] \\(Filename.cpp: 100                   \\): .+36mMessage.+0m"));
}

TEST_F(StreamrLogFormatterTest, traceTruncate) {
    StreamrLogFormatter::StreamrLogMessage msg = {
        tp,
        "1234567890123456789012345678901234567890.cpp",
        lineNumber2,
        folly::LogLevel::DBG,
        "Message"};

    EXPECT_THAT(
        formatter_.formatMessageInStreamrStyle(msg),
        testing::ContainsRegex(
            "90mTRACE.+0m \\[2024\\-01\\-0.T..:00:00\\.0\\] \\(1234567890123456789012345678901: 100\\): .+36mMessage.+0m"));
}

TEST_F(StreamrLogFormatterTest, debugNoTruncate) {
    StreamrLogFormatter::StreamrLogMessage msg = {
        tp, "Filename.cpp", lineNumber, folly::LogLevel::DBG0, "Message"};

    EXPECT_THAT(
        formatter_.formatMessageInStreamrStyle(msg),
        testing::ContainsRegex(
            "34mDEBUG.+0m \\[2024\\-01\\-0.T..:00:00\\.0\\] \\(Filename.cpp: 101010                \\): .+36mMessage.+0m"));
}

TEST_F(StreamrLogFormatterTest, infoNoTruncate) {
    StreamrLogFormatter::StreamrLogMessage msg = {
        tp, "Filename.cpp", lineNumber, folly::LogLevel::INFO, "Message"};

    EXPECT_THAT(
        formatter_.formatMessageInStreamrStyle(msg),
        testing::ContainsRegex(
            "32mINFO.+0m \\[2024\\-01\\-0.T..:00:00\\.0\\] \\(Filename.cpp: 101010                \\): .+36mMessage.+0m"));
}

TEST_F(StreamrLogFormatterTest, warnoNoTruncate) {
    StreamrLogFormatter::StreamrLogMessage msg = {
        tp, "Filename.cpp", lineNumber, folly::LogLevel::WARN, "Message"};

    EXPECT_THAT(
        formatter_.formatMessageInStreamrStyle(msg),
        testing::ContainsRegex(
            "33mWARN.+0m \\[2024\\-01\\-0.T..:00:00\\.0\\] \\(Filename.cpp: 101010                \\): .+36mMessage.+0m"));
}

TEST_F(StreamrLogFormatterTest, errorNoTruncate) {
    StreamrLogFormatter::StreamrLogMessage msg = {
        tp, "Filename.cpp", lineNumber, folly::LogLevel::ERR, "Message"};

    EXPECT_THAT(
        formatter_.formatMessageInStreamrStyle(msg),
        testing::ContainsRegex(
            "31mERROR.+0m \\[2024\\-01\\-0.T..:00:00\\.0\\] \\(Filename.cpp: 101010                \\): .+36mMessage.+0m"));
}

TEST_F(StreamrLogFormatterTest, fatalNoTruncate) {
    // Cannot use FATAL in Folly because it aborts, CRITICAL is converted to
    // FATAL
    StreamrLogFormatter::StreamrLogMessage msg = {
        tp, "Filename.cpp", lineNumber, folly::LogLevel::CRITICAL, "Message"};

    EXPECT_THAT(
        formatter_.formatMessageInStreamrStyle(msg),
        testing::ContainsRegex(
            "1;41mFATAL.+0m \\[2024\\-01\\-0.T..:00:00\\.0\\] \\(Filename.cpp: 101010                \\): .+36mMessage.+0m"));
}
