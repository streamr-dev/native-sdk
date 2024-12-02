// swift-tools-version: 6.0
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let package = Package(
    name: "streamrproxyclientunittests",
    platforms: [
        .iOS(.v17)
    ],
    dependencies: [
        .package(name: "ProxyClientAPI", path: "../StreamrProxyClient")
    ],
    targets: [
        .testTarget(
            name: "StreamrProxyClientUnitTestsTests",
              dependencies: [
                .product(name: "ProxyClientAPI", package: "ProxyClientAPI")
            ]),
        
    ]
   
)
