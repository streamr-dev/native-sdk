#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <streamr-logger/detail/StreamrLogFormatter.hpp>

using StreamrLogFormatter = streamr::logger::detail::StreamrLogFormatter;
using namespace std::chrono_literals; // NOLINT short chrono literals y, d etc.

class StreamrLogFormatterTest : public testing::Test {
protected:
    void SetUp() override {
        const auto ymd2 =
            std::chrono::year_month_day(2024y, std::chrono::January, 05d);
        mTp = std::chrono::sys_days{ymd2};
    }

    StreamrLogFormatter& getFormatter() { return mFormatter; }

    std::chrono::system_clock::time_point& getTp() { return mTp; }

    StreamrLogFormatter mFormatter; // NOLINT
    static constexpr unsigned int lineNumber2{100}; // NOLINT
    static constexpr unsigned int lineNumber{101010}; // NOLINT
    std::chrono::system_clock::time_point mTp; // NOLINT
};

TEST_F(StreamrLogFormatterTest, traceNoTruncate) {
    StreamrLogFormatter::StreamrLogMessage msg = {
        getTp(), "Filename.cpp", lineNumber2, folly::LogLevel::DBG, "Message"};

    EXPECT_THAT(
        getFormatter().formatMessageInStreamrStyle(msg),
        testing::ContainsRegex(
            "90mTRACE.+0m \\[2024\\-01\\-0.T..:00:00\\.0\\] \\(Filename.cpp: 100                   \\): .+36mMessage.+0m"));
}

TEST_F(StreamrLogFormatterTest, traceTruncate) {
    StreamrLogFormatter::StreamrLogMessage msg = {
        getTp(),
        "1234567890123456789012345678901234567890.cpp",
        lineNumber2,
        folly::LogLevel::DBG,
        "Message"};

    EXPECT_THAT(
        getFormatter().formatMessageInStreamrStyle(msg),
        testing::ContainsRegex(
            "90mTRACE.+0m \\[2024\\-01\\-0.T..:00:00\\.0\\] \\(1234567890123456789012345678901: 100\\): .+36mMessage.+0m"));
}

TEST_F(StreamrLogFormatterTest, debugNoTruncate) {
    StreamrLogFormatter::StreamrLogMessage msg = {
        getTp(), "Filename.cpp", lineNumber, folly::LogLevel::DBG0, "Message"};

    EXPECT_THAT(
        getFormatter().formatMessageInStreamrStyle(msg),
        testing::ContainsRegex(
            "34mDEBUG.+0m \\[2024\\-01\\-0.T..:00:00\\.0\\] \\(Filename.cpp: 101010                \\): .+36mMessage.+0m"));
}

TEST_F(StreamrLogFormatterTest, infoNoTruncate) {
    StreamrLogFormatter::StreamrLogMessage msg = {
        getTp(), "Filename.cpp", lineNumber, folly::LogLevel::INFO, "Message"};

    EXPECT_THAT(
        getFormatter().formatMessageInStreamrStyle(msg),
        testing::ContainsRegex(
            "32mINFO.+0m \\[2024\\-01\\-0.T..:00:00\\.0\\] \\(Filename.cpp: 101010                \\): .+36mMessage.+0m"));
}

TEST_F(StreamrLogFormatterTest, warnoNoTruncate) {
    StreamrLogFormatter::StreamrLogMessage msg = {
        getTp(), "Filename.cpp", lineNumber, folly::LogLevel::WARN, "Message"};

    EXPECT_THAT(
        getFormatter().formatMessageInStreamrStyle(msg),
        testing::ContainsRegex(
            "33mWARN.+0m \\[2024\\-01\\-0.T..:00:00\\.0\\] \\(Filename.cpp: 101010                \\): .+36mMessage.+0m"));
}

TEST_F(StreamrLogFormatterTest, errorNoTruncate) {
    StreamrLogFormatter::StreamrLogMessage msg = {
        getTp(), "Filename.cpp", lineNumber, folly::LogLevel::ERR, "Message"};

    EXPECT_THAT(
        getFormatter().formatMessageInStreamrStyle(msg),
        testing::ContainsRegex(
            "31mERROR.+0m \\[2024\\-01\\-0.T..:00:00\\.0\\] \\(Filename.cpp: 101010                \\): .+36mMessage.+0m"));
}

TEST_F(StreamrLogFormatterTest, fatalNoTruncate) {
    // Cannot use FATAL in Folly because it aborts, CRITICAL is converted to
    // FATAL
    StreamrLogFormatter::StreamrLogMessage msg = {
        getTp(),
        "Filename.cpp",
        lineNumber,
        folly::LogLevel::CRITICAL,
        "Message"};

    EXPECT_THAT(
        getFormatter().formatMessageInStreamrStyle(msg),
        testing::ContainsRegex(
            "1;41mFATAL.+0m \\[2024\\-01\\-0.T..:00:00\\.0\\] \\(Filename.cpp: 101010                \\): .+36mMessage.+0m"));
}
