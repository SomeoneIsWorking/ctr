# Codemap

CTR's structural authority: where every title subsystem lives, which namespace and class owns it, and
what that owner decides. Capability state is `docs/project-state.md`, migration order
`docs/migration.md`, per-step reverse-engineering status `docs/re-frontier.md`, atomic work
`docs/issues/`. Framework ownership is `external/psxport/docs/codemap.md`; every chain below names the
framework hop explicitly rather than restating it.

Nothing in `game/` is in the global namespace. Every owner is a class in namespace `ctr`; the two
stateless rules (`widenViewProjection`, `frameSuffixStillWaiting`) and the two Core-to-owner resolvers
(`ctrFrameDriver`, `discReadOwner`) are namespace functions in the same namespace.

## Directories

| Directory | Namespace | What it owns |
|---|---|---|
| `game/app/` | `ctr` | The process entry point and its argument policy. Composition only; it implements no subsystem. |
| `game/core/` | `ctr`, `ctr::native` | The title's runtime-facing owners: the frame turn, the continuation owners a field can owe, the CD/DMA completions the guest is waiting on, and the measured guest addresses they are keyed by. |
| `game/video/` | `ctr` | Presentation-facing owners: the projection publication, the widescreen decision, and the presentation fence. |
| `tests/` | — | Focused tests that drive the production seams above; they never restate a rule the owner implements. |
| `tools/` | — | Provisioning and launcher Python. No C++ owner lives here. |
| `titles/ctr/` | — | Measured title facts (revision, executable facts) and generated non-executable image identity. |

## `game/app/` — the process entry

| Class / function | Responsibility |
|---|---|
| `main` (`main.cpp`) | Parse arguments, refuse a missing executable, construct `Game`, install the title's owners, then step fields forever. It composes; it implements nothing. |
| `ctr::CommandLineAction`, `ctr::parseCommandLine`, `ctr::printUsage` (`command_line.*`) | The argument policy: run, `--help`, or a named refusal. |

## `game/core/` — runtime-facing owners

| Class / function | Responsibility |
|---|---|
| `ctr::CtrRuntime` (`ctr_runtime.*`) | The title's `GameRuntime`: identity facts, platform HLE facts, override install/remove, and every guest dispatch (one field, one continuation, one original call). It dispatches; it does not decide what runs next. |
| `ctr::CtrFrameDriver`, `ctr::ctrFrameDriver` (`frame_driver.*`) | One finite CTR field: arm the field's overrides and GTE observation, run the host's per-field work, serve the owed continuation, dispatch the field, and finish it. `ctrFrameDriver(Core&)` is the single Core-to-driver resolver every bare guest override uses. |
| `ctr::FieldBoundary` (`field_boundary.*`) | When a field ends: the title's own request OR the framework's typed exit, and the commit that consumes it, runs the debug teardown and the presentation fence, and counts the field. |
| `ctr::FieldOverrideScope` (`field_override_scope.*`) | Installs the field's guest-address overrides on entry and removes exactly those on exit; which rows a non-observing field drops is a property of each row. |
| `ctr::StartupResourceLoad` (`startup_resource_load.*`) | The fifth-argument −1 branch of `0x80031FDC`: the retail setup and commit, the two fields its omitted `VSync(2)` owed, and the caller frame it resumes. |
| `ctr::StartupResourcePump` (`startup_resource_pump.*`) | The phase of the two non-returning `0x8002DD24` polls, yielding one host field per false poll and delivering owed SPU work before the generic IRQ path. |
| `ctr::StartupAudioWait` (`startup_audio_wait.*`) | The post-archive XA wait at `0x8008D708`: the retail service call, one host audio field per wait, and the continuation it resumes. |
| `ctr::FrameSuffix` / `ctr::FrameSuffixWait` / `ctr::frameSuffixStillWaiting` (`frame_suffix.*`) | The three guest words the retail suffix waits on, the pure predicate over them, and when the suffix may run and end the field. |
| `ctr::FrameCallbackOwner` (`frame_callback_owner.*`) | The retail callbacks the direct runtime does not generate: vblank registration and per-field DrawSync/VSync delivery, each preserving the interrupted register context. |
| `ctr::DmaCallbackOwner`, `ctr::DmaCompletionBackend` (`dma_callback_owner.*`) | Delivers the measured SPU channel-4 completion from the guest's own callback table, at the finite host boundary before the generic IRQ path can consume it. |
| `ctr::DiscReadOwner`, `ctr::discReadOwner`, `ctr::cdReadWithCompletionCallback` (`async_disc_owner.*`) | The owed libcd completion: when it is delivered, and the retail callback that owns its effects. `cdReadWithCompletionCallback` is the PlatformHle binding for the stock `CdRead` leaf. |
| `ctr::OverlayImageOwner`, `ctr::CompletedDiscRead` (`overlay_image_owner.*`) | A BIGFILE transfer as an image candidate, and the publication of its relocated RAM extent after the retail callback returns. |
| `ctr::CtrWidescreen` (`game/video/widescreen_owner.*`) | The one widening decision: the aspect answer, and the plan resolved from the extent the guest's own publication carried. |
| `ctr::ProjectionOwner` (`game/video/projection_owner.*`) | The measured pre-GTE publication: capture the view input, compare the retail libgte state, then apply the plan. Also the per-source publication census. |
| `ctr::CtrGeometryProjectionOwner`, `ctr::ScopedGteProjectionObservation` (`game/video/geometry_projection_owner.*`) | The second application point: the same plan, applied at the GTE op that consumes the triple, armed for exactly one field. |
| `ctr::PresentationOwner` (`game/video/presentation_owner.*`) | The framework presentation fence at a field boundary: commit a captured frame, or record the field as explicitly unpresented. |
| `ctr::RenderListBoundaryDiagnostic` (`render_list_boundary_diagnostic.*`) | Debug-only observation of the `0x8003B43C` list publication; it never supplies list contents or changes control flow. |
| `ctr::platformHlePlan` (`platform_hle_plan.*`) | The authenticated libgte/libcd/libgpu addresses and windows, plus the six bindings installed for a direct runtime. |
| `ctr::runVsyncBridge`, `ctr::VsyncBridge`, `ctr::refuseUnexpectedRetailReturn` (`vsync_bridge.*`, `retail_return.*`) | One extracted VSync callsite as data, the bridge that omits only that call, and the shared refusal for a return address the title has no identity for. |
| `ctr::installRuntimeOwners` (`runtime_composition.*`) | The composition step between `Game` construction and boot: the disc media key and the render path. |
| `ctr::native` constants (`native_ownership.h`) | Every measured guest address and game-state offset the owners above are keyed by. Facts only; no behaviour. |

## Who owns it

One chain per turn, hop by hop, naming the class and method at every hop. A hop that is not in the
owner's file is the defect to look for first. Framework hops are marked *(framework)*.

### The frame turn

| Hop | Owner | What it decides |
|---|---|---|
| Field iteration | `main` → `FrameLoopShell::step` *(framework)* | One host iteration; the framework owns iteration, the title owns the finite step. |
| Field entry | `CtrFrameDriver::stepFrame` | Opens the field's census, arms `ScopedGteProjectionObservation` and `FieldOverrideScope`, and clears the previous field's request. |
| Host work | `Timing::frameTick` *(framework)*, `FrameCallbackOwner::deliverField`, `Pad::serviceFrame` *(framework)*, `SpuAudio::frame` *(framework)*, `DiscReadOwner::deliverPending`, `DmaCallbackOwner::serviceSpu` | Everything owed at the field seam, in that order: guest callbacks, input, audio, then the CD and DMA completions the guest is blocked on. |
| Owed continuation | `CtrFrameDriver::resumeOwedContinuation` | One ladder, one owner at a time: `FrameSuffix::resume` → `StartupAudioWait::resume` → `StartupResourcePump::resume` → `StartupResourceLoad::consumeWaitedField`. |
| Guest execution | `CtrFrameDriver::dispatchField` → `CtrRuntime::dispatch` → `psx::cpu::dispatchGuestUntilExit` *(framework)* → Lightrec *(framework)* | Runs the field from `kFrameLoopResume` (or the boot target), resuming a budget exit at the same PC and refusing one that consumed nothing. |
| Guest override | a `CtrFrameDriver::on*` entry point → `ctrFrameDriver(Core&)` | Each bare guest override names one operation and hands the Core to the owner that owns it. |
| Original call | `CtrRuntime::callOriginalToReturn` → `psx::cpu::callOriginal` *(framework)* | Runs the retail body a native owner replaced, and requires it to return. |
| Field end | `FieldBoundary::pending` → `FieldBoundary::finishField` | The only two routes to "finished" cannot disagree; a non-frame pending exit is refused. |
| Presentation | `PresentationOwner::finishField` → `Game::presentation.commit` / `commitUnpresented` *(framework)* | Exactly one fence per field, advanced or explicitly skipped. |
| Refusal | `CtrFrameDriver::refuseUnfinishedField` | A field that cannot finish reports the projection censuses and stops. Never an exception or a `longjmp`. |

**While a movie plays or a load waits.** CTR owns no movie player: an FMV is retail code executing
inside `dispatchField`, so the field turn above owns it and keeps pumping input each field. The
startup resource load, resource pump and XA wait are the only owners that end a field from the host
side, and each resumes from `resumeOwedContinuation` — that ladder is the frame turn's only other
entry, and `FieldBoundary` is the only thing that ends a field.

### Host input → `Pad` → guest pad buffer

| Hop | Owner | What it decides |
|---|---|---|
| Field service | `CtrFrameDriver::stepFrame` → `core.game->pad.serviceFrame()` | CTR's only input call, at the field seam before guest execution. |
| Host pump | `Pad::serviceFrame` → `Pad::pollHostInput` → `psx::input::HostInput::poll(bool)` *(framework)* | The ONE host-input owner (`psx::input::HostInput`, header `host_input.h`) and the ONE SDL drain; it returns the active-low mask for this turn. `pollHostInput` is called from inside `serviceFrame`, so a title that calls only `serviceFrame` still pumps. |
| Pad frame | `Pad::serviceFrame` *(framework)* | force/hold → REPL drive → suppression → record/replay, then the digital packet. |
| Guest | `Pad::fillBuffer` → the registered slot buffers *(framework)* | The 4-byte per-VBlank packet the guest reads. |
| Movie skip | `Fmv::playToEnd` → `Pad::pollHostInput` *(framework)* | A movie turn is a served host frame, so the skip reads the pad owner's serviced mask. CTR never calls the blocking movie entry point. |
| Debug channel | `HostInput::takePauseRequest` / `takeFrameStepRequest` → `DbgServer::togglePause` / `addStep` *(framework)* | The P/`.` edges are detected by the input owner and acted on by the debug channel. |
| Window availability | `gpu_vk_windowed()` *(framework, declared in `gpu_vk.h`)* | The window-answer every pump passes explicitly; the renderer no longer owns a `gpu_windowed()`. |

### Guest draw → presentation

| Hop | Owner | What it decides |
|---|---|---|
| Projection publication | `CtrFrameDriver::onProjectionProducer` → `ProjectionOwner::publish` | Captures the guest's pre-GTE view facts, runs the retail producer as an original call, compares the published libgte state, then applies the plan. |
| Widening decision | `CtrWidescreen::planFor` → `gpu_vk_latch_guest_projection` *(framework)* | Resolves the plan from the extent the guest's own publication carried, once, on the first publication. |
| GTE application | `CtrGeometryProjectionOwner::observe` → `ScopedGteProjectionObservation::onGteOp` *(framework observer)* | Applies the same plan at the op that consumes the triple, covering the ten submitters the descriptor route never reaches. |
| Guest draw | guest GP0/OT → `RenderQueue` / `GpuState` *(framework)* | The title supplies no primitives: the guest's own GTE and ordering table are rasterized by the framework. |
| Fence | `FieldBoundary::finishField` → `PresentationOwner::finishField` → `Game::presentation.commit` *(framework)* | One presented or explicitly unpresented field. |
| 60 fps in-between | `CtrRuntime::renderCapabilities` | Declares `temporalInterpolation = false`, so no in-between exists while CTR has no native producers. |
| Widescreen extent | `Game::presentation` / `GpuWindow` *(framework)* | The window and margin the plan asked for; the title never touches host pixels. |

### CD / streaming

| Hop | Owner | What it decides |
|---|---|---|
| Stock `CdRead` | `cdReadWithCompletionCallback` (bound by `platformHlePlan`) | Delivers any owed completion, runs the shared synchronous transfer, then records the new one. |
| Completion owed | `DiscReadOwner::noteTransferComplete` | Whether a callback is registered (owed) or the caller polls `CdReadSync` (counted). |
| Delivery | `DiscReadOwner::deliverPending` → `CtrRuntime::dispatchToReturn` | Runs the retail completion callback with the interrupted context intact, at a seam where the issuing call has unwound. |
| Image publication | `OverlayImageOwner::observeCompletedRead` / `publishAfterCallback` | Identity for an exact archive entry read, then the relocated extent published after the callback. |
| Sector transfer | `cd_read_stock_sync` *(framework)* | The transfer itself; the title never moves sectors. |

### Audio

| Hop | Owner | What it decides |
|---|---|---|
| Field mix | `SpuAudio::frame` *(framework)* | Advances one host audio field, which the startup XA wait depends on. |
| Startup wait | `StartupAudioWait::resumeLoop` / `service` | The retail service call, then one field yielded while the task runs. |
| SPU completion | `DmaCallbackOwner::serviceSpu` → `DmaCompletionBackend` *(framework DMA state)* | Delivers the guest's channel-4 callback before the generic IRQ path can consume it. |

### Debug / control channel

| Hop | Owner | What it decides |
|---|---|---|
| Live endpoint | `DbgServer::start` *(framework)*, enabled by `PSXPORT_DEBUG_SERVER` | The loopback control channel: state, memory, input injection, pause/step. |
| Title boot | `main` | **CTR's product never starts it**: the title has its own entry point rather than `native_boot`, so `PSXPORT_DEBUG_SERVER` opens nothing and a headless run cannot be driven. Recorded as a finding, not as a design choice. |
| Render-list observation | `RenderListBoundaryDiagnostic` (installed by `FieldOverrideScope` only when `PSXPORT_DEBUG=ctr-render-list`) | The title's only in-product diagnostic channel. |

## Where does it go?

- A guest address or game-state offset: `game/core/native_ownership.h`.
- A per-field decision about what runs next: the smallest owner named in the ladder, composed by
  `CtrFrameDriver`.
- Anything that must survive a field boundary: the owner that holds it, as a resumable method pair
  consumed by `resumeOwedContinuation`.
- Projection, widening or the presentation fence: `game/video/`, one owner per decision.
- A dispatch, an original call or an override install: `CtrRuntime`; never a call site.
- R3000 translation, Core synchronization, typed exits, invalidation, input, audio, disc transfer,
  rendering or the host loop: psxport.
- Capability state, migration order, RE status or atomic work: `docs/project-state.md`,
  `docs/migration.md`, `docs/re-frontier.md`, `docs/issues/`.
