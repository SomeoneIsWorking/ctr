#pragma once

#include <cstdint>

namespace ctr::native {

// SCUS_944.26 guest addresses, image offsets and game-state offsets the native owners key on.
inline constexpr uint32_t kExecutableEntry = 0x8007793Cu;
inline constexpr uint32_t kGuestMain = 0x8003C58Cu;
inline constexpr uint32_t kLoopTop = 0x8003C5D0u;
inline constexpr uint32_t kInitializerEvidenceFrontier = 0x800772E0u;

// Retail calls that precede a guest VSync, and the continuations after it.
inline constexpr uint32_t kStartupGpuInit = 0x8003D7D8u;
inline constexpr uint32_t kStartupGpuInitReturn = 0x8003C7D8u;
inline constexpr uint32_t kAfterFirstStartupVSync = 0x8003C7E0u;
inline constexpr uint32_t kStartupDisplayInit = 0x800251ACu;
inline constexpr uint32_t kStartupDisplayInitReturn = 0x8003C7ECu;
inline constexpr uint32_t kAfterSecondStartupVSync = 0x8003C7F4u;

// State-zero loads two startup resources through 0x80031FDC (arg5 = -1), which ends in VSync(2).
inline constexpr uint32_t kBootResourceWait = 0x80031FDCu;
inline constexpr uint32_t kBootResourceWaitReturn = 0x80032074u;
// Retail bodies the -1 branch runs before its omitted VSync(2); they stay guest code.
inline constexpr uint32_t kBootResourcePreWaitHelper = 0x8003E978u;
inline constexpr uint32_t kBootResourceSetup = 0x800321B4u;
inline constexpr uint32_t kBootResourceCommit = 0x80031EE4u;
inline constexpr uint32_t kBootResourceWaitFirstCaller = 0x8003C8D4u;
inline constexpr uint32_t kBootResourceWaitSecondCaller = 0x8003C984u;
inline constexpr uint32_t kBootResourceWaitRaceCaller = 0x800336F8u;
// Interior suffix of 0x80033610: restores that function's frame and returns to its sole direct caller.
inline constexpr uint32_t kBootResourceWaitRaceResume = 0x8003CC98u;
// 0x8002DD24 polls CD/GPU stages in two do/while loops; the owner yields a field per false poll.
inline constexpr uint32_t kBootResourcePump = 0x8002DD24u;
inline constexpr uint32_t kBootResourcePumpReturn = 0x8003C8FCu;
inline constexpr uint32_t kBootResourcePumpBegin = 0x800297A0u;
inline constexpr uint32_t kBootResourcePumpPoll = 0x800293B8u;
inline constexpr uint32_t kBootResourcePumpCommit = 0x80029C40u;
inline constexpr uint32_t kBootResourcePumpCommitPoll = 0x80029CA4u;
// State-zero waits for the XA task at 0x8008D708; the owner advances one audio field per service call.
inline constexpr uint32_t kStartupAudioService = 0x8001D06Cu;
inline constexpr uint32_t kStartupAudioLoop = 0x8003C94Cu;
inline constexpr uint32_t kStartupAudioServiceReturn = kStartupAudioLoop;
inline constexpr uint32_t kStartupAudioWaitState = 0x8008D708u;
// libapi's DMA callback table; slot 4 (SPU) holds libspu's completion wrapper (0x8007AB34).
inline constexpr uint32_t kDmaCallback = 0x8008044Cu;
inline constexpr int kSpuDmaChannel = 4;
inline constexpr uint32_t kDmaCallbackTable = 0x8008CB08u;
inline constexpr uint32_t kSpuDmaCallbackSlot = kDmaCallbackTable + 4u * kSpuDmaChannel;
inline constexpr uint32_t kShutdownDisplay = 0x80025208u;
inline constexpr uint32_t kShutdownDisplayReturn = 0x8003CF30u;
inline constexpr uint32_t kAfterShutdownVSync = 0x8003CF38u;

// The native bridge skips only the conditional debug VSync(0) and enters the retail timing suffix.
inline constexpr uint32_t kFrameOwner = 0x80035E70u;
inline constexpr uint32_t kFrameTiming = 0x8004B3A4u;
inline constexpr uint32_t kFrameTimingReturn = 0x8003785Cu;
// The leaf's other direct caller is an elapsed-time query with no VSync to omit.
inline constexpr uint32_t kFrameTimingQueryReturn = 0x8004B438u;
inline constexpr uint32_t kFrameSuffix = 0x80037880u;
inline constexpr uint32_t kFrameLoopResume = 0x8003CEB4u;
// The suffix waits on the DrawSync callback byte and the VSync callback's two-field countdown.
inline constexpr uint32_t kVblankCallbackInstall = 0x80077254u;
inline constexpr uint32_t kDrawSyncCallbackSlot = 0x8008AD8Cu;
inline constexpr uint32_t kGameStateGpOffset = 832u;
inline constexpr uint32_t kDrawSyncPendingOffset = 7472u;
inline constexpr uint32_t kFrameCallbackCountOffset = 7392u;
inline constexpr uint32_t kFrameWaitFieldsGpOffset = 840u;
// Scene identity: the main-loop state (FUN_8003C58C switch), the level id and the loading bit of the game tracker.
inline constexpr uint32_t kMainStateGpOffset = 0x188u;
inline constexpr uint32_t kTrackerLevelIdOffset = 0x1A10u;
inline constexpr uint32_t kTrackerGameModeOffset = 0u;
inline constexpr uint32_t kGameModeLoading = 0x40000000u;

// The three return addresses are the only admitted entries to the projection owner.
inline constexpr uint32_t kProjectionProducer = 0x80042910u;
inline constexpr uint32_t kProjectionReturnLensflare = 0x80024CD4u;
inline constexpr uint32_t kProjectionReturnState = 0x8003BD34u;
inline constexpr uint32_t kProjectionReturnOverlay = 0x8003F5C8u;

inline constexpr uint32_t kVSync = 0x80075350u;
inline constexpr uint32_t kVSyncEnd = 0x80075560u;
// libgpu's DMA-queue timeout pair uses VSync(-1) as a field clock; the title binding keeps the two timeout globals.
inline constexpr uint32_t kGpuTimeoutArm = 0x800750A8u;
inline constexpr uint32_t kGpuTimeoutCheck = 0x800750DCu;
inline constexpr uint32_t kGpuTimeoutDeadline = 0x8008AEBCu;
inline constexpr uint32_t kGpuTimeoutPollCount = 0x8008AEC0u;
inline constexpr uint32_t kCdRead = 0x80076F10u;
inline constexpr uint32_t kCdReadSync = 0x800770ACu;
// libcd data-ready slot: 0x8001C4F4 installs the sync pair at 0x8008C41C (0x8001C7A4, CdlComplete) and
// this one (0x8001C7FC, CdlDataReady), which walks the XA state [0x8008D708] 2 -> 3 -> 4.
inline constexpr uint32_t kCdReadyCallbackSlot = 0x8008C420u;
// libcd's CdlDataReady, from the callback's own bytes.
inline constexpr uint8_t kCdReadyCompletionStatus = 1u;
// CD-audio task word [0x8008D708] (gp+0x79C, gp = 0x8008CF6C) is cleared at 0x8001C3A8, 0x8001C4C4, 0x8001C544,
// 0x8001C7D0, 0x8001CF78 and 0x8001D020; the last needs the arm [0x8008D6B8].
// Written by CdReadCallback 0x800771B0; the native CdRead completes first, so the owner dispatches it with CdlComplete.
inline constexpr uint32_t kCdReadCompletionCallback = 0x8008AD10u;
inline constexpr uint32_t kBigfileCompletionCallback = 0x80032110u;
inline constexpr uint32_t kCdlComplete = 2u;
inline constexpr uint32_t kSetGeomScreen = 0x8007781Cu;
inline constexpr uint32_t kSetGeomOffset = 0x8007782Cu;
inline constexpr uint32_t kProjectionWindowEnd = 0x80077844u;

// State zero publishes a literal projection (OFX=256, OFY=120, H=320) by fall-through, so it is not an override key.
inline constexpr uint32_t kStartupLiteralPublication = 0x8003C84Cu;
inline constexpr int32_t kStartupProjectionOfx = 256;
inline constexpr int32_t kStartupProjectionOfy = 120;
inline constexpr int32_t kStartupProjectionH = 320;
// Delay slots carrying the second and third literals.
inline constexpr uint32_t kStartupProjectionOfyDelay = 0x8003C854u;
inline constexpr uint32_t kStartupProjectionHDelay = 0x8003C85Cu;

// Stock libcd leaves reached by CdInit; the PC CD model completes synchronously, so they report that result.
inline constexpr uint32_t kCdSync = 0x8007B6F0u;
inline constexpr uint32_t kCdControl = 0x8007BC38u;
inline constexpr uint32_t kCdControlWindowEnd = 0x8007C044u;

} // namespace ctr::native
