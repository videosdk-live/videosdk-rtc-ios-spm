#ifndef VIDEOSDK_IOS_H
#define VIDEOSDK_IOS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef void (*VideoSDKEventCallback)(const char *event_name, const char *data);

void videosdk_init(void);
void videosdk_setEventCallback(VideoSDKEventCallback callback);

/// Returns a static C string with the build stamp baked in by `build.rs` at
/// compile time. Format: `<branch>@<short-hash>[+dirty] webrtc-build@<short-hash>`.
/// Pointer is owned by the SDK; do NOT free.
const char *videosdk_get_build_id(void);
/// Core defaults used when the app passes no value. Static strings; do not free.
const char *videosdk_get_default_max_resolution(void);
const char *videosdk_get_default_mode(void);
bool videosdk_get_default_multi_stream(void);

/// Join a room.
/// Join a room. `device_info_json` is a UTF-8 JSON string for the
/// `deviceInfo` object sent on join (NULL or empty falls back to the
/// SDK default). `mode` is the participant mode string ("CONFERENCE" |
/// "VIEWER" | "SEND_AND_RECV" | "RECV_ONLY" | "SIGNALLING_ONLY"); NULL,
/// empty, or unknown falls back to "SEND_AND_RECV". The Rust core uses
/// this mode to gate produce — `RECV_ONLY`, `SIGNALLING_ONLY`, and
/// `VIEWER` skip mic/webcam production entirely even when
/// `mic_enabled` / `webcam_enabled` are true. `signaling_base_url` is an
/// optional custom signaling/proxy base URL (e.g. a dev environment); NULL
/// or empty falls back to the SDK default (`api.videosdk.live`).
/// `participant_id` pins the SFU peer id (`createRoom(participantId:)`); NULL
/// or empty makes the core mint one. Passing it is what lets a re-join replace
/// the existing server-side peer instead of adding a duplicate.
/// `extra_config_json` carries every remaining `createRoom(...)` argument that
/// has no positional parameter, as one UTF-8 JSON object; NULL, empty, or
/// unparseable is ignored and each absent key keeps its core default. Uses the
/// same key names as the Android bridge, so a new `createRoom` argument is a
/// JSON key on both platforms rather than another C-ABI change. Recognised
/// keys: `metaData` (object — participant metadata sent on `join`),
/// `preferredProtocol` ("UDP_ONLY" | "UDP_OVER_TCP" | "TCP_ONLY"),
/// `secureKey` (room-wide E2EE key; a key set via `room_setE2EEKey` wins).
/// `name` NULL = core default (random 6 chars). `multiStream` is an optional
/// bool key in `extra_config_json`; absent = core default (true).
int32_t room_join(const char *token, const char *room_id, const char *name,
                  bool mic_enabled, bool webcam_enabled,
                  const char *device_info_json, const char *mode,
                  const char *signaling_base_url, const char *participant_id,
                  const char *extra_config_json);
/// Leave the current room. Only the local participant departs; a join
/// still in flight is cancelled. Returns 0 on success, a negative catalogue
/// code on refusal (read via `videosdk_take_last_error`): 3054 once the
/// room was already left or closed, 3035 before any join.
int32_t room_leave(void);

/// End the room for all participants. Sends the `closeRoom`
/// signaling event so the server disconnects every remote participant,
/// then tears down the local session. Returns 0 on success, a negative
/// catalogue code on failure (read via `videosdk_take_last_error`):
/// 3065 reconnecting, 3035 not in the room, 3057 refused by the server,
/// 3079 no acknowledgement. A refused end leaves the local session untouched.
int32_t room_end(void);

/// Configure the room-wide shared key for end-to-end encryption.
/// Must be called BEFORE room_join so the key is in place when
/// the Rust layer builds its RoomConfig. Passing NULL clears the
/// key (disables E2EE for the next join). Returns 0 on success,
/// -1 on invalid UTF-8.
int32_t room_setE2EEKey(const char *key);

/// Set the E2EE key for a single participant (peer id). Mirrors
/// production Android's BaseKeyProvider.setKey(participantId, key).
/// Only consulted when NO shared key is set — the shared key always
/// wins. A peer without a key stays plaintext. Passing NULL for key
/// removes that participant's key. Safe to call after join: the change
/// is pushed onto the live transports. Returns 0 on success, -1 on a
/// null/invalid participantId or invalid UTF-8.
int32_t room_setE2EEParticipantKey(const char *participantId, const char *key);

/// Advance the E2EE epoch by one ratchet step. The next encrypted
/// frame carries the new kid; the previous epoch is retained by the
/// receive ring so in-flight frames still decrypt until every
/// sender has rotated. Returns 0 on success, -1 when no key is
/// configured.
int32_t room_ratchetE2EEKey(void);
/// Configure the E2EE AEAD algorithm. The wire format is selected by
/// the algorithm choice — VideoSDK variants interoperate with
/// the native-engine clients (PBKDF2 key derivation, trailer IV/key-id
/// layout, AAD-bound cleartext prefix). Rust-native variants use HKDF
/// derivation and a leading-header layout.
///   0 = VSDK_AES_GCM_128 (default, native-engine-compatible)
///   1 = VSDK_AES_GCM_256 (native-engine-compatible)
///   2 = AES_GCM_128      (Rust-native, peer must be Rust SDK)
///   3 = AES_GCM_256      (Rust-native, peer must be Rust SDK)
/// MUST be called before room_join. Returns 0 on success, -1 when
/// the value is outside the supported range.
int32_t room_setE2EEAlgorithm(int32_t algorithm);

// Audio
//
// room_enableMic / room_disableMic: typed-error path.
// Return `0` on success, a negative catalogue code on server rejection
// (e.g. -3055 for `ERROR_OPERATION_IN_PROGRESS`), or `-2` when no room
// is joined. On non-zero, read the full `{code, name, message}` triple
// via `videosdk_take_last_error` and rethrow a typed error at the
// Swift/RN boundary. The pre-existing `void`-return contract was
// upgraded to `int32_t`; the Swift wrapper may keep discarding it and
// stay source-compatible.
int32_t room_enableMic(void);
int32_t room_disableMic(void);

/// meta_json: {"permissionGranted","customTrack","hasCustomTrack"}, all optional; NULL allowed.
int32_t room_enableMicWithMeta(const char *meta_json);
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

/// Enable/disable the local webcam. Same typed-error contract as
/// `room_enableMic` / `room_disableMic`: `0` on success, a
/// negative catalogue code on failure — read the full `{code, name,
/// message}` triple via `videosdk_take_last_error`.
int32_t room_enableWebcam(void);
int32_t room_disableWebcam(void);

int32_t room_enableWebcamWithMeta(const char *meta_json);

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
char *room_getParticipants(void);
/// Returns the current session ID issued by the server (the value
/// observed in the latest `entryResponded` signaling event), or an
/// empty string if no session is established yet. Caller must free
/// with `videosdk_freeString`.
char *room_getSessionId(void);
void videosdk_freeString(char *s);

/// Consume the current thread's pending `{code, name, message}` error
/// record, if any, and return it as a JSON string. Every fallible FFI
/// method (any that now returns `int32_t`) sets this on the failure
/// path; the Swift/RN wrapper reads it to build a typed `VideoSDKError`
/// to throw. Caller must free the returned string with
/// `videosdk_freeString`. Returns NULL when the thread has no pending
/// error.
char *videosdk_take_last_error(void);

// ── B.1: Recording / HLS / Livestream / Whiteboard / Transcription ──────────
//
// All start_* methods accept optional JSON strings for the complex config
// parameters. Pass NULL (or an empty string) to omit. Return codes:
//   0  — success
//  -1  — SDK returned an error (see logs)
//  -2  — no active room

int32_t room_startRecording(const char *webhook_url,
                             const char *aws_dir_path,
                             const char *config_json,
                             const char *transcription_json);
int32_t room_stopRecording(void);

/// `outputs_json` must be a JSON array of `{"url":"...","streamKey":"..."}`.
/// `config_json` is the optional livestream config object
/// `{"layout":{...},"theme":"...","recording":{"enabled":bool}}`; pass NULL to omit.
int32_t room_startLivestream(const char *outputs_json, const char *config_json);
int32_t room_stopLivestream(void);

int32_t room_startHls(const char *config_json, const char *transcription_json);
int32_t room_stopHls(void);

int32_t room_startTranscription(const char *config_json);
int32_t room_stopTranscription(void);

/* ── Translation ───────────────────────────────────────────────────────────
 *
 * Server-side translation. `config_json` is forwarded verbatim; its shape is
 * defined by the server, not by the SDK. Call
 * videosdk_setTranslationLanguages BEFORE room_join — the languages are
 * merged into the join metaData, which is where the server reads them. */
int32_t room_setTranslationLanguages(const char *translation_language,
                                        const char *speaking_language);
int32_t room_startTranslation(const char *config_json);
int32_t room_stopTranslation(void);
int32_t room_changeTranslationLanguage(const char *language);

/// Pass NULL to use the iOS default of "v2".
int32_t room_startWhiteboard(const char *version);
int32_t room_stopWhiteboard(void);

// ── B.2: Stream pause/resume + bulk pause + adaptive subscription ──────────
//
// Single-consumer pause/resume take the server-assigned `consumerId`
// string (exposed on `Stream::consumer_id` in Rust). Bulk variants take
// the iOS `kind` string: "audio" | "video" | "share" | "all".
// Pass NULL to default to "all". Return codes match the B.1 FFI block.

int32_t stream_pause(const char *consumer_id);
int32_t stream_resume(const char *consumer_id);

int32_t room_pauseAllStreams(const char *kind);
int32_t room_resumeAllStreams(const char *kind);

int32_t room_enableAdaptiveSubscription(void);
int32_t room_disableAdaptiveSubscription(void);

// ── B.3: RealtimeStore key-value shared state ──────────────────────────────
//
// Values arrive as `realtimeStoreValueChanged` events on the existing
// `VideoSDKEventCallback`, but the server only sends them for keys
// subscribed with `realtimeStore_observe`.

/// Set a realtime-store key. Pass NULL for `value` to clear the key.
/// Return codes match the B.1/B.2 FFI block.
int32_t realtimeStore_set(const char *key, const char *value);

/// Fetch a realtime-store key's current value. Caller must free the
/// returned string with `videosdk_freeString`. Returns NULL on error or
/// when the key has no value.
char *realtimeStore_get(const char *key);

/// Subscribe to `key`'s `realtimeStoreValueChanged` events. Idempotent; the
/// core re-subscribes after a rejoin. Return codes as `realtimeStore_set`.
int32_t realtimeStore_observe(const char *key);

/// Undo `realtimeStore_observe`. Idempotent. Return codes as `realtimeStore_set`.
int32_t realtimeStore_stopObserve(const char *key);

// ── B.4: Pin / set_quality / entry response / pubsub history ───────────────

/// Request a remote participant to enable their microphone.
int32_t participant_enableMic(const char *peer_id);

/// Request a remote participant to disable their microphone.
int32_t participant_disableMic(const char *peer_id);

/// Request a remote participant to enable their webcam.
int32_t participant_enableWebcam(const char *peer_id);

/// Request a remote participant to disable their webcam.
int32_t participant_disableWebcam(const char *peer_id);

/// Remove a participant from the room.
int32_t participant_remove(const char *peer_id);

/// Pin a participant's streams. `kind` accepts "CAM" | "SHARE" |
/// "SHARE_AND_CAM" (case-insensitive); NULL defaults to SHARE_AND_CAM.
int32_t participant_pin(const char *peer_id, const char *kind);

/// Clear a participant's pin flags. Argument semantics match
/// `participant_pin`.
int32_t participant_unpin(const char *peer_id, const char *kind);

/// Snapshot of pinned participants as a JSON object
/// `{peerId: {cam, share}}`. Caller must free with `videosdk_freeString`.
char *room_getPinnedParticipants(void);

/// Request a specific forwarding quality for a participant's video
/// stream. `quality` accepts "low" | "medium" (or "med") | "high"
/// (case-insensitive); NULL defaults to "high".
int32_t participant_setQuality(const char *peer_id, const char *quality);

/// Respond to an outstanding entry request. `decision` must be
/// "allowed" or "denied".
int32_t room_respondEntry(const char *peer_id, const char *decision);

// ── Phase F: Mode / attributes / log level ──────────────────────────────────

/// Set the custom video track configuration. Call before
/// `videosdk_videoInit`. Pass a JSON string matching CameraVideoTrackConfig,
/// optionally with a `deviceId` key (recorded on the camera slot, not on the
/// core config type).
void videosdk_setVideoTrackConfig(const char *config_json);

/// Record the camera the platform actually opened — device fields only, the
/// encoder settings are untouched. `facing` is "front" or "back"; either
/// argument may be NULL.
void videosdk_setCameraDevice(const char *device_id, const char *facing);

/// Record the mic the platform actually opened. May be NULL.
void videosdk_setAudioDevice(const char *device_id);

/// The platform's opus profile name (e.g. "speech_standard"). Opaque to the
/// core; handed back in `videosdk_getMediaConfig` so a restore rebuilds the
/// same mic track.
void videosdk_setAudioEncoderProfile(const char *profile);

/// Set the audio-processing (APM) noise config for the mic track. Call before
/// `room_enableMic`. Pass a JSON string matching NoiseConfig (camelCase
/// keys: noiseSupression/noiseSuppression, echoCancellation, autoGainControl,
/// highPassFilter). Absent/null fields keep the engine default. NOTE:
/// highPassFilter is accepted but NOT applied by the native engine.
void videosdk_setNoiseConfig(const char *config_json);

/// Read back the APM flags: the ones the mic track was actually built with
/// once it exists, otherwise the ones last set. JSON
/// `{noiseSuppression, echoCancellation, autoGainControl, highPassFilter,
/// applied}`. With `applied` true a null flag is one this capture path does not
/// report: the pushed path nulls `echoCancellation`, having no far-end
/// reference, and the device path nulls `highPassFilter`. Caller must free with
/// `videosdk_freeString`.
/// Publishable video codecs as a JSON array of names, "[]" before join.
/// Caller owns the string (free it).
char *videosdk_getAvailableVideoCodecs(void);

/// The core's media-config snapshot: `{mic, webcam, camera, audio}`, with
/// `camera` / `audio` null until seeded. Rebuild capture from this rather than
/// from any Swift-side cache. Caller owns the string (free it).
char *videosdk_getMediaConfig(void);

/// Negotiated codec of the local webcam producer, or NULL when the webcam is
/// off. Caller owns the string (free it).
char *videosdk_getActiveVideoCodec(void);

char *videosdk_getNoiseConfig(void);

// ── Phase 1 measurement harness ─────────────────────────────────────────────
// docs/design/effects-core/effects-in-core-design-2026-09-08.md §4.7, §6 "Phase 1".

/// One-second `EffectsStats` snapshot as JSON. Caller frees it with
/// `videosdk_freeString`. The live row arrives once a second on the
/// `videoEffectsStats` event instead.
char *videosdk_effectsStatsSnapshot(void);

/// Push what only the platform can read: `ProcessInfo.thermalState` mapped to
/// the Android `PowerManager.THERMAL_STATUS_*` scale (nominal 0, fair 1,
/// serious 3 = SEVERE, critical 4 = CRITICAL), and whole-process CPU percent.
/// Pass a negative `headroom` — iOS has no `getThermalHeadroom()` equivalent.
void videosdk_telemetryPushDeviceLoad(int32_t thermal, float headroom, float cpu_pct);

/// Turn the `videosdk.effects.stats` harness on or off. OFF is the shipped
/// default; turning it on starts the 1 Hz aggregator. The env variable
/// `VIDEOSDK_EFFECTS_STATS=1` does the same at init.
void videosdk_setEffectsStatsEnabled(bool enabled);

/// Is the harness on? Ask before starting the device-load pump.
bool videosdk_effectsStatsEnabled(void);

/// Debug-only: force a drop counter so a rig can prove a non-zero count reaches
/// the `fx_stats` row. Inert unless the harness flag is on. `kind`: 0
/// halSkipped, 1 graphBackpressure, 2 readbackStall, 3 encoderDropped,
/// 4 geometryChanges.
void videosdk_effectsStatsInjectDrop(int32_t kind, int32_t count);

/// Ask for a video encoder backend: 0 auto (default), 1 software, 2 hardware,
/// 3 videotoolbox. Takes effect on the next produce.
void videosdk_setVideoEncoderBackend(int32_t backend);

// ── Phase 2: the effects graph ──────────────────────────────────────────────
// docs/design/effects-core/effects-in-core-design-2026-09-08.md §4.7.
//
// JSON in, JSON out. Adding an effect adds a key to the config document, never
// a function here.

/// Initialise or reconfigure the graph from an `EffectsConfig` document.
/// Returns the APPLIED config as JSON (adds `applied`, `tier`, `degradeLevel`,
/// `reason`). Caller frees with `videosdk_freeString`.
/// The default is `"pipeline":"LEGACY"` — the capture path is unchanged unless
/// a document asks for `GRAPH`.
char *videosdk_setVideoEffects(const char *config_json);

/// Design §4.7's name for the same call; `w`/`h` are accepted and ignored (the
/// graph follows the capturer's geometry).
char *videosdk_effectsInit(const char *config_json, int32_t w, int32_t h);

/// The config in force, as JSON. Caller frees with `videosdk_freeString`.
/// Publish a subject box for auto-framing, normalised 0..1 against the frame the
/// effects see. A `w` or `h` of zero or less means "no subject", which starts
/// the framer's hold and then eases the crop home.
///
/// Until a real face detector lands this is the only thing that moves the
/// framing crop, so framing switched on with nothing publishing is a clean
/// no-op. Wait-free; safe from any thread.
void videosdk_effectsPublishFaceBox(float cx, float cy, float w, float h);

char *videosdk_effectsGetAppliedConfig(void);

/// Tear the graph down and return the capture path to `LEGACY`.
void videosdk_effectsShutdown(void);

/// Is the graph handling capture frames right now? Not the same question as
/// "was GRAPH asked for": a device whose runtime failed to create reports
/// false here and keeps publishing down the legacy path.
bool videosdk_effectsGraphEnabled(void);

/// Register the self-view sink. The callback receives a RETAINED (+1)
/// CVPixelBufferRef and the frame's pts; the receiver owns it and must
/// CFRelease it. Pass NULL to detach.
void videosdk_effectsSetPreviewCallback(void (*cb)(void *pixel_buffer, int64_t pts_us));

/// Push one camera CVPixelBuffer into the graph. The buffer is BORROWED — this
/// call does not retain it. Returns 0 when a frame reached the encoder, -1
/// otherwise (already counted at the right drop site).
int32_t videosdk_pushVideoCaptureCVPixelBuffer(void *pixel_buffer, uint32_t width,
                                               uint32_t height, int64_t pts_us);

/// Set the opus audio encoder config for the mic track. `bitrate_kbps` 0 or
/// >128 means "no override". The `*_valid` flags gate whether the paired
/// DTX/FEC/stereo value is applied. `stereo_on` maps to the `opusStereo`
/// codec option prod sends for the `*_stereo` audio profiles.
/// `max_playback_rate_hz` / `ptime_ms` are prod's `opusMaxPlaybackRate` and
/// `opusPtime` from the same profile row; 0 means "keep the opus default".
void videosdk_setAudioEncoderConfig(uint32_t bitrate_kbps,
                                    int32_t dtx_valid,
                                    int32_t dtx_on,
                                    int32_t fec_valid,
                                    int32_t fec_on,
                                    int32_t stereo_valid,
                                    int32_t stereo_on,
                                    uint32_t max_playback_rate_hz,
                                    uint32_t ptime_ms);

/// Enable/disable the client-side codec-switch fallback (downgrade the local
/// camera to VP8 when the server reports a remote that cannot decode the
/// current codec). Default true. Call before `room_join`. Client-side only —
/// never sent to the server.
void videosdk_setCodecSwitchEnabled(bool enabled);

/// Route the engine's own log to stderr at informational level, so its audio
/// configuration and failures reach the platform console. Off by default and
/// for diagnosis only: it also logs candidate addresses and session details.
void videosdk_setEngineLogToStderr(bool enabled);

/// Declare that this platform can hand the audio device to the engine, letting
/// capture and playout share one unit so the canceller has a far-end reference.
/// Call before `room_enableMic`; the iOS wrapper declares it at first use.
///
/// A capability, not the user's choice — cancellation is asked for via
/// `echoCancellation` in the noise config, and both must hold. The handover is
/// decided per mic track, so keep your own audio engines available and stand
/// them down only while `videosdk_platformAudioActive` reports true.
void videosdk_setPlatformAudioEnabled(bool enabled);

/// Whether the live mic track holds the platform audio device, and so renders
/// the far end itself. The one source of truth for who owns playout.
///
/// False whenever no mic track exists — before the first publish, while muted in
/// a mode that cannot produce, after a leave, and after a mode change replaced
/// the media engine. Poll it on every media transition and run your own player
/// whenever it reads false.
bool videosdk_platformAudioActive(void);

/// Enable/disable server-side notification batching. Default true. Call before
/// `room_join`. Sent to the server on `join` as `batchingEnabled`; when the
/// server batches participant events into `batchNotification` envelopes, the
/// SDK unpacks and dispatches each inner entry through the normal path.
void videosdk_setBatchingEnabled(bool enabled);

/// Opt in to server-driven webcam layer caps (DynaCast). Call before room_join.
void videosdk_setDynaCast(bool enabled);

/// Set the SDK log level. Call before `room_join`. `level` accepts
/// "none" | "error" | "warn" | "info" | "debug" | "all".
void videosdk_setLogLevel(const char *level);

/// Change the local participant's mode mid-room. `mode` accepts
/// "CONFERENCE" | "VIEWER" | "SEND_AND_RECV" | "RECV_ONLY" | "SIGNALLING_ONLY".
int32_t room_changeMode(const char *mode);

/// Whether the live participant mode permits producing media. Ask BEFORE
/// starting camera/mic capture — the core's produce-lock blocks transmission
/// only, so capturing under RECV_ONLY/SIGNALLING_ONLY is a silent privacy leak.
/// Returns true when no room exists yet (pre-join intents still queue).
bool room_canProduce(void);
int32_t room_assertPublishAllowed(const char *method);

/// Re-publish the camera after a mode-up, a rejoin or a room switch, using the
/// core's stored config. Call from the `restoreLocalMedia` event after opening
/// the device. 0 = producing, 1 = the user's intent is gone so stop capture
/// again, negative = error code.
int32_t room_restoreWebcam(void);

/// Mic twin of `room_restoreWebcam`.
int32_t room_restoreMic(void);

int32_t room_restoreWebcamWithMeta(const char *meta_json);
int32_t room_restoreMicWithMeta(const char *meta_json);

/// Switch the active session to a different room without
/// destroying the local Room object. The server validates the
/// request, replies with `entryResponded` carrying `switchingRoomId`,
/// and the SDK soft-closes the current room and re-joins the new one.
/// `mode` accepts the same values as `room_changeMode`.
int32_t room_switchTo(const char *room_id, const char *token, const char *mode);

/// Reset the mic + video capture tracks so the next call to
/// `videosdk_videoInit` and `room_enableMic` installs fresh ones.
/// Required after `room_switchTo` because the old tracks remain
/// bound to the previous room's peer connection — re-using them
/// silently deadlocks `add_track` on the new room's PC.
void videosdk_resetMediaTracks(void);

/// Store attributes on the room.
int32_t room_setAttributes(const char *name, const char *version);

// ── Screen share + stats (Phase D) ──────────────────────────────────────────

/// Initialise the screen-share encoder. Call BEFORE the first
/// `videosdk_pushScreenShareNV12` and before `room_enableScreenShare`.
/// `codec`: 0 = VP8, 1 = H264. `fps` 0 means "use the frame rate of the
/// profile set by `videosdk_screenShareSetEncodingProfile`". Screen content is a
/// single layer unless that profile asked for `multi_stream`, in which case
/// the producer publishes three. Returns 0 on success.
int32_t videosdk_screenShareInit(uint32_t width, uint32_t height, uint32_t fps, int32_t codec);

/// Set the next share producer's encoding from a screen-share profile NAME.
///
/// The profile table lives in the core, so a platform sends only the name it
/// was given ("h720p_15fps", …) and never its own copy of the numbers. NULL or
/// empty uses the default profile. Call BEFORE `videosdk_screenShareInit`.
///
/// `multi_stream != 0` publishes three simulcast layers built from the
/// profile's three bitrate tiers (S3T3/S2T3/S1T3); otherwise one encoding
/// capped at the top tier.
///
/// Returns 0, -1 for an unknown profile name (the platforms' 3071
/// ERROR_INVALID_CUSTOM_TRACK_CONFIG), -2 with no room.
int32_t videosdk_screenShareSetEncodingProfile(const char *profile_name, int32_t multi_stream);

/// The screen-share profile table as JSON. Caller frees with
/// `videosdk_freeString`.
char *videosdk_getScreenShareProfiles(void);

/// Push one NV12 screen-capture frame into the screen-share encoder.
/// Called by the main app for each frame it receives from the Broadcast
/// Upload Extension. Returns 0 on success, -1 on bad args or a busy slot
/// (drop this frame).
int32_t videosdk_pushScreenShareNV12(const uint8_t *y_src, uint32_t y_stride,
                                     const uint8_t *uv_src, uint32_t uv_stride,
                                     uint32_t width, uint32_t height,
                                     int64_t pts_us);

/// Stop the screen-share encoder thread. Does NOT close the RTCRtpSender;
/// use `room_disableScreenShare` for the full teardown.
int32_t videosdk_screenShareStop(void);

/// Enable screen share: produce the pre-built screen-share track so
/// the SFU routes a new "share" stream. Call `videosdk_screenShareInit`
/// FIRST so the track exists. Returns 0 on success.
int32_t room_enableScreenShare(void);

int32_t room_enableScreenShareWithMeta(const char *meta_json);

/// Borrowed handle to a live core span (0 join, 1 entry, 2 send, 3 recv): release, never end.
uint64_t videosdk_tracerCoreSpan(int32_t scope);
uint64_t videosdk_tracerPeerSpan(const char *peer_id);
uint64_t videosdk_tracerConsumerSpan(const char *consumer_id);
void     videosdk_tracerReleaseSpan(uint64_t handle);

/// Disable screen share: stops the encoder and removes the producer.
int32_t room_disableScreenShare(void);

/// Return the active presenter's participant ID, or NULL. Caller must
/// free with `videosdk_freeString`.
char *room_getActivePresenterId(void);

/// Per-participant stream stats as JSON arrays. Caller must free with
/// `videosdk_freeString`.
char *participant_getAudioStats(const char *peer_id);
char *participant_getVideoStats(const char *peer_id);
char *participant_getShareStats(const char *peer_id);

// ── Cross-SDK parity: capabilities the browser could reach and iOS could not ─

/// Per-track stats for the audio riding along with a screen share, excluding
/// the mic (JSON array). Caller must free with `videosdk_freeString`.
char *participant_getShareAudioStats(const char *peer_id);

/// ICE/DTLS transport stats for one peer (JSON object). Caller must free with
/// `videosdk_freeString`.
char *participant_getTransportStats(const char *peer_id);

/// Rendered viewport (px) for a remote participant's video, so the SFU forwards
/// an appropriately-scaled simulcast layer.
int32_t participant_setViewPort(const char *peer_id, int32_t width, int32_t height);

/// The screen-share counterpart of `participant_setQuality` — re-layers the
/// peer's SHARE stream rather than its camera. `quality` accepts
/// "low" | "medium" (or "med") | "high"; NULL defaults to "high".
int32_t participant_setScreenShareQuality(const char *peer_id, const char *quality);

/// Start / stop consuming one peer's audio or video. The per-peer counterparts
/// of `room_pauseAllStreams` — adaptive-subscription apps use these to drop
/// off-screen peers entirely rather than merely pausing them.
int32_t participant_consumeMicStreams(const char *peer_id);
int32_t participant_consumeWebcamStreams(const char *peer_id);
int32_t participant_stopConsumingMicStreams(const char *peer_id);
int32_t participant_stopConsumingWebcamStreams(const char *peer_id);

/// Manual ICE restart — `room.refreshConnection()`. For a session whose
/// media has stalled but whose signaling is still up.
int32_t room_refreshConnection(void);

/// The device has a usable network again — retry a reconnect now. Never blocks;
/// a healthy session ignores it.
int32_t room_notifyNetworkAvailable(void);

/// The device has no network: rejoin attempts that fail until it returns don't count.
int32_t room_notifyNetworkLost(void);

/// Cap the live camera at `layer`, the 0-indexed simulcast rung the caller
/// resolved from its own quality table.
int32_t room_setWebcamQuality(int32_t layer);

/// Data-channel send. Text and binary are distinct frame types on the wire:
/// pass `text` for a text frame, or `bytes`/`len` for a binary one. Exactly one
/// of the two must be supplied.
int32_t room_send(const char *text, const uint8_t *bytes, size_t len, bool reliable);

/// The room's custom attributes as a JSON object — the read side of
/// `room_setAttributes`. Caller must free with `videosdk_freeString`.
char *room_getAttributes(void);

// ── Media relay ─────────────────────────────────────────────────────────────

/// Request media relay from another room. `kinds_json` is an
/// optional JSON array of kind strings e.g. `["audio","video"]`;
/// pass NULL to relay all kinds.
int32_t room_requestMediaRelay(const char *room_id,
                                  const char *token,
                                  const char *kinds_json);

/// Stop an active media relay.
int32_t room_stopMediaRelay(const char *room_id);

/// Respond to an incoming relay request. `decision` is "accepted" or
/// "rejected".
int32_t room_handleRelayRequestResponse(const char *decision,
                                           const char *peer_id,
                                           const char *room_id);

/// Enable relayed media of a specific kind ("audio" | "video").
int32_t room_enableRelayMedia(const char *room_id, const char *kind);

/// Disable relayed media.
int32_t room_disableRelayMedia(const char *room_id, const char *kind);

/// Publish a message to a PubSub topic. `options_json` and `payload_json` are
/// optional (NULL to omit). `payload_json` is a JSON object carried alongside
/// the message; the server never parses it.
int32_t pubsub_publish(const char *topic, const char *message, const char *options_json,
                       const char *payload_json);

/// Subscribe to a PubSub topic. Messages arrive via event callback.
int32_t pubsub_subscribe(const char *topic);

/// Subscribe to a PubSub topic, bounding how much history is replayed and
/// how live messages are paced. `options_json` is a `PubSubSubscribeOptions`
/// object (NULL or blank behaves exactly like `pubsub_subscribe`); invalid
/// options are rejected with a non-zero return, never sanitized.
/// `subscription_id` identifies the single listener this subscribe is for
/// and is echoed as `subscriptionId` on every `pubsubOldMessages` event, so
/// a replay reaches only the listener that asked for it (NULL to let the
/// core derive one).
/// Returns 0 on success, -3 when the pubsub socket is momentarily
/// unavailable ("Operation Timeout."), -2 with no active room, -1 otherwise.
int32_t pubsub_subscribeWithOptions(const char *topic, const char *options_json,
                                    const char *subscription_id);

/// Unsubscribe all listeners from a PubSub topic.
int32_t pubsub_unsubscribeAll(const char *topic);

/// Non-blocking probe: 1 while the dedicated pubsub socket is between an
/// unexpected close and its reconnect (or torn down), 0 otherwise. Lets the
/// Swift layer refuse a pubsub call at the call site instead of waiting out
/// the request timeout.
int32_t pubsub_isTransportUnavailable(void);

/// Return cached pubsub messages for `topic` as a JSON array. Caller
/// must free with `videosdk_freeString`.
char *pubsub_getMessagesForTopic(const char *topic);

// ── Base64 file upload / fetch ──────────────────────────────────────────────

/// Upload a base64-encoded file to VideoSDK temporary storage. Returns
/// the public URL of the stored file as a C string (caller frees with
/// `videosdk_freeString`), or NULL on error / no active room.
char *room_uploadBase64File(const char *base64_data,
                               const char *token,
                               const char *file_name);

/// Fetch a previously uploaded file from `url` and return it as a
/// base64-encoded C string (caller frees with `videosdk_freeString`),
/// or NULL on error / no active room.
char *room_fetchBase64File(const char *url, const char *token);

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
 * component: 0=Media 1=Transport 2=Network 3=Room 4=Participant
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

/* ── Direct routing — SdkTracer::to(mask) ──────────────────────────────────
 *
 * `target_mask` is a bitmask of OutputTarget bits, which are wire-stable and
 * must match `videosdk::stats::tracer::OutputTarget` in the core (and the
 * Kotlin/Swift `OutputTarget` mirrors) exactly:
 *
 *   TO_LOGCAT 1 | TO_SIGNOZ 2 | TO_INSPECTOR 4
 *   TO_CLIENT_LOGCAT 8 | TO_BACKEND 16 | TO_TELEMETRY 32
 *
 * A mask of 0 means "route by category", i.e. identical to the unrouted call
 * above. These symbols have always been exported by the core; they were simply
 * never declared here, which is what made `.to(...)` unreachable from Swift.
 */
uint64_t videosdk_tracerStartSpanTo(int32_t     target_mask,
                                    const char *name,
                                    const char *attrs_json,
                                    uint64_t    parent_handle,
                                    int32_t     component,
                                    int32_t     category);

void videosdk_tracerEventTo(int32_t     target_mask,
                            const char *name,
                            int32_t     component,
                            int32_t     category,
                            const char *attrs_json,
                            bool        dashboard_log);

/* Level convenience methods, routed to `target_mask`. */
void videosdk_tracerErrorTo(int32_t target_mask, const char *msg,
                            int32_t component, const char *attrs_json,
                            bool dashboard_log);
void videosdk_tracerBackendErrorTo(int32_t target_mask, const char *msg,
                                   int32_t component, const char *attrs_json,
                                   bool dashboard_log);
void videosdk_tracerWarnTo(int32_t target_mask, const char *msg,
                           int32_t component, const char *attrs_json,
                           bool dashboard_log);
void videosdk_tracerInfoTo(int32_t target_mask, const char *msg,
                           int32_t component, const char *attrs_json,
                           bool dashboard_log);
void videosdk_tracerDebugTo(int32_t target_mask, const char *msg,
                            int32_t component, const char *attrs_json,
                            bool dashboard_log);
void videosdk_tracerInternalLogTo(int32_t target_mask, const char *msg,
                                  int32_t component, const char *attrs_json,
                                  bool dashboard_log);

/* ── Inspector buffer ──────────────────────────────────────────────────────
 *
 * The debug-UI event buffer lives in the shared core, so iOS and Android read
 * identical events, span depth and counts; only the UI is per-platform.
 */

/* Called once per newly buffered event, with the same JSON envelope that
 * videosdk_inspectorSnapshotJson returns elements of. A refresh nudge only —
 * the event is already stored in the core. Do not free the pointer; it is
 * only valid for the duration of the call. */
typedef void (*VideoSDKInspectorEventCallback)(const char *json);

/* Register before enabling; only the first registration is kept. */
void videosdk_setInspectorCallback(VideoSDKInspectorEventCallback callback);

/* Start/stop buffering. Both idempotent. */
/// Forward the SDK's client-facing log lines to the event callback set with
/// `videosdk_setEventCallback`, as event `logMessage` with payload
/// `{"level":"error|warn|info|debug","message":"VideoSDK: ..."}`.
///
/// On iOS the stock console output writes to stdout, which is invisible to
/// os_log, Console.app and any embedding host's console (Metro, `flutter run`)
/// — only Xcode sees it. This is how a host surfaces those lines itself.
///
/// Idempotent. Filtering follows `videosdk_setLogLevel`; there is no second
/// level to keep in step. The callback owns both strings and must free them
/// with `videosdk_freeString`, as for every other event.
void videosdk_tracerEnableConsoleForwarding(void);

/// Stop forwarding log lines. Idempotent.
void videosdk_tracerDisableConsoleForwarding(void);

void videosdk_inspectorEnable(void);
void videosdk_inspectorDisable(void);

/* JSON array of buffered events, "[]" when off.
 * Free with videosdk_freeString. */
char *videosdk_inspectorSnapshotJson(void);

/* {events, errors, warnings, slowOps, components[]}, zeros when off.
 * Free with videosdk_freeString. */
char *videosdk_inspectorCountsJson(void);

void videosdk_inspectorClear(void);

/* Ring size; the core clamps to its supported range (50..2000). */
void    videosdk_inspectorSetCapacity(int32_t capacity);
int32_t videosdk_inspectorCapacity(void);

/* ── Pre-call test ─────────────────────────────────────────────────────────
 * Run a pre-call test to completion. Blocks the calling thread; call it off
 * the main thread. Returns {"ok":true,"result":…} or
 * {"ok":false,"error":{code,name,message}}. Free with videosdk_freeString.
 * While it runs the event callback receives "precall-open-media",
 * "precall-release-media", "precall-provided-media" and "precall-stats". */
char   *videosdk_precallRun(const char *options_json);

/* Stop the running pre-call test; waits at most timeout_ms (no cap if < 0). */
void    videosdk_precallStop(int64_t timeout_ms);

/* Answer a "precall-open-media" request with its requestId and a JSON reply. */
void    videosdk_precallMediaOpened(uint64_t request_id, const char *reply_json);

/* VideoSDK.createRoom created a room (true) or it was left (false).
 * A pre-call test is refused in between. */
void    videosdk_precallSetRoomInitialized(bool initialized);

#endif
