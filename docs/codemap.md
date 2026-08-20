# Codemap

The repository currently layers a measured title target over the shared psxport framework scaffold.
There is no game runtime layer yet; the honest frontier is disc provisioning, then a differential
boot harness.

| Subsystem | Status | Where | Gap / next |
|---|---|---|---|
| Framework consumer | 🟡 scaffold | `CMakeLists.txt`, `external/psxport/` | Clang-built `ctr_scaffold` links the smoke target only; no game seam |
| Target executable | 🟡 measured, not integrated | `titles/ctr/README.md` | USA `SCUS_944.26` identity/load map measured (C001); provision it without tracking disc-derived data |
| Project tooling | 🟡 scaffold | `tools/psxport_sync.py`, framework `discdump` / `crt0_extract` | Target instrument validated (I001); no game boot gate or project-local registry CLI yet |
| Native engine | ⬜ missing | — | No `game/` tree or owned game code |
| Native graphics producers | ⬜ missing | — | No producer exists |
| Widescreen | ⬜ missing | — | Blocked on native camera and producers |
| Interpolation | ⬜ missing | — | Blocked on PC ownership of transform producers |
| Differential harness | ⬜ missing | — | Stand up oracle before game logic |

## Where is X?

- Target identity and load map: `titles/ctr/README.md`
- Framework-only build target: `CMakeLists.txt` (`ctr_scaffold`)
- Ordered RE dependency chain: `docs/re-frontier.md`
- Symptom/finding history: `docs/issues/`
