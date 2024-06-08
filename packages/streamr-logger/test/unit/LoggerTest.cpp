#include <gtest/gtest.h>
#include <streamr-logger/Logger.hpp>

class LoggerTest : public testing::Test {};

TEST_F(LoggerTest, success) {
    Logger logger;
    logger.log("test");
    int jaa = 1;
}
