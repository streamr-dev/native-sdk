//
//  iOSUnitTestingTests.swift
//  iOSUnitTestingTests
//
//  Created by Santtu Rantanen on 30.8.2024.
//

import XCTest

class StreamrTests: XCTestCase {

    func testGoogleTests() {
        let runner = GoogleTestRunner()
        let argc: Int32 = 1
        var argv: [UnsafeMutablePointer<Int8>?] = [strdup("GTests")]
        let googleTestResult = runner.runAllTests(argc, &argv)
        let resultInSwiftString = String(googleTestResult.testOutput)
        print(resultInSwiftString)
        XCTAssertEqual(googleTestResult.result, 0)
    }
     
}
