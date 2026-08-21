# Codemap

The repository layers a verified USA executable provisioner, a true-oracle crt0 cross-check, and a
generated-code differential through an explicit A(39h) return over the shared psxport framework. There
is no game runtime layer yet; the honest frontier ends at the first subsequent game call.

| Subsystem | Status | Where | Gap / next |
|---|---|---|---|
| Framework consumer | 🟡 generated through modeled BIOS return | `CMakeLists.txt`, `external/psxport/`, `psxport.pin` | Clang-built trace target uses the framework's checked oracle-resume surface; no game seam or loop |
| Target executable | ✅ provisioned, oracle-executed, and port-traced | `tools/provision.py`, `tools/emit_substrate.py`, `titles/ctr/README.md` | Real USA media reproduced `SYSTEM.CNF` and `SCUS_944.26`; symbolic 7/7, pre-BIOS 34/34, post-InitHeap 108/108 |
| Generated substrate | 🟡 resident discovery output | `generated/` (gitignored), `tools/emit_substrate.py` | 1,236 emitted functions support the bounded trace; inventory is not execution proof and unresolved return edges remain |
| Project tooling | 🟡 three-boundary differential | `CMakeLists.txt` (`verify`, `oracle_boot_check`, `ctr04_check`, `ctr04_post_init_heap_check`), `tools/{provision,emit_substrate,compare_crt0_trace}.py` | Asset-free verification owns policy/both-answer selftests; explicit gates own repeat-oracle determinism and real generated execution |
| Native engine | ⬜ missing | `game/core/crt0_port_trace.cpp` is a harness only | No game seam, host owner, or frame loop |
| Native graphics producers | ⬜ missing | — | No producer exists |
| Widescreen | ⬜ missing | — | Blocked on native camera and producers |
| Interpolation | ⬜ missing | — | Blocked on PC ownership of transform producers |
| Differential harness | 🟡 through A(39h) return | framework `oracle_trace`/`crossvalidate_crt0.py`; CTR `crt0_port_trace.cpp`/`compare_crt0_trace.py`; CMake `oracle_boot_check`/`ctr04_post_init_heap_check` | Two oracle runs are identical; oracle vs generated agrees 108/108 at `0x8003C58C`; forced post-return `gp` disagreement is detected |

## Where is X?

- Target identity and load map: `titles/ctr/README.md`
- Disc resolution, transactional extraction, and identity gate: `tools/provision.py`
- Identity-gated resident emission: `tools/emit_substrate.py`, `game/recomp_seeds.json`
- Generated first-call/modeled-return/post-call capture: `game/core/crt0_port_trace.cpp`, `tools/compare_crt0_trace.py`
- Independent real-crt0 execution and cross-check: `CMakeLists.txt` (`oracle_boot_check`)
- Oracle-to-generated boundary gates: `CMakeLists.txt` (`ctr04_check`, `ctr04_post_init_heap_check`)
- Framework-only build target: `CMakeLists.txt` (`ctr_scaffold`)
- Normal build/style/lint/smoke gate: `CMakeLists.txt` (`verify`)
- Ordered RE dependency chain: `docs/re-frontier.md`
- Symptom/finding history: `docs/issues/`
