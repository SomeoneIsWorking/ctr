#pragma once

#include <cstdint>

namespace ctr::native {

// Identity-gated SCUS_944.26 ownership facts. Independent oracle comparison currently ends before
// the initializer body; static RE additionally grounds the title-owned repeating frame transition.
inline constexpr uint32_t kExecutableEntry = 0x8007793Cu;
inline constexpr uint32_t kGuestMain = 0x8003C58Cu;
inline constexpr uint32_t kLoopTop = 0x8003C5D0u;
inline constexpr uint32_t kInitializerEvidenceFrontier = 0x800772E0u;

// Exact retail calls which precede guest VSync and their post-VSync continuations. The title driver
// preserves each generated callee as a super, then resumes after the forbidden timing primitive.
inline constexpr uint32_t kStartupGpuInit = 0x8003D7D8u;
inline constexpr uint32_t kStartupGpuInitReturn = 0x8003C7D8u;
inline constexpr uint32_t kAfterFirstStartupVSync = 0x8003C7E0u;
inline constexpr uint32_t kStartupDisplayInit = 0x800251ACu;
inline constexpr uint32_t kStartupDisplayInitReturn = 0x8003C7ECu;
inline constexpr uint32_t kAfterSecondStartupVSync = 0x8003C7F4u;

// State-zero loads two startup resources through 0x80031FDC with its fifth argument set to -1.
// That exact branch performs synchronous setup and then calls VSync(2). The title owner preserves
// the prefix, waits two native fields, and resumes at the post-call instruction without entering
// libetc. Both measured callers are admitted explicitly when the preserved suffix returns.
inline constexpr uint32_t kBootResourceWait = 0x80031FDCu;
inline constexpr uint32_t kBootResourceWaitReturn = 0x80032074u;
inline constexpr uint32_t kBootResourceWaitFirstCaller = 0x8003C8D4u;
inline constexpr uint32_t kBootResourceWaitSecondCaller = 0x8003C984u;
inline constexpr uint32_t kBootResourceWaitRaceCaller = 0x800336F8u;
// The race/menu continuation is an interior suffix of 0x80033610. Unlike the two state-zero
// continuations above, it restores that function's saved frame and returns to its sole direct caller.
inline constexpr uint32_t kBootResourceWaitRaceResume = 0x8003CC98u;
// State-zero's first resource continuation enters 0x8002DD24, whose two retail do/while loops poll
// asynchronous CD/GPU stages without returning to the host. The title driver retains the generated
// stage functions but yields a host field after each false poll, then resumes the exact caller suffix.
inline constexpr uint32_t kBootResourcePump = 0x8002DD24u;
inline constexpr uint32_t kBootResourcePumpReturn = 0x8003C8FCu;
inline constexpr uint32_t kBootResourcePumpBegin = 0x800297A0u;
inline constexpr uint32_t kBootResourcePumpPoll = 0x800293B8u;
inline constexpr uint32_t kBootResourcePumpCommit = 0x80029C40u;
inline constexpr uint32_t kBootResourcePumpCommitPoll = 0x80029CA4u;
// After loading the startup sound archive, state-zero waits for its XA task at 0x8008D708. The
// retail loop calls this service function from one exact site; the native owner preserves each
// service call, advances one host audio field, and resumes at the post-call poll.
inline constexpr uint32_t kStartupAudioService = 0x8001D06Cu;
inline constexpr uint32_t kStartupAudioLoop = 0x8003C94Cu;
inline constexpr uint32_t kStartupAudioServiceReturn = kStartupAudioLoop;
inline constexpr uint32_t kStartupAudioWaitState = 0x8008D708u;
// libapi's seven-entry DMA callback table is initialized at 0x8008CB08. Slot 4 is the SPU channel
// and holds libspu's retail completion wrapper (0x8007AB34 once sound initialization finishes).
inline constexpr int kSpuDmaChannel = 4;
inline constexpr uint32_t kDmaCallbackTable = 0x8008CB08u;
inline constexpr uint32_t kSpuDmaCallbackSlot = kDmaCallbackTable + 4u * kSpuDmaChannel;
inline constexpr uint32_t kShutdownDisplay = 0x80025208u;
inline constexpr uint32_t kShutdownDisplayReturn = 0x8003CF30u;
inline constexpr uint32_t kAfterShutdownVSync = 0x8003CF38u;

// State 3 calls the frame/presentation owner once per iteration. Its generated prefix reaches the
// unconditional timing calculation below; the native bridge skips only conditional debug VSync(0)
// and enters the generated suffix, which restores the frame and returns to kFrameLoopResume.
inline constexpr uint32_t kFrameOwner = 0x80035E70u;
inline constexpr uint32_t kFrameTiming = 0x8004B3A4u;
inline constexpr uint32_t kFrameTimingReturn = 0x8003785Cu;
// The timing leaf's only other direct caller is a plain elapsed-time query. It returns through the
// generated wrapper and has no adjacent VSync to omit.
inline constexpr uint32_t kFrameTimingQueryReturn = 0x8004B438u;
inline constexpr uint32_t kFrameSuffix = 0x80037880u;
inline constexpr uint32_t kFrameLoopResume = 0x8003CEB4u;
// The suffix waits for the DrawSync callback's byte and for the VSync callback's two-field
// countdown. Both callbacks are registered by the retained retail APIs and delivered by the native
// field owner; the generated suffix remains the authority once this exact predicate becomes false.
inline constexpr uint32_t kVblankCallbackInstall = 0x80077254u;
inline constexpr uint32_t kDrawSyncCallbackSlot = 0x8008AD8Cu;
inline constexpr uint32_t kGameStateGpOffset = 832u;
inline constexpr uint32_t kDrawSyncPendingOffset = 7472u;
inline constexpr uint32_t kFrameCallbackCountOffset = 7392u;
inline constexpr uint32_t kFrameWaitFieldsGpOffset = 840u;

// Dynamic view projection publication. All three direct callsites are identity-gated by CTR-05;
// their return addresses are the only admitted entries to the title projection owner.
inline constexpr uint32_t kProjectionProducer = 0x80042910u;
inline constexpr uint32_t kProjectionReturnLensflare = 0x80024CD4u;
inline constexpr uint32_t kProjectionReturnState = 0x8003BD34u;
inline constexpr uint32_t kProjectionReturnOverlay = 0x8003F5C8u;

inline constexpr uint32_t kVSync = 0x80075350u;
inline constexpr uint32_t kVSyncEnd = 0x80075560u;
// PsyQ libgpu's DMA-queue timeout pair reads VSync(-1) only as a field clock. The native GPU
// completes submissions synchronously, so the title binding consumes Timing::vblank directly and
// preserves the two guest-visible timeout globals without entering guest libetc.
inline constexpr uint32_t kGpuTimeoutArm = 0x800750A8u;
inline constexpr uint32_t kGpuTimeoutCheck = 0x800750DCu;
inline constexpr uint32_t kGpuTimeoutDeadline = 0x8008AEBCu;
inline constexpr uint32_t kGpuTimeoutPollCount = 0x8008AEC0u;
inline constexpr uint32_t kCdRead = 0x80076F10u;
inline constexpr uint32_t kCdReadSync = 0x800770ACu;
// CTR wraps CdRead in an interrupt-completed state machine. psxport's native CdRead completes the
// transfer synchronously, so the title owner publishes the measured success-callback effects before
// returning to the generated caller.
inline constexpr uint32_t kAsyncDiscRead = 0x80032594u;
inline constexpr uint32_t kAsyncDiscCompletionCallback = 0x8003254Cu;
inline constexpr uint32_t kAsyncDiscCompletionStateGpOffset = 2260u;
inline constexpr uint32_t kCdReadyCallback = 0x8008AD10u;
inline constexpr uint32_t kAsyncDiscAwaitingCallback = 1u;
inline constexpr uint32_t kAsyncDiscComplete = 0u;
inline constexpr uint32_t kSetGeomScreen = 0x8007781Cu;
inline constexpr uint32_t kSetGeomOffset = 0x8007782Cu;
inline constexpr uint32_t kProjectionWindowEnd = 0x80077844u;

// Stock libcd leaves reached by CdInit. CTR's generated CdSync polls VSync(-1) while waiting for a
// controller interrupt. The PC CD model completes commands synchronously, so these two library
// leaves report that native result and the generated polling loop is never entered.
inline constexpr uint32_t kCdSync = 0x8007B6F0u;
inline constexpr uint32_t kCdControl = 0x8007BC38u;
inline constexpr uint32_t kCdControlWindowEnd = 0x8007C044u;

} // namespace ctr::native
