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
                name: "StreamrProxyClientActor",
                targets: ["StreamrProxyClientActor"]),
           .library(
               name: "StreamrProxyClient",
               targets: ["StreamrProxyClient"]),
           .library(
               name: "ProxyClientAPI",
               targets: ["ProxyClientAPI"])
       ],
   
    targets: [
        .target(
            name: "StreamrProxyClientActor",
            dependencies: ["StreamrProxyClient"],
            linkerSettings: [
                .linkedFramework("Foundation")
                // Add any other frameworks your library depends on
            ]),
        .target(
            name: "StreamrProxyClient",
            dependencies: ["streamr", "ProxyClientAPI"],
            linkerSettings: [
                .linkedFramework("Foundation")
                // Add any other frameworks your library depends on
            ]),
        .target(
            name: "ProxyClientAPI",
            linkerSettings: [
                .linkedFramework("Foundation")
                // Add any other frameworks your library depends on
            ]),
        .binaryTarget(
            name: "streamr",
            path: "Frameworks/streamr.xcframework"
        ),
    
    ]
)
