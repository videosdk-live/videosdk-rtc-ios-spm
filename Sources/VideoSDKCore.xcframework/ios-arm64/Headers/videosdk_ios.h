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

// Audio

uint32_t videosdk_pushAudioCapture(const int16_t *samples, uint32_t count);
/// PCM for audio lane 1 (screen audio), 2 (custom audio) or 3 (the mix, summed into the mic).
/// The caller owns `samples`. Returns the samples taken, 0 for a bad lane.
uint32_t videosdk_pushAudioLane(uint32_t lane, const int16_t *samples, uint32_t count,
                                uint32_t sample_rate, uint32_t channels);
uint32_t videosdk_pullAudioPlayback(int16_t *out_samples, uint32_t count);
/// One person's audio tap for an audio sink: source "microphone", "screen_audio" or "custom".
/// Writes up to count samples (48 kHz mono) into out_samples, which the caller owns; returns the
/// samples written, 0 with no tap or no data. Any thread; never blocks.
uint32_t videosdk_pullRemoteAudio(const char *participant_id, const char *source,
                                  int16_t *out_samples, uint32_t count);
uint32_t videosdk_audioPlaybackAvailable(void);

// Video capture
int32_t videosdk_videoInit(uint32_t width, uint32_t height, uint32_t fps, int32_t codec);

/// Push NV12 camera frame directly via plane pointers (zero intermediate buffers).
int32_t videosdk_pushVideoCaptureNV12(const uint8_t *y_src, uint32_t y_stride,
                                     const uint8_t *uv_src, uint32_t uv_stride,
                                     uint32_t width, uint32_t height,
                                     int64_t pts_us);

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

/// Screen-share sibling of videosdk_pollVideoPlaybackNV12: same contract,
/// reads the dedicated screen-share slot.
int32_t videosdk_pollScreenSharePlaybackNV12(const char *participant_id,
                                        uint8_t *y_dst, uint32_t y_stride,
                                        uint8_t *uv_dst, uint32_t uv_stride,
                                        uint32_t buf_width, uint32_t buf_height,
                                        uint32_t *out_width, uint32_t *out_height);

/// 1 when the participant's camera slot (screen-share slot with
/// is_share) holds a frame no poll has taken yet, else 0, including a
/// NULL or non-UTF-8 id. Takes nothing; any thread. A check on the
/// local camera counts as a poll, so the local slot keeps filling.
int32_t videosdk_videoFrameReady(const char *participant_id, bool is_share);

/// Runs one new-DX method (`me.join`, `room.leave`, `events.on`, ...) with its JSON arguments
/// and returns the JSON envelope: `{"ok":true,"result":{...}}` or
/// `{"ok":false,"error":{"code","number","area","retriable","message"}}`. Never NULL; the
/// caller frees it with `videosdk_freeString`. Blocking, safe from any thread.
char *videosdk_call(const char *method, const char *args_json);
void videosdk_freeString(char *s);

// ── Phase 2: the effects graph ──────────────────────────────────────────────
// docs/design/effects-core/effects-in-core-design-2026-09-08.md §4.7.

/// Is the graph handling capture frames right now? Not the same question as
/// "was GRAPH asked for": a device whose runtime failed to create reports
/// false here and keeps publishing down the legacy path.
bool videosdk_effectsGraphEnabled(void);

/// Register the self-view sink. The callback receives a RETAINED (+1)
/// CVPixelBufferRef and the frame's pts; the receiver owns it and must
/// CFRelease it. Both the effects graph and videosdk_pushVideoCaptureNative
/// feed it. Pass NULL to detach.
void videosdk_effectsSetPreviewCallback(void (*cb)(void *pixel_buffer, int64_t pts_us));

/// Register a sink for one remote track's frames: kind 0 camera, 1 screen
/// share, 2 custom video. The decoder's worker thread calls cb(ctx, pixel_buffer)
/// once per decoded frame with a RETAINED (+1) NV12 '420v' CVPixelBufferRef
/// from a core-owned pool; the receiver owns it and must CFRelease it. cb
/// must return quickly and must not call this function. Pass NULL cb to
/// clear: when that returns no call with the old ctx is in flight and none
/// follows. While a sink is registered the track's playback slot is not
/// written. Returns 0, or -1 for a NULL or non-UTF-8 participant_id or a kind outside 0 to 2.
int32_t videosdk_setRemoteVideoCallback(const char *participant_id, int32_t kind,
                                        void (*cb)(void *ctx, void *pixel_buffer),
                                        void *ctx);

/// Push one camera CVPixelBuffer into the graph. The buffer is BORROWED — this
/// call does not retain it. Returns 0 when a frame reached the encoder, -1
/// otherwise (already counted at the right drop site).
int32_t videosdk_pushVideoCaptureCVPixelBuffer(void *pixel_buffer, uint32_t width,
                                               uint32_t height, int64_t pts_us);

/// Push one NV12 or BGRA CVPixelBuffer onto video lane `lane` (2, the custom video).
/// The buffer is BORROWED: the caller keeps it. Returns 0 taken, -1 refused.
int32_t videosdk_pushVideoLaneCVPixelBuffer(uint32_t lane, void *pixel_buffer, int64_t pts_us);

/// Publish one camera CVPixelBuffer natively at `width`x`height`, centre-cropped
/// and scaled when its size differs. The buffer is BORROWED: the core retains
/// what it publishes. Returns 0 pushed, -1 dropped, -3 when native capture is
/// off and nothing was touched. A -1 is counted as `encoderDropped`, except for
/// a NULL buffer or a zero size.
int32_t videosdk_pushVideoCaptureNative(void *pixel_buffer, uint32_t width, uint32_t height,
                                        int64_t pts_us);

/// Turn the native camera publish on or off for the process. Off by default.
void videosdk_setNativeCaptureEnabled(bool enabled);

/// Whether the native camera publish is on.
bool videosdk_nativeCaptureEnabled(void);

/// Route the engine's own log to stderr at informational level, so its audio
/// configuration and failures reach the platform console. Off by default and
/// for diagnosis only: it also logs candidate addresses and session details.
void videosdk_setEngineLogToStderr(bool enabled);

/// Declare that this platform can hand the audio device to the engine, letting
/// capture and playout share one unit so the canceller has a far-end reference.
/// Call before `me.setMic`; the iOS wrapper declares it at first use.
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

/// Set the SDK log level. Call before `me.join`. `level` accepts
/// "none" | "error" | "warn" | "info" | "debug" | "all".
void videosdk_setLogLevel(const char *level);

// ── Screen share + stats (Phase D) ──────────────────────────────────────────

/// Record the screen capture's geometry; the core builds the share track and
/// starts its encoder when `me.setScreen` produces. Call on the first frame,
/// BEFORE `me.setScreen`.
/// `codec`: 0 = VP8, 1 = H264. `fps` 0 means "use the share profile's frame
/// rate". Screen content is a single layer unless that profile asked for
/// `multi_stream`, in which case the producer publishes three. Returns 0 on
/// success.
int32_t videosdk_screenShareInit(uint32_t width, uint32_t height, uint32_t fps, int32_t codec);

/// Push one NV12 screen-capture frame into the screen-share encoder.
/// Called by the main app for each frame it receives from the Broadcast
/// Upload Extension. Returns 0 on success, -1 on bad args or a busy slot
/// (drop this frame).
int32_t videosdk_pushScreenShareNV12(const uint8_t *y_src, uint32_t y_stride,
                                     const uint8_t *uv_src, uint32_t uv_stride,
                                     uint32_t width, uint32_t height,
                                     int64_t pts_us);

/// Stop the screen-share encoder thread. Does NOT close the RTCRtpSender
/// or the screen-share producer.
int32_t videosdk_screenShareStop(void);

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

#endif
