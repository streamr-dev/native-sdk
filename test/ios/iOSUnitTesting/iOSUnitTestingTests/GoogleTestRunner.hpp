//
//  GoogleTestRunner.hpp
//  iOSUnitTestingTests
//
//  Created by Santtu Rantanen on 30.8.2024.
//

#ifndef GoogleTestRunner_hpp
#define GoogleTestRunner_hpp

#include <string>

struct GoogleTestResult {
    std::string testOutput;
    int result;
};

class GoogleTestRunner {
public:
    GoogleTestRunner() { }
    GoogleTestResult runAllTests(int argc, char** argv);
};

#endif /* GoogleTestRunner_hpp */
