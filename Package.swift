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
            targets: ["VideoSDKRTCSwift", "VideoSDKRTC", "WebRTC", "Mediasoup", "SocketIO", "Starscream"]),
    ],
   dependencies: [],
    targets: [
        .target(name: "VideoSDKRTCSwift", dependencies: []),
        .binaryTarget(name: "WebRTC", path: "Sources/WebRTC.xcframework"),
        .binaryTarget(name: "Mediasoup", path: "Sources/Mediasoup.xcframework"),
        .binaryTarget(name: "VideoSDKRTC", path: "Sources/VideoSDKRTC.xcframework"),
        .binaryTarget(name: "SocketIO", path: "Sources/SocketIO.xcframework"),
        .binaryTarget(name: "Starscream", path: "Sources/Starscream.xcframework"),
        .testTarget(name: "VideoSDKRTCSwiftTests", dependencies: ["VideoSDKRTCSwift"]),
    ]
)

