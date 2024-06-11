#include "streamr-logger/Logger.hpp"
#include <gtest/gtest.h>

using Logger = streamr::logger::Logger;

class LoggerTest : public testing::Test {};

TEST_F(LoggerTest, success) {
    Logger logger;
    logger.log("test");
    int jaa = 1;
}
