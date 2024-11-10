// swift-tools-version: 6.0
// The swift-tools-version declares the minimum version of Swift required to build this package.
/*
 import PackageDescription
 
 let package = Package(
 name: "StreamrProxyClientAPI",
 products: [
 // Products define the executables and libraries a package produces, making them visible to other packages.
 .library(
 name: "StreamrProxyClientAPI",
 targets: ["StreamrProxyClientAPI"]),
 ],
 targets: [
 // Targets are the basic building blocks of a package, defining a module or a test suite.
 // Targets can depend on other targets in this package and products from dependencies.
 .target(
 name: "StreamrProxyClientAPI"),
 .testTarget(
 name: "StreamrProxyClientAPITests",
 dependencies: ["StreamrProxyClientAPI"]
 ),
 ]
 )
 */
 // swift-tools-version: 6.0
 // The swift-tools-version declares the minimum version of Swift required to build this package.

 import PackageDescription


 let package = Package(
     name: "StreamrProxyClientAPI",
     platforms: [
         .iOS(.v17)
     ],
     products: [
         .library(
             name: "StreamrProxyClientAPI",
             type: .static,
             targets: ["StreamrProxyClientAPI"]),
     ],
     targets: [
         .target(
             name: "StreamrProxyClientAPI",
             linkerSettings: [
                 .linkedFramework("Foundation")
                 // Add any other frameworks your library depends on
             ]),
         .testTarget(
             name: "StreamrProxyClientAPITests",
             dependencies: ["StreamrProxyClientAPI"]
         ),
     ]
 )

