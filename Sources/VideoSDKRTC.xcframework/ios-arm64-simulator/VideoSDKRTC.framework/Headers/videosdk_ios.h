#ifndef VIDEOSDK_IOS_H
#define VIDEOSDK_IOS_H

#include <stdbool.h>
#include <stdint.h>

typedef void (*VideoSDKEventCallback)(const char *event_name, const char *data);

void videosdk_init(void);
void videosdk_setEventCallback(VideoSDKEventCallback callback);

/// Returns a static C string with the build stamp baked in by `build.rs` at
/// compile time. Format: `<branch>@<short-hash>[+dirty] webrtc-build@<short-hash>`.
/// Pointer is owned by the SDK; do NOT free.
const char *videosdk_get_build_id(void);

/// Join a meeting.
/// Join a meeting. `device_info_json` is a UTF-8 JSON string for the
/// `deviceInfo` object sent on join (NULL or empty falls back to the
/// SDK default). `mode` is the participant mode string ("CONFERENCE" |
/// "VIEWER" | "SEND_AND_RECV" | "RECV_ONLY" | "SIGNALLING_ONLY"); NULL,
/// empty, or unknown falls back to "SEND_AND_RECV". The Rust core uses
/// this mode to gate produce — `RECV_ONLY`, `SIGNALLING_ONLY`, and
/// `VIEWER` skip mic/webcam production entirely even when
/// `mic_enabled` / `webcam_enabled` are true.
int32_t meeting_join(const char *token, const char *meeting_id, const char *name,
                  bool mic_enabled, bool webcam_enabled, bool multistream,
                  const char *device_info_json, const char *mode);
/// Leave the current meeting. Only the local participant departs.
void meeting_leave(void);

/// End the meeting for all participants. Sends the `closeRoom`
/// signaling event so the server disconnects every remote participant,
/// then tears down the local session.
void meeting_end(void);

/// Configure the room-wide shared key for end-to-end encryption.
/// Must be called BEFORE meeting_join so the key is in place when
/// the Rust layer builds its MeetingConfig. Passing NULL clears the
/// key (disables E2EE for the next join). Returns 0 on success,
/// -1 on invalid UTF-8.
int32_t meeting_setE2EEKey(const char *key);

/// Advance the E2EE epoch by one ratchet step. The next encrypted
/// frame carries the new kid; the previous epoch is retained by the
/// receive ring so in-flight frames still decrypt until every
/// sender has rotated. Returns 0 on success, -1 when no key is
/// configured.
int32_t meeting_ratchetE2EEKey(void);
/// Configure the E2EE AEAD algorithm. The wire format is selected by
/// the algorithm choice — VideoSDK variants interoperate with
/// libwebrtc-based clients (PBKDF2 key derivation, trailer IV/key-id
/// layout, AAD-bound cleartext prefix). Rust-native variants use HKDF
/// derivation and a leading-header layout.
///   0 = VSDK_AES_GCM_128 (default, libwebrtc-compatible)
///   1 = VSDK_AES_GCM_256 (libwebrtc-compatible)
///   2 = AES_GCM_128      (Rust-native, peer must be Rust SDK)
///   3 = AES_GCM_256      (Rust-native, peer must be Rust SDK)
/// MUST be called before meeting_join. Returns 0 on success, -1 when
/// the value is outside the supported range.
int32_t meeting_setE2EEAlgorithm(int32_t algorithm);


// Audio
void meeting_enableMic(void);
void meeting_disableMic(void);
uint32_t videosdk_pushAudioCapture(const int16_t *samples, uint32_t count);
uint32_t videosdk_pullAudioPlayback(int16_t *out_samples, uint32_t count);
uint32_t videosdk_audioPlaybackAvailable(void);

// Video capture
int32_t videosdk_videoInit(uint32_t width, uint32_t height, uint32_t fps, int32_t codec);
int32_t videosdk_pushVideoCapture(const uint8_t *yuv_data, uint32_t len, int64_t pts_us, bool force_keyframe);

/// Push NV12 camera frame directly via plane pointers (zero intermediate buffers).
int32_t videosdk_pushVideoCaptureNV12(const uint8_t *y_src, uint32_t y_stride,
                                     const uint8_t *uv_src, uint32_t uv_stride,
                                     uint32_t width, uint32_t height,
                                     int64_t pts_us);

void meeting_enableWebcam(void);
void meeting_disableWebcam(void);

/// Force the next encoded frame to be a keyframe (IDR).
void stream_requestKeyframe(void);

// Video playback — raw YUV I420 per participant (Swift renders via GPU)
uint32_t videosdk_pollVideoPlaybackYUV(const char *participant_id,
                                      uint8_t *out_yuv, uint32_t buf_capacity,
                                      uint32_t *out_width, uint32_t *out_height);

/// Zero-copy: Rust reads I420, converts to NV12, writes directly into CVPixelBuffer planes.
/// Eliminates intermediate Swift buffer.
///
/// Pass the CVPixelBuffer's allocated dimensions via `buf_width`/`buf_height`.
/// If the decoded frame's resolution differs (e.g. the SFU just switched
/// simulcast layers), this function will NOT write past the buffer; it
/// returns 2 with the frame's real dimensions in `out_width`/`out_height`
/// and does not consume the frame, so the caller can recreate the
/// CVPixelBuffer at the correct size and retry on the next poll.
///
/// Returns:
///   0 — no frame ready / error
///   1 — success, NV12 written, frame consumed
///   2 — resolution mismatch; recreate pixel buffer at out_width/out_height
int32_t videosdk_pollVideoPlaybackNV12(const char *participant_id,
                                    uint8_t *y_dst, uint32_t y_stride,
                                    uint8_t *uv_dst, uint32_t uv_stride,
                                    uint32_t buf_width, uint32_t buf_height,
                                    uint32_t *out_width, uint32_t *out_height);

/// Pull the latest decoded screen-share frame for a participant.
/// Mirror of videosdk_pollVideoPlaybackYUV but reads the dedicated
/// screen-share frame slot, so a peer with both camera and share
/// active can render both concurrently without the two writers
/// fighting over one slot. Same return-code contract.
uint32_t videosdk_pollScreenSharePlaybackYUV(const char *participant_id,
                                        uint8_t *out_yuv, uint32_t buf_capacity,
                                        uint32_t *out_width, uint32_t *out_height);

/// NV12 variant of videosdk_pollScreenSharePlaybackYUV. Identical
/// contract to videosdk_pollVideoPlaybackNV12 but reads the
/// screen-share slot.
int32_t videosdk_pollScreenSharePlaybackNV12(const char *participant_id,
                                        uint8_t *y_dst, uint32_t y_stride,
                                        uint8_t *uv_dst, uint32_t uv_stride,
                                        uint32_t buf_width, uint32_t buf_height,
                                        uint32_t *out_width, uint32_t *out_height);

// Participants
char *meeting_getParticipants(void);
/// Returns the current session ID issued by the server (the value
/// observed in the latest `entryResponded` signaling event), or an
/// empty string if no session is established yet. Caller must free
/// with `videosdk_freeString`.
char *meeting_getSessionId(void);
void videosdk_freeString(char *s);

// ── B.1: Recording / HLS / Livestream / Whiteboard / Transcription ──────────
//
// All start_* methods accept optional JSON strings for the complex config
// parameters. Pass NULL (or an empty string) to omit. Return codes:
//   0  — success
//  -1  — SDK returned an error (see logs)
//  -2  — no active meeting

int32_t meeting_startRecording(const char *webhook_url,
                             const char *aws_dir_path,
                             const char *config_json,
                             const char *transcription_json);
int32_t meeting_stopRecording(void);

/// `outputs_json` must be a JSON array of `{"url":"...","streamKey":"..."}`.
int32_t meeting_startLivestream(const char *outputs_json);
int32_t meeting_stopLivestream(void);

int32_t meeting_startHls(const char *config_json, const char *transcription_json);
int32_t meeting_stopHls(void);

int32_t meeting_startTranscription(const char *config_json);
int32_t meeting_stopTranscription(void);

/// Pass NULL to use the iOS default of "v2".
int32_t meeting_startWhiteboard(const char *version);
int32_t meeting_stopWhiteboard(void);

// ── B.2: Stream pause/resume + bulk pause + adaptive subscription ──────────
//
// Single-consumer pause/resume take the server-assigned `consumerId`
// string (exposed on `Stream::consumer_id` in Rust). Bulk variants take
// the iOS `kind` string: "audio" | "video" | "share" | "all".
// Pass NULL to default to "all". Return codes match the B.1 FFI block.

int32_t stream_pause(const char *consumer_id);
int32_t stream_resume(const char *consumer_id);

int32_t meeting_pauseAllStreams(const char *kind);
int32_t meeting_resumeAllStreams(const char *kind);

int32_t meeting_enableAdaptiveSubscription(void);
int32_t meeting_disableAdaptiveSubscription(void);

// ── B.3: RealtimeStore key-value shared state ──────────────────────────────
//
// Observe/stop-observing are intentionally not surfaced across the FFI;
// Swift should consume `realtimeStoreValueChanged` events via the
// existing `VideoSDKEventCallback` and use these two helpers for the
// mutation and read paths.

/// Set a realtime-store key. Pass NULL for `value` to clear the key.
/// Return codes match the B.1/B.2 FFI block.
int32_t realtimeStore_set(const char *key, const char *value);

/// Fetch a realtime-store key's current value. Caller must free the
/// returned string with `videosdk_freeString`. Returns NULL on error or
/// when the key has no value.
char *realtimeStore_get(const char *key);

// ── B.4: Pin / set_quality / entry response / pubsub history ───────────────

/// Request a remote participant to enable their microphone.
int32_t participant_enableMic(const char *peer_id);

/// Request a remote participant to disable their microphone.
int32_t participant_disableMic(const char *peer_id);

/// Request a remote participant to enable their webcam.
int32_t participant_enableWebcam(const char *peer_id);

/// Request a remote participant to disable their webcam.
int32_t participant_disableWebcam(const char *peer_id);

/// Remove a participant from the meeting.
int32_t participant_remove(const char *peer_id);

/// Pin a participant's streams. `kind` accepts "CAM" | "SHARE" |
/// "SHARE_AND_CAM" (case-insensitive); NULL defaults to SHARE_AND_CAM.
int32_t participant_pin(const char *peer_id, const char *kind);

/// Clear a participant's pin flags. Argument semantics match
/// `participant_pin`.
int32_t participant_unpin(const char *peer_id, const char *kind);

/// Snapshot of pinned participants as a JSON object
/// `{peerId: {cam, share}}`. Caller must free with `videosdk_freeString`.
char *meeting_getPinnedParticipants(void);

/// Request a specific forwarding quality for a participant's video
/// stream. `quality` accepts "low" | "medium" (or "med") | "high"
/// (case-insensitive); NULL defaults to "high".
int32_t participant_setQuality(const char *peer_id, const char *quality);

/// Respond to an outstanding entry request. `decision` must be
/// "allowed" or "denied".
int32_t meeting_respondEntry(const char *peer_id, const char *decision);

// ── Phase F: Mode / attributes / log level ──────────────────────────────────

/// Set the custom video track configuration. Call before
/// `videosdk_videoInit`. Pass a JSON string matching CameraVideoTrackConfig.
void videosdk_setVideoTrackConfig(const char *config_json);

/// Set the SDK log level. Call before `meeting_join`. `level` accepts
/// "none" | "error" | "warn" | "info" | "debug" | "all".
void videosdk_setLogLevel(const char *level);

/// Change the local participant's mode mid-meeting. `mode` accepts
/// "CONFERENCE" | "VIEWER" | "SEND_AND_RECV" | "RECV_ONLY" | "SIGNALLING_ONLY".
int32_t meeting_changeMode(const char *mode);

/// Switch the active session to a different meeting room without
/// destroying the local Meeting object. The server validates the
/// request, replies with `entryResponded` carrying `switchingRoomId`,
/// and the SDK soft-closes the current room and re-joins the new one.
/// `mode` accepts the same values as `meeting_changeMode`.
int32_t meeting_switchTo(const char *meeting_id, const char *token, const char *mode);

/// Reset the mic + video capture tracks so the next call to
/// `videosdk_videoInit` and `meeting_enableMic` installs fresh ones.
/// Required after `meeting_switchTo` because the old tracks remain
/// bound to the previous room's peer connection — re-using them
/// silently deadlocks `add_track` on the new room's PC.
void videosdk_resetMediaTracks(void);

/// Store attributes on the meeting.
int32_t meeting_setAttributes(const char *name, const char *version);

// ── Screen share + stats (Phase D) ──────────────────────────────────────────

/// Initialise the screen-share encoder. Call BEFORE the first
/// `videosdk_pushScreenShareNV12` and before `meeting_enableScreenShare`.
/// `codec`: 0 = VP8, 1 = H264. Screen content is always single-layer
/// (no simulcast). Returns 0 on success.
int32_t videosdk_screenShareInit(uint32_t width, uint32_t height, uint32_t fps, int32_t codec);

/// Push one NV12 screen-capture frame into the screen-share encoder.
/// Called by the main app per frame polled from the Broadcast Upload
/// Extension's shared ring buffer. Returns 0 on success, -1 on bad
/// args or a busy slot (drop this frame).
int32_t videosdk_pushScreenShareNV12(const uint8_t *y_src, uint32_t y_stride,
                                     const uint8_t *uv_src, uint32_t uv_stride,
                                     uint32_t width, uint32_t height,
                                     int64_t pts_us);

/// Stop the screen-share encoder thread. Does NOT close the RTCRtpSender;
/// use `meeting_disableScreenShare` for the full teardown.
int32_t videosdk_screenShareStop(void);

/// Enable screen share: produce the pre-built screen-share track so
/// the SFU routes a new "share" stream. Call `videosdk_screenShareInit`
/// FIRST so the track exists. Returns 0 on success.
int32_t meeting_enableScreenShare(void);

/// Disable screen share: stops the encoder and removes the producer.
int32_t meeting_disableScreenShare(void);

/// Return the active presenter's participant ID, or NULL. Caller must
/// free with `videosdk_freeString`.
char *meeting_getActivePresenterId(void);

/// Per-participant stream stats as JSON arrays. Caller must free with
/// `videosdk_freeString`.
char *participant_getAudioStats(const char *peer_id);
char *participant_getVideoStats(const char *peer_id);
char *participant_getShareStats(const char *peer_id);

// ── Media relay ─────────────────────────────────────────────────────────────

/// Request media relay from another meeting. `kinds_json` is an
/// optional JSON array of kind strings e.g. `["audio","video"]`;
/// pass NULL to relay all kinds.
int32_t meeting_requestMediaRelay(const char *meeting_id,
                                  const char *token,
                                  const char *kinds_json);

/// Stop an active media relay.
int32_t meeting_stopMediaRelay(const char *meeting_id);

/// Respond to an incoming relay request. `decision` is "accepted" or
/// "rejected".
int32_t meeting_handleRelayRequestResponse(const char *decision,
                                           const char *peer_id,
                                           const char *meeting_id);

/// Enable relayed media of a specific kind ("audio" | "video").
int32_t meeting_enableRelayMedia(const char *meeting_id, const char *kind);

/// Disable relayed media.
int32_t meeting_disableRelayMedia(const char *meeting_id, const char *kind);

// ── AI Character ────────────────────────────────────────────────────────────

/// Add a new AI character to the meeting. `config_json` is the JSON
/// representation of `CharacterConfig` (interactionId, characterId,
/// displayName, characterMode, characterRole, knowledgeBases, metaData).
/// Returns a JSON string of the server's response payload (caller frees
/// with `videosdk_free_string`), or NULL on error.
char *meeting_joinCharacter(const char *config_json);

/// Remove an AI character by interaction id.
int32_t meeting_leaveCharacter(const char *interaction_id);

/// Send a text message to an AI character.
int32_t meeting_sendCharacterMessage(const char *interaction_id, const char *text);

/// Interrupt an AI character's current response.
int32_t meeting_interruptCharacter(const char *interaction_id);

/// Publish a message to a PubSub topic. `options_json` is optional (NULL to omit).
int32_t pubsub_publish(const char *topic, const char *message, const char *options_json);

/// Subscribe to a PubSub topic. Messages arrive via event callback.
int32_t pubsub_subscribe(const char *topic);

/// Unsubscribe all listeners from a PubSub topic.
int32_t pubsub_unsubscribeAll(const char *topic);

/// Return cached pubsub messages for `topic` as a JSON array. Caller
/// must free with `videosdk_freeString`.
char *pubsub_getMessagesForTopic(const char *topic);

/* ── Image capture ─────────────────────────────────────────────────────── */

#define VIDEOSDK_CAPTURE_OK                 0
#define VIDEOSDK_CAPTURE_ERR_BAD_ID         1
#define VIDEOSDK_CAPTURE_ERR_NO_FRAME       2
#define VIDEOSDK_CAPTURE_ERR_ENCODE_FAILED  3
#define VIDEOSDK_CAPTURE_ERR_INTERNAL       4

/* Returns a NUL-terminated base64-encoded JPEG on success, NULL on error.
 * *out_err receives VIDEOSDK_CAPTURE_OK or one of the ERR_* codes.
 * Caller MUST free the returned string with videosdk_freeString exactly once.
 *
 * max_width / max_height: 0 means "native". Otherwise the output is scaled
 * to fit within both caps preserving aspect ratio, never upscaling. */
char* videosdk_captureParticipantImage(
    const char* participant_id,
    uint32_t    max_width,
    uint32_t    max_height,
    int32_t*    out_err);

/* ── SdkTracer ─────────────────────────────────────────────────────────── */

/* Open a span. Returns the new opaque span handle, or 0 on failure.
 * Pass the returned handle to videosdk_tracerEndSpan to close, or to a
 * child videosdk_tracerStartSpan / videosdk_tracerEvent call as
 * parent_handle to nest under it.
 *
 * attrs_json may be NULL or empty. parent_handle = 0 means root.
 * component: 0=Media 1=Transport 2=Network 3=Meeting 4=Participant
 *            5=InternalLog 6=Info
 * category:  0=InternalLog 1=Debug 2=Realtime 3=Telemetry 4=Warn
 *            5=Error 6=BackendError */
uint64_t videosdk_tracerStartSpan(const char *name,
                                  const char *attrs_json,
                                  uint64_t    parent_handle,
                                  int32_t     component,
                                  int32_t     category,
                                  bool        dashboard_log);

/* End a previously-opened span.
 * category: pass -1 to keep the span's own category.
 * attrs_json: NULL/empty for none.
 * message: NULL/empty defaults to "SUCCESS" on the Rust side. */
void videosdk_tracerEndSpan(uint64_t    handle,
                            int32_t     category,
                            const char *attrs_json,
                            bool        success,
                            const char *message,
                            bool        dashboard_log);

/* End the span with success=false and an error message. */
void videosdk_tracerEndSpanWithError(uint64_t    handle,
                                     const char *error_msg,
                                     const char *attrs_json);

/* Emit a point-in-time trace event. parent_handle = 0 means root. */
void videosdk_tracerEvent(const char *name,
                          int32_t     component,
                          int32_t     category,
                          const char *attrs_json,
                          uint64_t    parent_handle,
                          bool        dashboard_log);

#endif
