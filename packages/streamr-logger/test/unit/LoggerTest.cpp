#include <gtest/gtest.h>
#include <streamr-logger/Logger.hpp>
#include <gtest/gtest.h>
#include <streamr-logger/Logger.hpp>
#include <folly/logging/LogMessage.h>
#include <folly/logging/LogCategory.h>
#include <string>

class LoggerTest : public testing::Test {};

/*
TEST_F(LoggerTest, trace) {
  Logger logger;
    logger.logFatal("test failure");
//  logger.logTrace("Message for trace");
  EXPECT_EQ(1, 2);
}
*/
/*
TEST_F(LoggerTest, success) {
  Logger logger;
  logger.log("test");
  int JAA = 1;
}

TEST_F(LoggerTest, failure) {
  Logger logger;
  logger.log("test failure");
  EXPECT_EQ(1, 2);
}

TEST_F(LoggerTest, trace) {
  Logger logger;
  logger.logTrace("Message for trace");
  EXPECT_EQ(1, 2);
}
*/

class StreamrLogFormatterTest : public testing::Test {

 protected:

  StreamrLogFormatter formatter_{};
};

TEST_F(StreamrLogFormatterTest, traceNoTruncate) {
  using namespace std::chrono;
  constexpr auto ymd2 = std::chrono::year_month_day(
    2024y, std::chrono::January, 31d 
  );
  std::chrono::system_clock::time_point tp = std::chrono::sys_days{ymd2};
  StreamrLogFormatter::StreamrLogMessage msg = {tp, "Filename.cpp", 100, folly::LogLevel::DBG, "Message"};
  EXPECT_EQ(formatter_.formatMessageInStreamrStyle(msg), "\x1B[90mTRACE\x1B[0m [2024-01-31T02:00:00.0] (Filename.cpp: 100                   ): \x1B[36mMessage\x1B[0m\n");
}

TEST_F(StreamrLogFormatterTest, traceTruncate) {
  using namespace std::chrono;
  constexpr auto ymd2 = std::chrono::year_month_day(
    2024y, std::chrono::January, 31d 
  );
  std::chrono::system_clock::time_point tp = std::chrono::sys_days{ymd2};
  StreamrLogFormatter::StreamrLogMessage msg = {tp, "1234567890123456789012345678901234567890.cpp", 100, folly::LogLevel::DBG, "Message"};
  EXPECT_EQ(formatter_.formatMessageInStreamrStyle(msg), "\x1B[90mTRACE\x1B[0m [2024-01-31T02:00:00.0] (1234567890123456789012345678901: 100): \x1B[36mMessage\x1B[0m\n");
}

TEST_F(StreamrLogFormatterTest, debugNoTruncate) {
  using namespace std::chrono;
  constexpr auto ymd2 = std::chrono::year_month_day(
    2024y, std::chrono::January, 31d 
  );
  std::chrono::system_clock::time_point tp = std::chrono::sys_days{ymd2};
  StreamrLogFormatter::StreamrLogMessage msg = {tp, "Filename.cpp", 101010, folly::LogLevel::DBG0, "Message"};
  EXPECT_EQ(formatter_.formatMessageInStreamrStyle(msg), "\x1B[34mDEBUG\x1B[0m [2024-01-31T02:00:00.0] (Filename.cpp: 101010                ): \x1B[36mMessage\x1B[0m\n");
}

TEST_F(StreamrLogFormatterTest, infoNoTruncate) {
  using namespace std::chrono;
  constexpr auto ymd2 = std::chrono::year_month_day(
    2024y, std::chrono::January, 31d 
  );
  std::chrono::system_clock::time_point tp = std::chrono::sys_days{ymd2};
  StreamrLogFormatter::StreamrLogMessage msg = {tp, "Filename.cpp", 101010, folly::LogLevel::INFO, "Message"};
  EXPECT_EQ(formatter_.formatMessageInStreamrStyle(msg), "\x1B[32mINFO\x1B[0m [2024-01-31T02:00:00.0] (Filename.cpp: 101010                ): \x1B[36mMessage\x1B[0m\n");
}

TEST_F(StreamrLogFormatterTest, warnoNoTruncate) {
  using namespace std::chrono;
  constexpr auto ymd2 = std::chrono::year_month_day(
    2024y, std::chrono::January, 31d 
  );
  std::chrono::system_clock::time_point tp = std::chrono::sys_days{ymd2};
  StreamrLogFormatter::StreamrLogMessage msg = {tp, "Filename.cpp", 101010, folly::LogLevel::WARN, "Message"};
  EXPECT_EQ(formatter_.formatMessageInStreamrStyle(msg), "\x1B[33mWARN\x1B[0m [2024-01-31T02:00:00.0] (Filename.cpp: 101010                ): \x1B[36mMessage\x1B[0m\n");
}

TEST_F(StreamrLogFormatterTest, errorNoTruncate) {
  using namespace std::chrono;
  constexpr auto ymd2 = std::chrono::year_month_day(
    2024y, std::chrono::January, 31d 
  );
  std::chrono::system_clock::time_point tp = std::chrono::sys_days{ymd2};
  StreamrLogFormatter::StreamrLogMessage msg = {tp, "Filename.cpp", 101010, folly::LogLevel::ERR, "Message"};
  EXPECT_EQ(formatter_.formatMessageInStreamrStyle(msg), "\x1B[31mERROR\x1B[0m [2024-01-31T02:00:00.0] (Filename.cpp: 101010                ): \x1B[36mMessage\x1B[0m\n");
}

TEST_F(StreamrLogFormatterTest, fatalNoTruncate) {
  using namespace std::chrono;
  constexpr auto ymd2 = std::chrono::year_month_day(
    2024y, std::chrono::January, 31d 
  );
  std::chrono::system_clock::time_point tp = std::chrono::sys_days{ymd2};
  StreamrLogFormatter::StreamrLogMessage msg = {tp, "Filename.cpp", 101010, folly::LogLevel::FATAL, "Message"};
  EXPECT_EQ(formatter_.formatMessageInStreamrStyle(msg), "\x1B[1;41mFATAL\x1B[0m [2024-01-31T02:00:00.0] (Filename.cpp: 101010                ): \x1B[36mMessage\x1B[0m\n");
}

