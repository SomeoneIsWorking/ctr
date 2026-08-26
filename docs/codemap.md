# Codemap

CTR follows Dusklight's ownership pattern by responsibility: the app composes process-lifetime
owners, core modules bridge the verified game/runtime boundary, future simulation owns authoritative
camera and object transforms, video owns producer commands and rendering, and a separate temporal
presentation owner decorates previous/current simulation state. Capability coverage belongs in
`docs/project-state.md`; the ordered evidence chain belongs in `docs/re-frontier.md`.

| Subsystem | Responsibility | Current / target location | Entry point | Deep doc |
|---|---|---|---|---|
| Player composition | Construct the title runtime and framework machine services, load the verified executable, and invoke the bounded product lifecycle | `game/app/main.cpp`, `game/core/bootstrap_frontier.{h,cpp}` | `main`, `ctr::runBootstrapToSupportedFrontier` | `README.md` |
| Framework-facing runtime | Own CTR's process-lifetime executable facts, generated dispatch entry, platform-HLE fact slice, and guest-picture declaration | `game/core/ctr_runtime.{h,cpp}` | `ctr::CtrRuntime` | `AGENTS.md` |
| Framework dependency | Provide shared runtime, renderer, oracle, recompiler, and host services at the exact provenance recorded by the title | `external/psxport/` resolved from `psxport.pin` | `tools/psxport_sync.py`, CMake `PSXPORT_DIR` | `AGENTS.md` |
| Generated-program adapter | Install the generated registry and expose invocation-scoped override wiring without leaking generated shard APIs into app composition | `game/core/recomp_register.{h,cpp}` | `ctr::installRecompiledProgram`, `ctr::setRecompiledOverride` | `docs/re-frontier.md` |
| Projection HLE facts | Supply the identity-gated retail libgte leaf addresses and admitted executable window; generic handlers remain framework-owned | `game/core/projection_hle_plan.{h,cpp}` | `ctr::projectionHlePlan` | `titles/ctr/README.md` |
| Bounded trace adapter | Execute generated boundaries and serialize CPU, device, and memory evidence for independent comparison | `game/core/crt0_port_trace.cpp` | `main` | `docs/re-frontier.md` |
| Input provisioning | Resolve user-supplied disc media, extract transactionally, and enforce complete executable identity | `tools/provision.py`, `tools/emit_substrate.py` | `provision.py`, `emit_substrate.py` | `titles/ctr/README.md` |
| Boot differential tooling | Construct exact bounded replay images and compare independent oracle state with shipping generated execution | `tools/resident_replay.py`, `tools/compare_crt0_trace.py`, `tools/compare_crt0_trace_selftest.py` | `compare_crt0_trace.py` | `docs/re-frontier.md` |
| Static render-source measurement | Verify exact projection leaves, producer signatures, callers, and GTE control/command census in the selected executable | `tools/measure_render_frontier.py` | `measure_render_frontier.py` | `docs/issues/0013-ctr-render-artifacts-had-no-binary-grounded-proj.md` |
| Runtime seam verification | Exercise the production direct-runtime install, executable extent, projection plan, generic handlers, picture declaration, and dispatch contract | `tests/test_ctr_runtime.cpp` | `ctr_runtime_test` | `docs/project-state.md` |
| Native simulation | Own authoritative simulation ticks plus current camera and object transforms | future simulation module under `game/` | target `Simulation` | `docs/project-goals.md` |
| Native video producers | Translate pre-GTE game camera/object/material state into typed native primitive commands | future producer modules under `game/` | target producer interfaces | `docs/project-goals.md` |
| Native render queue and renderer | Own primitive lifetime, ordering/depth, materials, viewport/projection, and final presentation | future video modules under `game/` | target `RenderQueue`, `Renderer` | `docs/project-goals.md` |
| Temporal presentation | Retain previous/current native transforms and calculate presentation-only interpolation without mutating simulation | future temporal presentation module under `game/` | target temporal decorator | `docs/project-goals.md` |
| Build and launcher policy | Compose the frozen Python setup, generated inputs, Clang-verifiable CMake targets, and asset-free/asset-gated checks | `run.sh`, `bootstrap.py`, `tools/run.py`, `CMakeLists.txt`, `pyproject.toml`, `uv.lock` | `run.sh`, CMake `verify` | `README.md` |

## Where does X go?

- Executable identity and load map: `titles/ctr/README.md`
- Disc resolution and extraction: `tools/provision.py`
- Resident generation: `tools/emit_substrate.py`, `game/recomp_seeds.json`
- Framework-facing process ownership: `game/core/ctr_runtime.{h,cpp}`
- Product boot boundary: `game/core/bootstrap_frontier.{h,cpp}`
- Generated registry installation: `game/core/recomp_register.{h,cpp}`
- Measured projection leaf plan: `game/core/projection_hle_plan.{h,cpp}`
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
