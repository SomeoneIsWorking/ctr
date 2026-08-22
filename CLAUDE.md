# Crash Team Racing port

Read `external/psxport/CLAUDE.md` and `external/psxport/docs/workspace/PROTOCOL.md` before work.
Generated code is sacrosanct. Never commit discs, extracted executables, `generated/`, `.env`, or
machine-specific paths. Run artifacts go under `scratch/`, never `/tmp`.

**`external/psxport` is NOT a git submodule** (2026-08-16): it is a symlink to the workspace's shared
framework clone when one exists, or a private clone at this repo's `psxport.pin` on a fresh machine.
`tools/psxport_sync.py --auto` establishes whichever applies; `psxport_sync.py --bump` records the
framework commit this game is built and VERIFIED against, and `--check` fails when the built framework
is not the recorded pin. Framework edits happen in the shared clone (`$PSX/psxport`), never here.

All picture work is RE-driven. Widescreen and interpolation require PC-native graphics producers
reading game state; do not reconstruct pictures from GTE/OT/GP0 output. Establish a faithful,
measurable base before enhancements.

CTR-04 owns one narrow game module: `game/core/crt0_port_trace.cpp` executes the gitignored shipping
substrate through the oracle-observed first call, an explicit consumer-owned A(39h) InitHeap return,
the first subsequent call, and the exact checked replay through runtime initializer `0x800779E4`,
startup service `0x80032DC0`, background service `0x8001D06C`, and the state-zero initialization
call to executable thunk `0x800718BC`.
`tools/emit_substrate.py` is the identity-gated emitter entry point; `tools/compare_crt0_trace.py` owns
the repeat-oracle cross-process diff; `tools/resident_replay.py` owns the bounded exact-state replay.
None is a game loop or permission to guess later addresses or RAM; the modeled external leaf is
separate from generated game code.

`game/core/ctr_runtime.{h,cpp}` follows Dusklight's process-owner/composition boundary through
psxport's `GameRuntime`: one process-lifetime `CtrRuntime` owns the validated generated-code dispatch,
while the trace harness owns only command parsing, capture state, and invocation-scoped boundary
overrides. CTR has no legacy `GameConfig` or `GameHooks`; do not introduce the compatibility adapter
unless a future measured framework fact genuinely requires it.
