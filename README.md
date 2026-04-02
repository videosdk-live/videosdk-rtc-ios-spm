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
    .package(url: "https://github.com/videosdk-live/videosdk-rtc-ios-spm.git", from: "2.7.1")
]
```

## Quick Start

```swift
import VideoSDKRTC

// Configure VideoSDK
VideoSDK.config(token: "VideoSDK Token")

// Initialize and join meeting
let meeting = VideoSDK.initMeeting(
		        meetingId: "abcd-1234-efgh",
		        participantName: "John",
		        micEnabled: true,
		        webcamEnabled: true
		    )

meeting.join()
```

## Documentation

For detailed documentation, visit [Documentation Portal](https://docs.videosdk.live/ios/guide/video-and-audio-calling-api-sdk/ios-sdk-spm)
