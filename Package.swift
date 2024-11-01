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
            name: "VideoSDKRTC",
            targets: ["VideoSDKRTC", "WebRTC", "Mediasoup"]),
    ],
    dependencies: [],
    targets: [
        .binaryTarget(name: "WebRTC", path: "Sources/WebRTC.xcframework"),
        .binaryTarget(name: "Mediasoup", path: "Sources/Mediasoup.xcframework"),
        .binaryTarget(name: "VideoSDKRTC", path: "Sources/VideoSDKRTC.xcframework")
    ]
)
