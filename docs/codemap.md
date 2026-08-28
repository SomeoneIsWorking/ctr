# Codemap

CTR follows Dusklight's ownership pattern by responsibility: the app composes process-lifetime
owners, core modules bridge the verified game/runtime boundary, future simulation owns authoritative
camera and object transforms, video owns producer commands and rendering, and a separate temporal
presentation owner decorates previous/current simulation state. Capability coverage belongs in
`docs/project-state.md`; the ordered evidence chain belongs in `docs/re-frontier.md`.

| Subsystem | Responsibility | Current / target location | Entry point | Deep doc |
|---|---|---|---|---|
| Player composition | Parse asset-independent help before provisioning, construct title/framework services, bind CTR's direct native-disc key, install runtime owners and the effective render path through one production/test path, and delegate the only bounded product iteration to the framework shell | `game/app/command_line.{h,cpp}`, `game/app/main.cpp`, `game/core/runtime_composition.{h,cpp}` | `ctr::parseCommandLine`, `main`, `ctr::installRuntimeOwners`, `FrameLoopShell::step` | `README.md` |
| Framework-facing runtime | Own CTR's process-lifetime executable facts, generated dispatch entry, platform-HLE fact slice, truthful player capabilities, and guest-picture declaration | `game/core/ctr_runtime.{h,cpp}` | `ctr::CtrRuntime` | `AGENTS.md` |
| Native iteration boundary | Own one finite retail state-3 transition, field-counter tick, input and audio sample per host step; preserve generated supers around forbidden VSync calls; yield host fields across state-zero resource and startup-audio waits; and return at the measured frame-owner fence | `game/core/frame_driver.{h,cpp}`, `game/core/native_ownership.h` | `ctr::CtrFrameDriver::stepFrame` | `docs/re-frontier.md` |
| Synchronous disc completion | Own the measured stock libcd CdRead leaf: shared native transfer, then dispatch the retail read-completion callback the CD interrupt would have entered | `game/core/async_disc_owner.{h,cpp}` | `ctr::DiscReadOwner::deliverCompletion`, `ctr::cdReadWithCompletionCallback` | `docs/issues/0019-ctr-load-pipeline-stalled-on-an-undelivered-libcd.md` |
| SPU DMA callback delivery | Deliver only owed channel-4 completion through CTR's measured libapi slot while preserving DICR acknowledgement, BIOS nesting order, CPU context, and finite chained-transfer deferral | `game/core/dma_callback_owner.{h,cpp}` | `ctr::DmaCallbackOwner::serviceSpu` | `docs/issues/0015-ctr-product-had-no-native-frame-loop-vsync-owner.md` |
| Framework dependency | Provide shared runtime, renderer, oracle, recompiler, and host services at the exact provenance recorded by the title | `external/psxport/` resolved from `psxport.pin` | `tools/psxport_sync.py`, CMake `PSXPORT_DIR` | `AGENTS.md` |
| Generated-program adapter | Install the generated registry, expose frame-scoped override wiring, and preserve named raw generated supers without leaking shard APIs into app composition | `game/core/recomp_register.{h,cpp}` | `ctr::installRecompiledProgram`, `ctr::setRecompiledOverride`, `ctr::runRecompiledSuper` | `docs/re-frontier.md` |
| Platform HLE facts | Supply identity-gated libgte, stock-libcd sync/read leaves, libgpu queue-timeout arm/check, retail VSync, and admitted executable windows; shared CD effects and the fatal VSync trap remain framework-owned while CTR's timeout pair consumes the host field clock | `game/core/platform_hle_plan.h`, `game/core/platform_hle_plan.cpp` | `ctr::platformHlePlan` | `README.md` |
| Projection publication | Capture CTR's measured pre-GTE view facts, preserve the raw retail publication as an A/B super, and refuse disagreement with the published libgte state | `game/video/projection_owner.h`, `game/video/projection_owner.cpp` | `ctr::ProjectionOwner::publish` | `docs/issues/0016-ctr-advertised-native-and-temporal-rendering-bef.md` |
| Presentation fence | Rotate exactly one framework fence at each measured frame-owner return, commit captured retail work through the compatibility presenter, and mark empty fields unpresented | `game/video/presentation_owner.h`, `game/video/presentation_owner.cpp` | `ctr::PresentationOwner::finishField` | `docs/issues/0015-ctr-product-had-no-native-frame-loop-vsync-owner.md` |
| Bounded trace adapter | Execute generated boundaries and serialize CPU, device, and memory evidence for independent comparison | `game/core/crt0_port_trace.cpp` | `main` | `docs/re-frontier.md` |
| Input provisioning | Resolve user-supplied disc media, extract transactionally, and enforce complete executable identity | `tools/provision.py`, `tools/emit_substrate.py` | `provision.py`, `emit_substrate.py` | `titles/ctr/README.md` |
| Overlay image provisioning | Verify BIGFILE.BIG identity, parse its entry index, and slice the declared code modules into the emitter's overlay input | `tools/extract_overlays.py` | `extract_overlays.py` | `docs/issues/0020-ctr-calls-overlay-code-the-emitter-never-discove.md` |
| Boot differential tooling | Construct exact bounded replay images and compare independent oracle state with shipping generated execution | `tools/resident_replay.py`, `tools/compare_crt0_trace.py`, `tools/compare_crt0_trace_selftest.py` | `compare_crt0_trace.py` | `docs/re-frontier.md` |
| Static render-source measurement | Verify exact projection leaves, producer signatures, callers, and GTE control/command census in the selected executable | `tools/measure_render_frontier.py` | `measure_render_frontier.py` | `docs/issues/0013-ctr-render-artifacts-had-no-binary-grounded-proj.md` |
| Runtime seam verification | Exercise the production direct-runtime install, executable extent, projection plan, generic handlers, picture declaration, and dispatch contract | `tests/test_ctr_runtime.cpp` | `ctr_runtime_test` | `docs/project-state.md` |
| Native simulation | Own authoritative simulation ticks plus current camera and object transforms | future simulation module under `game/` | target `Simulation` | `docs/project-goals.md` |
| Native video producers | Translate pre-GTE game camera/object/material state into typed native primitive commands | future producer modules under `game/` | target producer interfaces | `docs/project-goals.md` |
| Native render queue and renderer | Own primitive lifetime, ordering/depth, materials, viewport/projection, and final presentation | future video modules under `game/` | target `RenderQueue`, `Renderer` | `docs/project-goals.md` |
| Temporal presentation | Retain previous/current native transforms and calculate presentation-only interpolation without mutating simulation | future temporal presentation module under `game/` | target temporal decorator | `docs/project-goals.md` |
| Build and launcher policy | Compose the frozen Python setup, generated inputs, Clang-verifiable CMake targets, asset-free/asset-gated checks, and the final player environment including the linked framework's asset root | `run.sh`, `bootstrap.py`, `tools/run.py`, `CMakeLists.txt`, `pyproject.toml`, `uv.lock` | `run.sh`, `tools.run.launch`, CMake `verify` | `README.md` |

## Where does X go?

- Executable identity and load map: `titles/ctr/README.md`
- Disc resolution and extraction: `tools/provision.py`, `tools/extract_overlays.py`
- Resident generation: `tools/emit_substrate.py`, `game/recomp_seeds.json`
- Framework-facing process ownership: `game/core/ctr_runtime.{h,cpp}`
- Product runtime composition: `game/core/runtime_composition.{h,cpp}`
- Asset-independent command line: `game/app/command_line.{h,cpp}`
- Product iteration and measured frame transition: `game/core/frame_driver.{h,cpp}`, `game/core/native_ownership.h`
- Synchronous title disc/DMA completion: `game/core/async_disc_owner.{h,cpp}`, `game/core/dma_callback_owner.{h,cpp}`
- Generated registry installation: `game/core/recomp_register.{h,cpp}`
- Measured platform-HLE plan: `game/core/platform_hle_plan.{h,cpp}`
- Measured projection publication: `game/video/projection_owner.{h,cpp}`
- Current compatibility presentation fence: `game/video/presentation_owner.{h,cpp}`
- Exact CPU/device/memory replay: `tools/resident_replay.py`, `tools/compare_crt0_trace.py`
- Static projection and primitive-producer census: `tools/measure_render_frontier.py`
- Native camera and transforms: future simulation module under `game/`
- Native primitive producers: future producer modules under `game/`
- Native ordering, depth, widescreen projection, and presentation: future video modules under `game/`
- Previous/current transform interpolation: future temporal presentation module under `game/`
- Capability status and current focus: `docs/project-state.md`
- Epic outcomes and success conditions: `docs/project-goals.md`
- Atomic investigations and resolved defects: `docs/issues/`
- Ordered ground-truth dependency chain: `docs/re-frontier.md`
