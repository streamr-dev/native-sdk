#include <streamr-logger/Logger.hpp>

#include <folly/Portability.h>
#include <folly/portability/GTest.h>

class LoggerTest : public testing::Test {
};

TEST_F(LoggerTest, success) {
  Logger logger;
  logger.log("test");
}

TEST_F(LoggerTest, failure) {
  Logger logger;
  logger.log("test failure");
  EXPECT_EQ(1, 2);
}

