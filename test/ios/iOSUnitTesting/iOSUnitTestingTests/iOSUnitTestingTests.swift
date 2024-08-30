//
//  iOSUnitTestingTests.swift
//  iOSUnitTestingTests
//
//  Created by Santtu Rantanen on 30.8.2024.
//

import XCTest

class YourProjectTests: XCTestCase {

    func testGoogleTests() {
        var runner = GoogleTestRunner()
        let argc: Int32 = 1
        var argv: [UnsafeMutablePointer<Int8>?] = [strdup("GTests")]
        let googleTestResult = runner.runAllTests(argc, &argv)
        let resultInSwiftString = String(googleTestResult.testOutput)
        print(resultInSwiftString)
        XCTAssertEqual(googleTestResult.result, 0)
    }
     
}
