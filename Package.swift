// swift-tools-version: 5.7
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let package = Package(
    name: "VideoSDKRTCSwift",
    platforms: [
        .iOS(.v11)
    ],
    products: [
        .library(
            name: "VideoSDKRTCSwift",
            targets: ["VideoSDKRTCSwift", "VideoSDKRTC", "Starscream", "WebRTC", "Mediasoup"]),
    ],
    dependencies: [],
    targets: [
        .target(name: "VideoSDKRTCSwift", dependencies: []),
        .binaryTarget(name: "WebRTC", path: "Sources/WebRTC.xcframework"),
        .binaryTarget(name: "Mediasoup", path: "Sources/Mediasoup.xcframework"),
        .binaryTarget(name: "Starscream", path: "Sources/Starscream.xcframework"),
        .binaryTarget(name: "VideoSDKRTC", path: "Sources/VideoSDKRTC.xcframework"),
        .testTarget(name: "VideoSDKRTCSwiftTests", dependencies: ["VideoSDKRTCSwift"]),
    ]
)
