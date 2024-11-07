// swift-tools-version: 6.0
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription


let package = Package(
    name: "StreamrProxyClient",
    platforms: [
        .iOS(.v17)
    ],
    products: [
        .library(
            name: "StreamrProxyClient",
            type: .static,
            targets: ["StreamrProxyClient"]),
    ],
    targets: [
        .target(
            name: "StreamrProxyClient",
            dependencies: ["streamr"],
            linkerSettings: [
                .linkedFramework("Foundation")
                // Add any other frameworks your library depends on
            ]),
        .binaryTarget(
            name: "streamr",
            path: "Frameworks/streamr.xcframework"
        ),
        .testTarget(
            name: "StreamrProxyClientTests",
            dependencies: ["StreamrProxyClient"]
        ),
    ]
)
