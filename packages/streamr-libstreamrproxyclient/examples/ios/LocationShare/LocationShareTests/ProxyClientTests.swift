//
//  ProxyClientTests.swift
//  LocationShare
//
//  Created by Santtu Rantanen on 7.11.2024.
//

import XCTest
@testable import StreamrProxyClient

final class ProxyClientTests: XCTestCase {
    let validEthereumAddress =
        "0x1234567890123456789012345678901234567890"
    let invalidEthereumAddress =
    "INVALID_ETHEREUM_ADDRESS"
    let validStreamPartId =
    "0xa000000000000000000000000000000000000000#01"
    let invalidProxyUrl = "poiejrg039utg240"
    
    override func setUpWithError() throws {
        // Put setup code here. This method is called before the invocation of each test method in the class.
    }
    
    override func tearDownWithError() throws {
        // Put teardown code here. This method is called after the invocation of each test method in the class.
    }
    
    func testInvalidEthereumAddress() throws {
        
        XCTAssertThrowsError(try StreamrProxyClient(ownEthereumAddress: invalidEthereumAddress, streamPartId: validStreamPartId)) { error in
            guard let streamrError = error as? StreamrError else {
                XCTFail("Expected StreamrError, got \(type(of: error))")
                return
            }
            XCTAssertEqual(
                streamrError,
                StreamrError.initializationError("Failed to initialize StreamrProxyClient")
            )
        }
    }
    
    func testInvalidProxyUrl() async throws {
        // Create client with valid address
        let client = try StreamrProxyClient(ownEthereumAddress: validEthereumAddress, streamPartId: validStreamPartId)
        
        // Create a test proxy with invalid URL
        let proxies = [
            
            StreamrProxyAddress(websocketUrl: invalidProxyUrl, ethereumAddress: validEthereumAddress)
        ]
        
        // Try to connect and expect error
        let result = client.connect(proxies: proxies)
        
        // Verify results
        XCTAssertEqual(result.numConnected, 0)
        XCTAssertTrue(result.successful.isEmpty)
        XCTAssertEqual(result.failed.count, 1)
        
        // Check the error details
     //   let error = result.failed[0]
       // XCTAssertEqual(error.code, .invalidProxyUrl)  // Assuming StreamrProxyErrorCode is an enum
      //  XCTAssertFalse(error.message.isEmpty)
    }
    
    func testConnectSuccessfully() async throws {
        // Create client with valid address
        let client = try StreamrProxyClient(ownEthereumAddress: validEthereumAddress, streamPartId: "0xd2078dc2d780029473a39ce873fc182587be69db/low-level-client#0")
        
        // Create a test proxy with invalid URL
        let proxies = [
            
            StreamrProxyAddress(websocketUrl: "ws://95.216.15.80:44211", ethereumAddress: "0xd0d14b38d1f6b59d3772a63d84ece0a79e6e1c1f")
        ]
        
        // Try to connect and expect error
        let result = client.connect(proxies: proxies)
        
        // Verify results
        XCTAssertEqual(result.numConnected, 1)
        XCTAssertTrue(result.successful.isEmpty)
        XCTAssertEqual(result.failed.count, 0)
        
        // Check the error details
     //   let error = result.failed[0]
       // XCTAssertEqual(error.code, .invalidProxyUrl)  // Assuming StreamrProxyErrorCode is an enum
      //  XCTAssertFalse(error.message.isEmpty)
    }
}
