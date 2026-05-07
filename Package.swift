// swift-tools-version: 5.7
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let package = Package(
    name: "VideoSDKRTCSwift",
    platforms: [
        .iOS(.v13)
    ],
    products: [
        .library(
            name: "VideoSDKRTCSwift",
            targets: ["VideoSDKRTCSwift", "VideoSDKRTC", "VideoSDKCore"]),
    ],
   dependencies: [],
    targets: [
        .target(name: "VideoSDKRTCSwift", dependencies: []),
        .binaryTarget(name: "VideoSDKCore", path: "Sources/VideoSDKCore.xcframework"),
        .binaryTarget(name: "VideoSDKRTC", path: "Sources/VideoSDKRTC.xcframework"),
        .testTarget(name: "VideoSDKRTCSwiftTests", dependencies: ["VideoSDKRTCSwift"]),
    ]
)

