# Codemap

The repository layers a verified USA executable provisioner and a true-oracle crt0 cross-check over
the shared psxport framework scaffold. There is no game runtime layer yet; the honest frontier is a
generated CTR substrate compared against the independent reference trace.

| Subsystem | Status | Where | Gap / next |
|---|---|---|---|
| Framework consumer | 🟡 scaffold | `CMakeLists.txt`, `external/psxport/`, `psxport.pin` | Clang-built `ctr_scaffold` links the smoke target against verified framework pin `be381503`; no game seam |
| Target executable | ✅ provisioned and oracle-executed, not port-integrated | `tools/provision.py`, `titles/ctr/README.md` | Real USA media reproduced `SYSTEM.CNF` and `SCUS_944.26`; C002/I002. Independent CPU execution cross-checked the crt0 boundary 7/7; C003/I003 |
| Project tooling | 🟡 scaffold | `CMakeLists.txt` (`verify`, `cpp_policy`, `provision_selftest`, `oracle_boot_check`), `tools/provision.py`, `tools/psxport_sync.py` | Asset-free verification owns policy/provision/smoke; the explicit asset-gated target owns true-oracle crt0 execution |
| Native engine | ⬜ missing | — | No `game/` tree or owned game code |
| Native graphics producers | ⬜ missing | — | No producer exists |
| Widescreen | ⬜ missing | — | Blocked on native camera and producers |
| Interpolation | ⬜ missing | — | Blocked on PC ownership of transform producers |
| Differential harness | 🟡 first independent window | framework `oracle_trace` and `crossvalidate_crt0.py`, wired by CMake `oracle_boot_check` | Real crt0 execution agrees 7/7 with symbolic decode; no generated CTR substrate exists to compare against it yet |

## Where is X?

- Target identity and load map: `titles/ctr/README.md`
- Disc resolution, transactional extraction, and identity gate: `tools/provision.py`
- Independent real-crt0 execution and cross-check: `CMakeLists.txt` (`oracle_boot_check`)
- Framework-only build target: `CMakeLists.txt` (`ctr_scaffold`)
- Normal build/style/lint/smoke gate: `CMakeLists.txt` (`verify`)
- Ordered RE dependency chain: `docs/re-frontier.md`
- Symptom/finding history: `docs/issues/`
