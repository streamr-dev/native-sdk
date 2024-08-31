//
//  GoogleTestRunner.cpp
//  iOSUnitTestingTests
//
//  Created by Santtu Rantanen on 30.8.2024.
//

#define GLOG_USE_GLOG_EXPORT
#define GLOG_EXPORT

#include "GoogleTestRunner.hpp"
#include "gtest/gtest.h"
#include <folly/coro/blockingWait.h>

GoogleTestResult GoogleTestRunner::runAllTests(int argc, char** argv) const {
    testing::internal::CaptureStdout();
    testing::InitGoogleTest(&argc, argv);
    int result = RUN_ALL_TESTS();
    std::string output = testing::internal::GetCapturedStdout();
    GoogleTestResult googleTestResult = {output, result};
    return googleTestResult;
}



