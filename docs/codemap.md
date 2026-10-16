# Codemap

Where code lives in the CTR port. Capability state is in `docs/project-state.md`, feature hops in
`docs/features.md`, migration order in `docs/migration.md`, the evidence chain in `docs/re-frontier.md`, atomic
work in `docs/issues/`. The framework side is `external/psxport/docs/codemap.md`.

Product path: `run.sh` → `tools/run.py` (provision, build) → the framework main (`psxport_add_product`, target
`ctr_port`) → `psx::host::ProductHost` over `ctr::createCatalog` → `TitleSession` → `ctr::Runtime`
(`admitExecutable` default, `registerOverrides`) → `ctr::FrameDriver::stepFrame` once per display field on
psxport's Lightrec executor.

## Placement rules

- Everything is in namespace `ctr`, with `ctr::native` for measured addresses, `ctr::identity` for the generated
  executable facts and `ctr::guest` for guest calls. Includes name the owning directory: `"runtime/runtime.h"`;
  psxport headers are bare.
- `ctr::guest` (`game/execution/guest_turns.*`) is the only module that spells an `ExecutionBudget` or a dispatch
  form. Every owner calls guest code through it.
- Per-Core title state lives on `ctr::Session`, reached through `session(core)`, or on the frame driver; the
  runtime holds only what is the same for every Core (stock library bindings, the guest projection).
- A measured address lives at a name in `game/title/native_ownership.h`; executable identity facts in
  `titles/ctr/executable.json`, read by CMake and `tools/provision.py`.
- psxport owns the executor, override keys, the stock VSync/libgte/libcd classes, input, audio, disc transfer and
  presentation. This repository supplies addresses and title behaviour.

## Owners

| Owner | Path | What it owns |
|---|---|---|
| `createCatalog` | `game/product/catalog.*` | The one-title catalog over the `SCUS_944.26` identity generated from `titles/ctr/executable.json` (`game/product/executable_identity.h.in`), provisioned under `scratch/raw` |
| `ctr::Runtime` | `game/runtime/runtime.*` | The `GameRuntime`: `StockVSync`, `StockLibgte` and `GuestInterruptLibcd` (`CdReadSync` stock, ready slot `0x8008C420`) bound at this executable's addresses, the title's platform leaves, Record with no producers, `sealedFrameIsCut`, the guest projection |
| `ctr::Session`, `session(core)` | `game/runtime/session.*` | Per-Core `SceneCut` and `DiscReadOwner` |
| platform leaves | `game/execution/platform_leaves.*` | libgpu's timeout pair on the host field clock, `CdSync` as CdInit's callers read it, and their binding with the title's `CdRead` |
| `ctr::guest` | `game/execution/guest_turns.*` | Field dispatch, nested hops, guest and original calls, frame-boundary propagation |
| `SpuDmaCallbackRegistration` | `game/execution/spu_dma_callback_registration.*` | Runs the retail DMACallback `0x8008044C` and publishes the guest's SPU channel-4 callback to psxport's `DmaCallbackRegistry` |
| `ctr::FrameDriver` | `game/frame/frame_driver.*` | One finite field: arm the field's overrides and GTE observation, the host's per-field work, the owed continuation, dispatch from the admitted entry (first field) or `kFrameLoopResume`, finish. `FrameDriver::from(core)` resolves the driver for a bare guest override |
| `FieldBoundary` | `game/frame/field_boundary.*` | When a field ends: the title's request or the framework's typed frame-boundary exit; finishing it commits one fence |
| `FieldOverrideScope` | `game/frame/field_override_scope.*` | Installs the field's guest-address overrides on entry and removes exactly those on exit |
| `FrameSuffix`, `frameSuffixStillWaiting` | `game/frame/frame_suffix.*` | The timing bridge with its debug `VSync(0)` omitted, the three words the retail suffix `0x80037880` waits on, and when it may run |
| `FrameCallbackOwner` | `game/frame/frame_callback_owner.*` | The DrawSync callback owed at the field seam and a poll of the latched vblank edge, so libetc's ISR runs once per field |
| `runVsyncBridge`, `refuseUnexpectedRetailReturn` | `game/frame/vsync_bridge.*`, `retail_return.*` | One VSync callsite omitted with its register effects kept; the refusal for an unmeasured return address |
| `StartupResourceLoad` | `game/boot/startup_resource_load.*` | The −1 branch of `0x80031FDC`: retail setup and commit, the field its omitted `VSync(2)` owed, the caller it resumes |
| `StartupResourcePump` | `game/boot/startup_resource_pump.*` | The two `0x8002DD24` poll loops, one host field per false poll |
| `StartupAudioWait` | `game/boot/startup_audio_wait.*` | The post-archive XA wait on `0x8008D708`, one audio field per service call |
| `LevelWarp` | `game/debug/level_warp.*` | The `warp <level-hex>` control command: armed during a race with no load pending, applied at the next field start through the guest's own level-load request `0x8003CFC0`; reached from `Runtime::titleCommand` through `FrameDriver::armWarp` |
| `DiscReadOwner`, `readWithCompletionCallback` | `game/disc/async_disc_owner.*` | The title's `CdRead`: the stock synchronous transfer, then the libcd completion callback owed once the drive's read time has passed |
| `OverlayImageOwner` | `game/disc/overlay_image_owner.*` | An exact BIGFILE entry read as an image candidate, published after the retail callback relocates it |
| `ProjectionOwner` | `game/video/projection_owner.*` | The pre-GTE publication `0x80042910`: capture the view, compare the retail libgte state, apply the plan |
| `WidescreenOwner` | `game/video/widescreen_owner.*` | The widening plan, resolved once from the extent the guest's own publication carried |
| `GeometryProjectionOwner` | `game/video/geometry_projection_owner.*` | The same plan applied at the GTE perspective op, armed for exactly one field through `core.rsub.gtePreOp` |
| `widenViewProjection` | `game/video/view_projection_plan.*` | The pure widening rule over a guest view |
| `PresentationOwner` | `game/video/presentation_owner.*` | The one `presentation.commit` per field; samples `SceneCut` first |
| `SceneCut` | `game/video/scene_cut.*` | Main-loop state `[gp+0x188]`, level id and loading bit of the tracker `[gp+0x340]`, and whether they changed since the last commit |
| facts | `game/title/native_ownership.h` | Every measured guest address and game-state offset; facts only |
| tests | `tests/` | Each owner through the shipping sources (`ctr_<name>` CTest targets) and the launcher test |
| tools | `tools/` | Launcher, provisioning, overlay extraction, the verifier, `policy/accepted.txt` |

## The frame turn

| Hop | Owner |
|---|---|
| product spine | `ProductHost` → `TitleSession`: `admitExecutable` → `registerOverrides` → `startProgram` (default) |
| field entry | `FrameDriver::stepFrame`: `ScopedGteProjectionObservation`, `FieldOverrideScope`, `FieldBoundary::beginField` |
| host work | `Timing::frameTick`, `FrameCallbackOwner::deliverField`, `Pad::serviceFrame`, `SpuAudio::frame`, `DiscReadOwner::deliverPending` |
| owed continuation | `FrameSuffix` → `StartupAudioWait` → `StartupResourcePump` → `StartupResourceLoad`, one at a time |
| debug warp | `LevelWarp::apply` before the loop resumes, when no continuation is owed |
| guest | `FrameDriver::dispatchField` → `guest::dispatch` → Lightrec; a budget exit resumes at the same PC, one that consumed nothing is refused |
| field end | `FieldBoundary::finishField` → `PresentationOwner::finishField` → `presentation.commit` |
| refusal | `FrameDriver::refuseUnfinishedField` logs and stops; never an exception or `longjmp` |

## Where does it go?

| Responsibility | Owner |
|---|---|
| A measured address | `game/title/native_ownership.h`, and the chain in `docs/re-frontier.md` |
| Calling or re-entering guest code | `game/execution/guest_turns.*` only |
| A stock library binding or a platform leaf | `Runtime::registerOverrides`, `game/execution/platform_leaves.*` |
| A field override or a per-field decision | `ctr::FrameDriver` and the smallest owner in its ladder |
| Per-Core state | a member of `ctr::Session` or the frame driver |
| A debug route to a screen or level | `game/debug/`, armed by `Runtime::titleCommand` and applied by the frame driver at a field start |
| Projection, widening or the presentation fence | `game/video/`, one owner per decision |
| Executor, input, audio, disc transfer, presentation | psxport |
