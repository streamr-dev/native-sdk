/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <folly/logging/Init.h>

#include <folly/init/Init.h>
#include <folly/logging/LogConfigParser.h>
#include <folly/logging/LoggerDB.h>
#include <folly/portability/GFlags.h>
#include <folly/portability/GTest.h>
#include <folly/test/TestUtils.h>

using folly::initLogging;
using folly::LoggerDB;
using folly::parseLogConfig;

namespace {
// A counter to help confirm that our getBaseLoggingConfigCalled() was invoked
// rather than the default implementation that folly exports as a weak symbol.
unsigned int getBaseLoggingConfigCalled;
} // namespace

namespace folly {

const char* getBaseLoggingConfig() {
    ++getBaseLoggingConfigCalled;
    return "folly=INFO; default:stream=stdout";
}

} // namespace folly

// We use our custom main() to ensure that folly::initLogging() has
// not been called yet when we start running the tests.
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    gflags::ParseCommandLineFlags(&argc, &argv, /* remove_flags = */ true);
    folly::Init init(&argc, &argv);
    return RUN_ALL_TESTS();
}
