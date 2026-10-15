# VideoSDKRTCSwift

A Swift framework for real-time audio and video communication for iOS applications.

## Features

- 🎥 Real-time audio and video calling
- 💬 Text chat
- 🔄 Screen sharing
- 👥 Multiple participants

## Requirements

- iOS 13.0+
- Swift 5.0+

## Installation

### Swift Package Manager

```swift
dependencies: [
    .package(url: "https://github.com/videosdk-live/videosdk-rtc-ios-spm.git", from: "4.0.1")
]
```

## Quick Start

```swift
import VideoSDKRTC

// Initialize and join meeting
do {
	room = try VideoSDK.createRoom(
		roomId: roomData.roomId,
		token: roomData.token,
		participantName: roomData.name,
		micEnabled: roomData.micEnabled,
		camEnabled: roomData.cameraEnabled
	)
} catch {
	print("Setup failed: \(error)")   // ERROR_INVALID_PARAMETER for an empty room ID or token
	return
}

// Adding the listener to room
room?.addEventListener(self)

// joining the room
Task {
	do {
		try await room?.join()
	} catch {
		print("Join failed: \(error)")
	}
}
```

## Documentation

For detailed documentation, visit [Documentation Portal](https://docs.videosdk.live/ios/guide/video-and-audio-calling-api-sdk/ios-sdk-spm)
