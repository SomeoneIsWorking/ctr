# Codemap

The repository layers a verified USA executable provisioner, a true-oracle crt0 cross-check, and a
generated-code differential through an explicit A(39h) return over the shared psxport framework. A
bounded exact-register replay proves the first resident prefix, its one-time runtime initializer,
the startup services' exact idle/state-zero paths, the explicitly modeled A(2Bh) memset leaf
(poison-checked on both CPUs), executable swap `0x80077CD8`, and the indirect dispatcher
`0x800771C4`, then the initializer's complete IRQ/DPCR prefix, stopping before zero-fill callee
`0x800777E8`. A real `ctr_port` product owns the validated
generated registry and boots the provisioned executable to that bounded frontier; `run.sh` is its
zero-argument frozen-uv setup and launch interface.

| Subsystem | Status | Where | Gap / next |
|---|---|---|---|
| Player product | 🟡 real bounded boot product | `run.sh`, `bootstrap.py`, `tools/run.py`, `game/app/main.cpp`, `game/core/{recomp_register,bootstrap_frontier}.{h,cpp}`, CMake `ctr_port` | Zero args provision, emit, build, and launch `ctr_port`; product reaches `0x800777E8`, but zero-fill/gameplay/frame ownership is not implemented |
| Framework consumer | 🟡 derived runtime plus bounded generated trace | `game/core/ctr_runtime.{h,cpp}`, `CMakeLists.txt`, `external/psxport/`, `psxport.pin` | `CtrRuntime : GameRuntime` owns measured resident text and validated dispatch with null legacy views |
| Target executable | ✅ provisioned, oracle-executed, and port-traced | `tools/provision.py`, `tools/emit_substrate.py`, `titles/ctr/README.md` | Real USA media reproduced `SYSTEM.CNF` and `SCUS_944.26`; generated state agrees 34/34 at indirect target `0x800772E0` and 37/37 after its device prefix at `0x800777E8` |
| Generated substrate | 🟡 resident discovery output | `generated/` (gitignored), `tools/emit_substrate.py` | 1,236 emitted functions support the bounded trace; inventory is not execution proof and unresolved return edges remain |
| Project tooling | 🟡 bounded resident differential | `CMakeLists.txt` (`verify`, `oracle_boot_check`, `ctr04_*_check` through `ctr04_startup_init_device_next_call_check`), `tools/{provision,emit_substrate,compare_crt0_trace,resident_replay}.py` | Asset-free verification owns policy/both-answer selftests; exact code/data validation and trampoline exclusions prevent replay beyond or on top of its proof |
| Runtime seam test | ✅ direct inheritance contract | `tests/test_ctr_runtime.cpp` | Production `CtrRuntime` is installed into `Core`, exposes no legacy views/context, explicitly reports no guest-VRAM picture ownership without a rendered frame, and dispatches only its immutable validated target |
| Native engine | 🔬 runtime ownership only | `game/core/ctr_runtime.{h,cpp}`; `game/core/crt0_port_trace.cpp` | Process owner is direct inheritance; no game context, frame driver, scheduler, or native engine yet |
| Native graphics producers | ⬜ missing | — | No producer exists |
| Widescreen | ⬜ missing | — | Blocked on native camera and producers |
| Interpolation | ⬜ missing | — | Blocked on PC ownership of transform producers |
| Differential harness | 🟡 through initializer device prefix | framework `oracle_trace`/`crossvalidate_crt0.py`; CTR `crt0_port_trace.cpp`/`compare_crt0_trace.py`/`resident_replay.py`; CMake `ctr04_startup_init_device_next_call_check` | Repeated oracle evidence agrees on 34 CPU plus 3 device fields at `0x800777E8`; forced DPCR reports the sole 36/37 mismatch; next is zero-fill callee execution |

## Where is X?

- Target identity and load map: `titles/ctr/README.md`
- Disc resolution, transactional extraction, and identity gate: `tools/provision.py`
- Identity-gated resident emission: `tools/emit_substrate.py`, `game/recomp_seeds.json`
- Generated first-call/modeled-return/post-call capture: `game/core/crt0_port_trace.cpp`, `tools/compare_crt0_trace.py`
- Framework-facing process owner: `game/core/ctr_runtime.{h,cpp}` (`ctr::CtrRuntime`)
- Runtime inheritance contract: `tests/test_ctr_runtime.cpp`, CTest `ctr_runtime_inheritance`
- Exact-prefix resident replay: `tools/resident_replay.py`, `CMakeLists.txt` (`ctr04_resident_next_call_check`)
- Runtime-initializer continuation: `tools/compare_crt0_trace.py`, `CMakeLists.txt` (`ctr04_runtime_init_next_call_check`)
- Startup-service idle continuation: `tools/compare_crt0_trace.py`, `CMakeLists.txt` (`ctr04_startup_service_next_call_check`)
- State-zero initialization service: `tools/compare_crt0_trace.py`, `CMakeLists.txt` (`ctr04_startup_memset_thunk_check`)
- Modeled A(2Bh) memset continuation: `tools/compare_crt0_trace.py`, `CMakeLists.txt` (`ctr04_startup_post_memset_next_call_check`)
- Init-swap continuation to `0x800771C4`: `tools/compare_crt0_trace.py`, `CMakeLists.txt` (`ctr04_startup_init_swap_next_call_check`)
- Indirect-dispatch continuation to `0x800772E0`: `tools/compare_crt0_trace.py --startup-init-dispatch-next-call`, CMake `ctr04_startup_init_dispatch_next_call_check`
- Initializer device-prefix continuation to `0x800777E8`: `tools/compare_crt0_trace.py --startup-init-device-next-call`, CMake `ctr04_startup_init_device_next_call_check`
- Shipping player composition and bounded lifecycle: `game/app/main.cpp`, `game/core/{recomp_register,bootstrap_frontier}.{h,cpp}`
- Fresh-clone player launcher: `run.sh`, `bootstrap.py`, `tools/run.py`, `pyproject.toml`, `uv.lock`
- Independent real-crt0 execution and cross-check: `CMakeLists.txt` (`oracle_boot_check`)
- Oracle-to-generated boundary gates: `CMakeLists.txt` (`ctr04_check`, `ctr04_post_init_heap_check`, `ctr04_resident_next_call_check`, `ctr04_runtime_init_next_call_check`, `ctr04_startup_service_next_call_check`, `ctr04_startup_memset_thunk_check`)
- Framework-only build target: `CMakeLists.txt` (`ctr_scaffold`)
- Normal build/style/lint/smoke gate: `CMakeLists.txt` (`verify`)
- Ordered RE dependency chain: `docs/re-frontier.md`
- Symptom/finding history: `docs/issues/`
