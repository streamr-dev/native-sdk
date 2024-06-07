#include <gtest/gtest.h>
#include <streamr-logger/Logger.hpp>
#include <gtest/gtest.h>
#include <streamr-logger/Logger.hpp>
#include <folly/logging/LogMessage.h>
#include <folly/logging/LogCategory.h>

class LoggerTest : public testing::Test {};

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

class StreamrFormatterTest : public testing::Test {

 protected:

  StreamrFormatter formatter_{};
};

TEST_F(StreamrFormatterTest, formatMessage) {
  
  const std::chrono::time_point<std::chrono::system_clock> now =
        std::chrono::system_clock::now();

  StreamrFormatter::StreamrLogMessage msg = {now, "Filename.cpp", 100, folly::LogLevel::DBG, "Message"};
  EXPECT_EQ(formatter_.formatMessageInStreamrStyle(msg), "");

}

