# Codemap

The repository layers a verified USA executable provisioner, a true-oracle crt0 cross-check, and the
first generated-code differential over the shared psxport framework. There is no game runtime layer
yet; the honest frontier ends at the first libc/BIOS call boundary.

| Subsystem | Status | Where | Gap / next |
|---|---|---|---|
| Framework consumer | 🟡 first generated boundary | `CMakeLists.txt`, `external/psxport/`, `psxport.pin` | Clang-built trace target links the shipping substrate against verified framework pin `2b5ef7b5`; no game seam or loop |
| Target executable | ✅ provisioned, oracle-executed, and port-traced | `tools/provision.py`, `tools/emit_substrate.py`, `titles/ctr/README.md` | Real USA media reproduced `SYSTEM.CNF` and `SCUS_944.26`; C002/I002. Oracle symbolic cross-check is 7/7 (C003/I003); generated boundary is 34/34 (C004/I004) |
| Project tooling | 🟡 first differential | `CMakeLists.txt` (`verify`, `oracle_boot_check`, `ctr04_check`), `tools/{provision,emit_substrate,compare_crt0_trace}.py` | Asset-free verification owns policy and both-answer selftests; explicit asset gates own real provisioning/oracle/generated execution |
| Native engine | ⬜ missing | `game/core/crt0_port_trace.cpp` is a harness only | No game seam, host owner, or frame loop |
| Native graphics producers | ⬜ missing | — | No producer exists |
| Widescreen | ⬜ missing | — | Blocked on native camera and producers |
| Interpolation | ⬜ missing | — | Blocked on PC ownership of transform producers |
| Differential harness | 🟡 first three-way window | framework `oracle_trace`/`crossvalidate_crt0.py`; CTR `crt0_port_trace.cpp`/`compare_crt0_trace.py`; CMake `oracle_boot_check`/`ctr04_check` | Symbolic vs oracle agrees 7/7; oracle vs generated agrees 34/34 and forced `gp` disagreement is detected. Next boundary is after InitHeap/BIOS semantics |

## Where is X?

- Target identity and load map: `titles/ctr/README.md`
- Disc resolution, transactional extraction, and identity gate: `tools/provision.py`
- Identity-gated resident emission: `tools/emit_substrate.py`, `game/recomp_seeds.json`
- Generated first-call capture and comparator: `game/core/crt0_port_trace.cpp`, `tools/compare_crt0_trace.py`
- Independent real-crt0 execution and cross-check: `CMakeLists.txt` (`oracle_boot_check`)
- Oracle-to-generated boundary gate: `CMakeLists.txt` (`ctr04_check`)
- Framework-only build target: `CMakeLists.txt` (`ctr_scaffold`)
- Normal build/style/lint/smoke gate: `CMakeLists.txt` (`verify`)
- Ordered RE dependency chain: `docs/re-frontier.md`
- Symptom/finding history: `docs/issues/`
