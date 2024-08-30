//
//  GTestWrapper.m
//  iOSUnitTestingTests
//
//  Created by Santtu Rantanen on 30.8.2024.
//

#import <XCTest/XCTest.h>
#import "gtest/gtest.h"

/*
int run_all_tests(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
*/
#import <XCTest/XCTest.h>
#import "gtest/gtest.h"

 extern "C" {
     
     const char* run_all_tests(int argc, char** argv) {
         testing::internal::CaptureStdout();
         testing::InitGoogleTest(&argc, argv);
         int result = RUN_ALL_TESTS();
         std::string output = testing::internal::GetCapturedStdout();
         return output.c_str();
     }
      /*
     const char* run_all_tests(int argc, char** argv) {
         // Redirect stdout to a stringstream
         std::stringstream buffer;
         std::streambuf* old = std::cout.rdbuf(buffer.rdbuf());

         // Run the tests
         testing::InitGoogleTest(&argc, argv);
         int result = RUN_ALL_TESTS();

         // Restore stdout
         std::cout.rdbuf(old);

         // Get the output as a string
         std::string output = buffer.str();

         // Prepend the result to the output
         output = "Test result: " + std::to_string(result) + "\n" + output;

         // Allocate memory for the string and copy it
         char* result_str = new char[output.length() + 1];
         std::strcpy(result_str, output.c_str());

         return result_str;
     }
     
     void free_test_output(const char* str) {
          delete[] str;
     }
       */
 }
