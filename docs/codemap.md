# Codemap

CTR's app composition wires process owners; title core modules own
retail/native boundaries; simulation owns authoritative transforms; video owns producer commands,
rendering, and temporal presentation. Capability state belongs in `docs/project-state.md`, migration
order in `docs/migration.md`, binary evidence in `docs/re-frontier.md`, and atomic work in
`docs/issues/`.

## Ownership

| Subsystem | Responsibility | Current / target location | Entry point | Placement rule |
|---|---|---|---|---|
| Player composition | Parse asset-independent help, construct title/framework services, map the authenticated image, and step the only gameplay executor | `game/app/command_line.*`, `game/app/main.cpp`, `game/core/runtime_composition.*` | `main`, `ctr::installRuntimeOwners` | Product composition wires owners; it does not implement them or expose an engine selector |
| Framework-facing runtime | Own CTR identity, title policy, native-owner registration, and executor composition | `game/core/ctr_runtime.*` | `ctr::CtrRuntime` | CPU translation, Core synchronization, original-call dispatch, typed exits, and invalidation stay in psxport |
| Lightrec executor | Translate non-native retail code from authenticated runtime images and return typed bounded exits | `external/psxport/runtime/cpu/lightrec_executor.*` | `Core::lightrecExecutor`, `psx::cpu::dispatchGuest` | Lightrec owns cache/executable memory and its bounded refusal fallback; psxport owns state and device integration; CTR supplies title policy and never selects an interpreter |
| Frame/service exit owner | Record exact CTR wait/service/frame continuations, request typed executor exits, and finish one field after normal executor return | `game/core/frame_driver.*`, `game/core/native_ownership.h` | `CtrFrameDriver::stepFrame` | Never throw or `longjmp` through JIT frames |
| Disc completion | Keep each guest drive's owed callback in its game frame driver, run the shared native transfer, and deliver the measured retail completion at CTR's per-field seam | `game/core/async_disc_owner.*`, `game/core/frame_driver.*` | `discReadOwner(Core&)`, `DiscReadOwner::deliverPending` | Retail callback owns effects; timing and state lifetime remain title-owned |
| BIGFILE image publication | Match exact archive entry reads and raw content, retire overwritten generations, then publish the relocated byte extent after the retail callback | `game/core/overlay_image_owner.*`, generated identity facts from `titles/ctr/overlays.json` | `OverlayImageOwner::observeCompletedRead`, `publishAfterCallback` | Image identity is per-Core and bounded by exact archive byte size; Lightrec invalidation and dispatch stay in psxport |
| SPU DMA completion | Deliver owed channel-4 completion with measured DICR/BIOS/CPU ordering | `game/core/dma_callback_owner.*` | `DmaCallbackOwner::serviceSpu` | Safe seam remains after B0:17 unwind at `0x8003C94C` |
| Platform HLE facts | Supply authenticated libgte, libcd, libgpu, and fatal VSync addresses/windows | `game/core/platform_hle_plan.*` | `ctr::platformHlePlan` | Shared hardware semantics stay in psxport; title facts stay here |
| Projection publication | Capture pre-GTE view facts and compare the retail publication via an executor original call | `game/video/projection_owner.*` | `ProjectionOwner::publish` | Evidence plumbing is not widescreen or camera ownership |
| Presentation fence | Rotate one field fence and commit only captured retail/native work | `game/video/presentation_owner.*` | `PresentationOwner::finishField` | Called after a validated executor exit, never from guest VSync |
| Input/media provisioning | Resolve user media, verify `SCUS_944.26` and `BIGFILE.BIG`, and generate non-executable image identity facts | `tools/provision.py`, `tools/extract_overlays.py` | provisioning CLIs | Runtime publication belongs to the title image owner; provisioning never emits guest code |
| Title identity facts | Record the selected retail revision and its measured executable/load facts | `titles/` | `titles/ctr/README.md` | Game bytes remain untracked; title policy consumes verified facts |
| Differential evidence | Compare deterministic executor state/device/memory with an independent emulator | target dynamic harness; recorded evidence in `docs/info/` and `docs/re-frontier.md` | separate diagnostic target | An interpreter-only oracle is separately built and absent from gameplay; backend refusal fallback remains executor-owned |
| Product verification | Exercise shipping runtime owners, exit results, link composition, and negative controls | `tests/`, `tools/verify.py` | `tools/verify.py` over PSXPort's shared `port.consumer_verify` | Tests call production seams and do not duplicate instruction or exit semantics |
| Budget frontier diagnostic | Admit only the synchronized resident budget exit with active BF0233, then observe one unchanged Lightrec continuation under GDB | `tools/ctr_budget_probe*.py`, `tests/ctr_budget_probe_fixture.cpp` | `tools/ctr_budget_probe.py` | The tool owns observation and refusal; the shipping executor still owns translation, cycles, state, and exits |
| Native simulation | Own authoritative ticks and current camera/object transforms | future cohesive modules under `game/` | target `Simulation` | Simulation state is independent of presentation interpolation |
| Native producers | Convert pre-GTE camera/object/material state into typed primitive commands | future producer modules under `game/video/` | target producer interfaces | Never consume GTE/OT/GP0/framebuffer output as product source |
| Native renderer | Own primitive lifetime, ordering/depth, materials, viewport/projection, and presentation | future renderer modules under `game/video/` | target queue/renderer interfaces | Widescreen is applied at owned projection/viewport/culling boundaries |
| Temporal presentation | Interpolate previous/current native transforms without mutating simulation | future presentation decorator | target temporal interface | Alpha endpoints reproduce exact simulation snapshots |
| Build and launcher policy | Frozen Python setup, native/Lightrec product build, checks, and final player environment | `run.sh`, `bootstrap.py`, `tools/run.py`, `CMakeLists.txt`, `pyproject.toml`, `uv.lock` | `run.sh` | No offline translator, generated corpus, interpreter selector, or alternate engine mode |

## Where does it go?

- R3000A translation, Core synchronization, typed exits, or invalidation: psxport's Lightrec executor.
- CTR native override or original call: the smallest title owner, registered by image generation and address.
- Frame, host-work, interrupt, or service suspension: title continuation state plus a typed executor exit.
- Disc/overlay identity and runtime mapping: provisioning tools plus title runtime policy.
- Native camera/transforms: simulation owner; native primitives/order/depth: video owners.
- Capability, migration order, evidence, or atomic work: `docs/project-state.md`,
  `docs/migration.md`, `docs/re-frontier.md`, or `docs/issues/` respectively.
