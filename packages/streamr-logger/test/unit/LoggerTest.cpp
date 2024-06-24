#include <string>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <streamr-logger/Logger.hpp>

using streamr::logger::Logger;
using streamr::logger::detail::StreamrLogLevel;

struct LogWriterMock : public folly::LogWriter {
private:
    std::string mBuffer;
    int mIsCalled{0};

public:
    void writeMessage(folly::StringPiece buf, uint32_t /* flags */) override {
        std::cout << "!!!!!!!!!!!:" + buf.toString() + "\n";
        mBuffer = buf.toString();
        mIsCalled = 1;
    }

    [[nodiscard]] int getIsCalled() const { return mIsCalled; }

    std::string getBuffer() { return mBuffer; }

    [[nodiscard]] bool ttyOutput() const override { return true; }

    void flush() override {}
};

class LoggerTest : public testing::Test {
protected:
    std::shared_ptr<LogWriterMock> mLogWriterMock = // NOLINT
        std::make_shared<LogWriterMock>();

    Logger mLogger = // NOLINT
        Logger(std::string(""), StreamrLogLevel::INFO, mLogWriterMock);

    Logger& getLogger() { return mLogger; }
    std::shared_ptr<LogWriterMock> getLogWriterMock() { return mLogWriterMock; }
};

TEST_F(LoggerTest, NoLogLevelEnvVariableSetInfoLogSent) {
    unsetenv("LOG_LEVEL");

    getLogger().info("Testi");
    // Log message written with default info
    EXPECT_EQ(getLogWriterMock()->getIsCalled(), 1);
}

TEST_F(LoggerTest, NoLogLevelEnvVariableSetDebugLogNotSent) {
    unsetenv("LOG_LEVEL");

    getLogger().info("Testi");
    // Log message not written because default is info
    EXPECT_EQ(getLogWriterMock()->getIsCalled(), 1);
}

TEST_F(LoggerTest, LogLevelSetWronglyInEnvVariable) {
    unsetenv("NotFound");

    getLogger().info("Testi");
    // Info log message written
    EXPECT_EQ(getLogWriterMock()->getIsCalled(), 1);
}

// Env variable log level set to trace

TEST_F(LoggerTest, LogLevelEnvVariableSetToTraceWithTraceLogMsg) {
    setenv("LOG_LEVEL", "trace", 1);

    getLogger().trace("Testi");
    // Log written
    EXPECT_EQ(getLogWriterMock()->getIsCalled(), 1);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToTraceWithDebugLogMsg) {
    setenv("LOG_LEVEL", "trace", 1);

    getLogger().debug("Testi");
    // Log written
    EXPECT_EQ(getLogWriterMock()->getIsCalled(), 1);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToTraceWithInfoLogMsg) {
    setenv("LOG_LEVEL", "trace", 1);

    getLogger().info("Testi");
    // Log written
    EXPECT_EQ(getLogWriterMock()->getIsCalled(), 1);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToTracelWithWarnLogMsg) {
    setenv("LOG_LEVEL", "trace", 1);

    getLogger().warn("Testi");
    // Log written
    EXPECT_EQ(getLogWriterMock()->getIsCalled(), 1);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToTraceWithErrorLogMsg) {
    setenv("LOG_LEVEL", "trace", 1);

    getLogger().error("Testi");
    // Log written
    EXPECT_EQ(getLogWriterMock()->getIsCalled(), 1);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToTraceWithFatalLogMsg) {
    setenv("LOG_LEVEL", "trace", 1);

    getLogger().fatal("Testi");
    // Log written
    EXPECT_EQ(getLogWriterMock()->getIsCalled(), 1);
}

// Env variable log level set to debug

TEST_F(LoggerTest, LogLevelEnvVariableSetToDebugWithTraceLogMsg) {
    setenv("LOG_LEVEL", "debug", 1);

    getLogger().trace("Testi");
    // Log written
    EXPECT_EQ(getLogWriterMock()->getIsCalled(), 0);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToDebugWithDebugLogMsg) {
    setenv("LOG_LEVEL", "debug", 1);

    getLogger().debug("Testi");
    // Log written
    EXPECT_EQ(getLogWriterMock()->getIsCalled(), 1);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToDebugWithInfoLogMsg) {
    setenv("LOG_LEVEL", "debug", 1);

    getLogger().info("Testi");
    // Log written
    EXPECT_EQ(getLogWriterMock()->getIsCalled(), 1);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToDebugWithWarnLogMsg) {
    setenv("LOG_LEVEL", "debug", 1);

    getLogger().warn("Testi");
    // Log written
    EXPECT_EQ(getLogWriterMock()->getIsCalled(), 1);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToDebugWithErrorLogMsg) {
    setenv("LOG_LEVEL", "debug", 1);

    getLogger().error("Testi");
    // Log written
    EXPECT_EQ(getLogWriterMock()->getIsCalled(), 1);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToDebugWithFatalLogMsg) {
    setenv("LOG_LEVEL", "debug", 1);

    getLogger().fatal("Testi");
    // Log written
    EXPECT_EQ(getLogWriterMock()->getIsCalled(), 1);
}

// Env variable log level set to info

TEST_F(LoggerTest, LogLevelEnvVariableSetToInfoWithTraceLogMsg) {
    setenv("LOG_LEVEL", "info", 1);

    getLogger().trace("Testi");
    // Log written
    EXPECT_EQ(getLogWriterMock()->getIsCalled(), 0);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToInfoWithDebugLogMsg) {
    setenv("LOG_LEVEL", "info", 1);

    getLogger().debug("Testi");
    // Log written
    EXPECT_EQ(getLogWriterMock()->getIsCalled(), 0);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToInfoWithInfoLogMsg) {
    setenv("LOG_LEVEL", "info", 1);

    getLogger().info("Testi");
    // Log written
    EXPECT_EQ(getLogWriterMock()->getIsCalled(), 1);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToInfoWithWarnLogMsg) {
    setenv("LOG_LEVEL", "info", 1);

    getLogger().warn("Testi");
    // Log written
    EXPECT_EQ(getLogWriterMock()->getIsCalled(), 1);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToInfoWithErrorLogMsg) {
    setenv("LOG_LEVEL", "info", 1);

    getLogger().error("Testi");
    // Log written
    EXPECT_EQ(getLogWriterMock()->getIsCalled(), 1);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToInfoWithFatalLogMsg) {
    setenv("LOG_LEVEL", "info", 1);

    getLogger().fatal("Testi");
    // Log written
    EXPECT_EQ(getLogWriterMock()->getIsCalled(), 1);
}

// Env variable log level set to warn

TEST_F(LoggerTest, LogLevelEnvVariableSetToWarnWithTraceLogMsg) {
    setenv("LOG_LEVEL", "warn", 1);

    getLogger().trace("Testi");
    // Log written
    EXPECT_EQ(getLogWriterMock()->getIsCalled(), 0);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToWarnWithDebugLogMsg) {
    setenv("LOG_LEVEL", "warn", 1);

    getLogger().debug("Testi");
    // Log written
    EXPECT_EQ(getLogWriterMock()->getIsCalled(), 0);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToWarnWithInfoLogMsg) {
    setenv("LOG_LEVEL", "warn", 1);

    getLogger().info("Testi");
    // Log written
    EXPECT_EQ(getLogWriterMock()->getIsCalled(), 0);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToWarnWithWarnLogMsg) {
    setenv("LOG_LEVEL", "warn", 1);

    getLogger().warn("Testi");
    // Log written
    EXPECT_EQ(getLogWriterMock()->getIsCalled(), 1);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToWarnWithErrorLogMsg) {
    setenv("LOG_LEVEL", "warn", 1);

    getLogger().error("Testi");
    // Log written
    EXPECT_EQ(getLogWriterMock()->getIsCalled(), 1);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToWarnWithFatalLogMsg) {
    setenv("LOG_LEVEL", "warn", 1);

    getLogger().fatal("Testi");
    // Log written
    EXPECT_EQ(getLogWriterMock()->getIsCalled(), 1);
}
// Env variable log level set to error

TEST_F(LoggerTest, LogLevelEnvVariableSetErrorWithTraceLogMsg) {
    setenv("LOG_LEVEL", "error", 1);

    getLogger().trace("Testi");
    // Log written
    EXPECT_EQ(getLogWriterMock()->getIsCalled(), 0);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToErrorWithDebugLogMsg) {
    setenv("LOG_LEVEL", "error", 1);

    getLogger().debug("Testi");
    // Log written
    EXPECT_EQ(getLogWriterMock()->getIsCalled(), 0);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToErrorWithInfoLogMsg) {
    setenv("LOG_LEVEL", "error", 1);

    getLogger().info("Testi");
    // Log written
    EXPECT_EQ(getLogWriterMock()->getIsCalled(), 0);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToErrorWithWarnLogMsg) {
    setenv("LOG_LEVEL", "error", 1);

    getLogger().warn("Testi");
    // Log written
    EXPECT_EQ(getLogWriterMock()->getIsCalled(), 0);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToErrorWithErrorLogMsg) {
    setenv("LOG_LEVEL", "error", 1);

    getLogger().error("Testi");
    // Log written
    EXPECT_EQ(getLogWriterMock()->getIsCalled(), 1);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToErrorWithFatalLogMsg) {
    setenv("LOG_LEVEL", "error", 1);

    getLogger().fatal("Testi");
    // Log written
    EXPECT_EQ(getLogWriterMock()->getIsCalled(), 1);
}

// Env variable log level set to fatal
TEST_F(LoggerTest, LogLevelEnvVariableSetFatalWithTraceLogMsg) {
    setenv("LOG_LEVEL", "fatal", 1);

    getLogger().trace("Testi");
    // Log written
    EXPECT_EQ(getLogWriterMock()->getIsCalled(), 0);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToFatalWithDebugLogMsg) {
    setenv("LOG_LEVEL", "fatal", 1);

    getLogger().debug("Testi");
    // Log written
    EXPECT_EQ(getLogWriterMock()->getIsCalled(), 0);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToFatalWithInfoLogMsg) {
    setenv("LOG_LEVEL", "fatal", 1);

    getLogger().info("Testi");
    // Log written
    EXPECT_EQ(getLogWriterMock()->getIsCalled(), 0);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToFatalWithWarnLogMsg) {
    setenv("LOG_LEVEL", "fatal", 1);

    getLogger().warn("Testi");
    // Log written
    EXPECT_EQ(getLogWriterMock()->getIsCalled(), 0);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToFatalWithErrorLogMsg) {
    setenv("LOG_LEVEL", "fatal", 1);

    getLogger().error("Testi");
    // Log written
    EXPECT_EQ(getLogWriterMock()->getIsCalled(), 0);
}

TEST_F(LoggerTest, LogLevelEnvVariableSetToFatalWithFatalLogMsg) {
    setenv("LOG_LEVEL", "fatal", 1);

    getLogger().fatal("Testi");
    // Log written
    EXPECT_EQ(getLogWriterMock()->getIsCalled(), 1);
}

TEST_F(LoggerTest, TraceLogWithExtraArgumentText) {
    setenv("LOG_LEVEL", "trace", 1);

    getLogger().trace("Testi", std::string("LogExtraArgumentText"));

    EXPECT_THAT(getLogWriterMock()->getBuffer(), testing::HasSubstr("TRACE"));
    EXPECT_THAT(
        getLogWriterMock()->getBuffer(),
        testing::HasSubstr("Testi {\"metadata\":\"LogExtraArgumentText\"}"));
}

TEST_F(LoggerTest, DebugLogWithExtraArgumentText) {
    setenv("LOG_LEVEL", "debug", 1);

    getLogger().debug("Testi", std::string("LogExtraArgumentText"));

    EXPECT_THAT(getLogWriterMock()->getBuffer(), testing::HasSubstr("DEBUG"));
    EXPECT_THAT(
        getLogWriterMock()->getBuffer(),
        testing::HasSubstr("Testi {\"metadata\":\"LogExtraArgumentText\"}"));
}

TEST_F(LoggerTest, InfoLogWithExtraArgumentText) {
    setenv("LOG_LEVEL", "info", 1);

    getLogger().info("Testi", std::string("LogExtraArgumentText"));

    EXPECT_THAT(getLogWriterMock()->getBuffer(), testing::HasSubstr("INFO"));
    EXPECT_THAT(
        getLogWriterMock()->getBuffer(),
        testing::HasSubstr("Testi {\"metadata\":\"LogExtraArgumentText\"}"));
}

TEST_F(LoggerTest, WarnLogWithExtraArgumentText) {
    setenv("LOG_LEVEL", "warn", 1);

    getLogger().warn("Testi", std::string("LogExtraArgumentText"));

    EXPECT_THAT(getLogWriterMock()->getBuffer(), testing::HasSubstr("WARN"));
    EXPECT_THAT(
        getLogWriterMock()->getBuffer(),
        testing::HasSubstr("Testi {\"metadata\":\"LogExtraArgumentText\"}"));
}

TEST_F(LoggerTest, ErrorLogWithExtraArgumentText) {
    setenv("LOG_LEVEL", "error", 1);

    getLogger().error("Testi", std::string("LogExtraArgumentText"));

    EXPECT_THAT(getLogWriterMock()->getBuffer(), testing::HasSubstr("ERROR"));
    EXPECT_THAT(
        getLogWriterMock()->getBuffer(),
        testing::HasSubstr("Testi {\"metadata\":\"LogExtraArgumentText\"}"));
}

TEST_F(LoggerTest, FatalLogWithExtraArgumentText) {
    setenv("LOG_LEVEL", "fatal", 1);

    getLogger().fatal("Testi", std::string("LogExtraArgumentText"));

    EXPECT_THAT(getLogWriterMock()->getBuffer(), testing::HasSubstr("FATAL"));

    EXPECT_THAT(
        getLogWriterMock()->getBuffer(),
        testing::HasSubstr("Testi {\"metadata\":\"LogExtraArgumentText\"}"));
}

TEST_F(LoggerTest, FatalLogWithExtraArgumentObject) {
    setenv("LOG_LEVEL", "fatal", 1);

    struct TestData {
        std::string value1{"TestString"};
        int value2{42}; // NOLINT
    };

    auto testData = TestData();
    getLogger().fatal("Testi", testData);
    //  EXPECT_THAT(logWriterMock->buffer, TestData());
    EXPECT_THAT(
        getLogWriterMock()->getBuffer(),
        testing::HasSubstr("Testi {\"value1\":\"TestString\",\"value2\":42}"));
}

TEST(LoggerContextBindingAndMetadataMerge, StringsMerged) {
    setenv("LOG_LEVEL", "fatal", 1);
    std::shared_ptr<LogWriterMock> tmpLogWriterMock =
        std::make_shared<LogWriterMock>();
    Logger tmpLogger = Logger(
        std::string("ContextBindingText"),
        StreamrLogLevel::INFO,
        tmpLogWriterMock);

    tmpLogger.fatal("Testi", std::string("LogExtraArgumentText")); // NOLINT

    EXPECT_THAT(tmpLogWriterMock->getBuffer(), testing::HasSubstr("FATAL"));

    EXPECT_THAT(
        tmpLogWriterMock->getBuffer(),
        testing::HasSubstr(
            "{\"contextBindings\":\"ContextBindingText\",\"metadata\":\"LogExtraArgumentText\"}"));
}

TEST(
    LoggerContextBindingAndMetadataMerge,
    ContextBindingStringAndEmptyMetadataMerge) {
    setenv("LOG_LEVEL", "fatal", 1);
    std::shared_ptr<LogWriterMock> tmpLogWriterMock =
        std::make_shared<LogWriterMock>();
    Logger tmpLogger = Logger(
        std::string("ContextBindingText"),
        StreamrLogLevel::INFO,
        tmpLogWriterMock);

    tmpLogger.fatal("Testi"); // NOLINT

    EXPECT_THAT(tmpLogWriterMock->getBuffer(), testing::HasSubstr("FATAL"));

    EXPECT_THAT(
        tmpLogWriterMock->getBuffer(),
        testing::HasSubstr(
            "\x1B[36mTesti {\"contextBindings\":\"ContextBindingText\"}\x1B[0m"));
}

TEST(
    LoggerContextBindingAndMetadataMerge,
    EmptyContextBindingAndMetadataStringMerge) {
    setenv("LOG_LEVEL", "fatal", 1);
    std::shared_ptr<LogWriterMock> tmpLogWriterMock =
        std::make_shared<LogWriterMock>();
    Logger tmpLogger =
        Logger(std::string(""), StreamrLogLevel::INFO, tmpLogWriterMock);

    tmpLogger.fatal("Testi", std::string("LogExtraArgumentText")); // NOLINT

    EXPECT_THAT(tmpLogWriterMock->getBuffer(), testing::HasSubstr("FATAL"));

    EXPECT_THAT(
        tmpLogWriterMock->getBuffer(),
        testing::HasSubstr(
            "\x1B[36mTesti {\"metadata\":\"LogExtraArgumentText\"}\x1B[0m"));
}

TEST(LoggerContextBindingAndMetadataMerge, ObjectsMerged) {
    setenv("LOG_LEVEL", "fatal", 1);
    std::shared_ptr<LogWriterMock> tmpLogWriterMock =
        std::make_shared<LogWriterMock>();

    struct TestStruct1 {
        std::string foo1 = "bar1A";
        int foo2 = 42; // NOLINT
        std::string foo3 = "bar3";
    };

    struct TestStruct2 {
        std::string foo1 = "Bar1B";
        int foo4 = 24; // NOLINT
        std::string foo5 = "bar5";
    };

    Logger logger{TestStruct1(), StreamrLogLevel::INFO, tmpLogWriterMock};

    auto testStruct2 = TestStruct2();
    logger.fatal("Testi", testStruct2); // NOLINT
    EXPECT_THAT(tmpLogWriterMock->getBuffer(), testing::HasSubstr("FATAL"));

    EXPECT_THAT(
        tmpLogWriterMock->getBuffer(),
        testing::HasSubstr(
            "Testi {\"foo1\":\"bar1A\",\"foo2\":42,\"foo3\":\"bar3\",\"foo4\":24,\"foo5\":\"bar5\"}"));
}

TEST(
    LoggerContextBindingAndMetadataMerge, ContextBindingAndEmptyMetadataMerge) {
    setenv("LOG_LEVEL", "fatal", 1);
    std::shared_ptr<LogWriterMock> tmpLogWriterMock =
        std::make_shared<LogWriterMock>();

    struct TestStruct1 {
        std::string foo1 = "bar1A";
        int foo2 = 42; // NOLINT
        std::string foo3 = "bar3";
    };
    Logger logger{TestStruct1(), StreamrLogLevel::INFO, tmpLogWriterMock};

    logger.fatal("Testi"); // NOLINT
    EXPECT_THAT(tmpLogWriterMock->getBuffer(), testing::HasSubstr("FATAL"));

    EXPECT_THAT(
        tmpLogWriterMock->getBuffer(),
        testing::HasSubstr(
            "\x1B[36mTesti {\"foo1\":\"bar1A\",\"foo2\":42,\"foo3\":\"bar3\"}\x1B[0m"));
}

TEST(
    LoggerContextBindingAndMetadataMerge, EmptyContextBindingAndMetadataMerge) {
    setenv("LOG_LEVEL", "fatal", 1);
    std::shared_ptr<LogWriterMock> tmpLogWriterMock =
        std::make_shared<LogWriterMock>();

    struct TestStruct1 {
        std::string foo1 = "bar1A";
        int foo2 = 42; // NOLINT
        std::string foo3 = "bar3";
    };

    Logger logger{std::string(""), StreamrLogLevel::INFO, tmpLogWriterMock};

    auto testStruct1 = TestStruct1();
    logger.fatal("Testi", testStruct1); // NOLINT
    EXPECT_THAT(tmpLogWriterMock->getBuffer(), testing::HasSubstr("FATAL"));

    EXPECT_THAT(
        tmpLogWriterMock->getBuffer(),
        testing::HasSubstr(
            "\x1B[36mTesti {\"foo1\":\"bar1A\",\"foo2\":42,\"foo3\":\"bar3\"}\x1B[0m"));
}
