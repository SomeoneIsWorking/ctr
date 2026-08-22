# Crash Team Racing

PC-native PlayStation port of Crash Team Racing, built on
[psxport](https://github.com/SomeoneIsWorking/psxport).

Current status: the USA target executable is measured, its reproducible provisioner is verified on
real media, and an independent Beetle/Mednafen CPU has executed and cross-checked its crt0. The
shipping recompiler emits the gitignored resident substrate; the pre-BIOS boundary agrees on 34/34
fields, an explicit A(39h) return continuation agrees through the next call on 108/108 fields, and
bounded resident execution agrees at `0x800779E4`, `0x80032DC0`, and the startup service's next call
`0x8001D06C` on 34/34 fields. The state-zero path then agrees 34/34 at executable initialization
thunk `0x800718BC`. There is still no shipping game loop, and no broader
hardware-dependent execution is claimed.

## Configure the framework scaffold

Configure with Clang before the first verification or after changing CMake inputs:

```sh
python3 tools/psxport_sync.py --auto
CCACHE_DISABLE=1 cmake -S . -B build \
  -DCMAKE_C_COMPILER=/usr/bin/clang \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++
```

The normal gate builds the scaffold and shipping `discdump`, checks the recorded framework pin, runs
the shared first-party C++ policy plus the provisioner's both-answer selftest, and executes the
framework smoke test:

```sh
CCACHE_DISABLE=1 cmake --build build --target verify
```

The shared policy format-checks, size-checks, and runs clang-tidy over the port-side trace translation
unit using the real compile command. Generated and vendored code remain excluded.

## Provision the USA executable

Pass the untracked CHD directly, or omit it to resolve `PSXPORT_CTR_DISC`, `PSXPORT_DISC`, `.env`,
then a deterministic root `*.chd` drop-in:

```sh
python3 tools/provision.py /path/to/CTR-USA.chd
```

The tool extracts `SYSTEM.CNF` and `SCUS_944.26` transactionally into `scratch/raw/ctr/`, then
refuses unless the boot target, complete SHA-256, file size, and PS-X EXE header fields match the
measured USA executable. Copy `.env.example` to the gitignored `.env` for a persistent local path.

## Emit and compare the resident substrate

After provisioning, emit from the identity-checked executable with the shipping psxport recompiler:

```sh
python3 tools/emit_substrate.py
```

The tracked seed manifest is deliberately empty: the executable header supplies the measured entry,
and the shipping emitter discovers direct calls. Reconfigure after the first emit so CMake sees the
generated source manifest, then build the asset-gated comparator:

```sh
CCACHE_DISABLE=1 cmake -S . -B build \
  -DCMAKE_C_COMPILER=/usr/bin/clang \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++
PSXPORT_CTR_DISC=/path/to/CTR-USA.chd \
  CCACHE_DISABLE=1 cmake --build build --target ctr04_check
PSXPORT_CTR_DISC=/path/to/CTR-USA.chd \
  CCACHE_DISABLE=1 cmake --build build --target ctr04_post_init_heap_check
PSXPORT_CTR_DISC=/path/to/CTR-USA.chd \
  CCACHE_DISABLE=1 cmake --build build --target ctr04_resident_next_call_check
PSXPORT_CTR_DISC=/path/to/CTR-USA.chd \
  CCACHE_DISABLE=1 cmake --build build --target ctr04_runtime_init_next_call_check
PSXPORT_CTR_DISC=/path/to/CTR-USA.chd \
  CCACHE_DISABLE=1 cmake --build build --target ctr04_startup_service_next_call_check
PSXPORT_CTR_DISC=/path/to/CTR-USA.chd \
  CCACHE_DISABLE=1 cmake --build build --target ctr04_startup_memset_thunk_check
```

`ctr04_check` re-provisions the executable, executes the oracle to its first call, supplies that
observed target to the generated registry, and compares the PC plus all 31 mutable GPRs and `lo`/`hi`.
It then forces one captured `gp` value to the opposite answer and requires a named disagreement. The
generated trace executable also refuses a target absent from the generated registry.

`ctr04_post_init_heap_check` depends on that pre-BIOS proof, then runs the independent oracle twice,
requires identical step/state evidence, applies only the framework's explicit A(39h) `v0=0` leaf contract, and compares the initial
call, modeled return, and first subsequent call against generated execution. The real CTR image agrees
on 108/108 fields at post-return call `0x8003C58C`; the opposite-answer pass forces `post.gp=0` and
requires the named 107/108 disagreement.

`ctr04_resident_next_call_check` proves the next, deliberately narrow window. Independent Ghidra
disassembly established that `0x8003C58C..0x8003C5AC` has no RAM reads before its first call, only
register arithmetic and stack stores. The replay builder refuses unless those exact original bytes
still match, restores the complete true-oracle register state in an original aligned zero run, and
uses canonical `oracle_trace --capture-call 1`—not a CTR call parser—to discover `0x800779E4`. Two
oracle replays are identical and generated execution agrees on all 34 boundary fields; forced
`resident.gp=0` is detected as 33/34. The proof ends at that call because later code may read RAM
whose post-crt0 contents have not been replayed.

`ctr04_runtime_init_next_call_check` executes that first resident call as well. Ghidra established
that `0x800779E4` is a one-time runtime initializer: on this executable its constructor count is zero,
it changes initialized-data flag `0x8008C050` from zero to one, returns, and the caller reads initial
mode word `0x8008D0F4` before calling `0x80032DC0`. Both inputs precede the BSS cleared by crt0 and are
zero in the identity-checked executable. The replay validates all three non-contiguous code ranges,
both initialized-data words, and every byte written by the two stack frames. Two oracle replays are
identical and generated execution agrees on all 34 boundary fields; forced `resident.gp=0` is detected
as 33/34. Ghidra then established `0x80032DC0`'s startup idle path: request word `0x8008D0A0` is 1,
loading flag `0x8008D708` and timestamp `0x8008D0A8` are zero, and the packed request count is zero.
The replay checks those values plus every executed instruction island, then agrees 34/34 at the next
call `0x8001D06C`; its forced `gp=0` control reports 33/34. That callee's pending-work word
`0x8008D6B8` is BSS zero, so it returns without calling its optional worker. Mode word zero selects
jump-table word `0x80011594`, which points to `0x8003C614`; initialized pointer word `0x8008D2AC`
supplies destination `0x80096B20`. The next gate checks both executed islands, the complete state
dispatch, the case-zero callsite, and those exact inputs, then agrees 34/34 at `0x800718BC` with a
forced 33/34 opposite. That thunk jumps to external BIOS A(2Bh) `memset`; the proof stops before it
because later execution requires its RAM side effect, not a guessed return.

## Cross-check the first boot window in the independent oracle

After configuring the Clang build, run the asset-gated oracle target with the same disc-resolution
routes as the provisioner:

```sh
PSXPORT_CTR_DISC=/path/to/CTR-USA.chd cmake --build build --target oracle_boot_check
```

This target re-provisions and identity-checks the executable, runs the oracle's permanent positive
and negative program classes, then executes the real CTR crt0 in the vendored Beetle/Mednafen CPU.
The independent execution is compared by code against the framework's symbolic crt0 decoder. It is
kept out of the normal asset-free `verify` target because a fresh clone must not require copyrighted
media.

`ctr_scaffold` and its smoke test only prove that the game-agnostic framework links. They do not
launch Crash Team Racing. See `titles/ctr/README.md` for the measured target and
`docs/re-frontier.md` for the ordered work required before a boot claim is possible.

Disc images and extracted executables are never committed. The remaining boot frontier is the
external A(2Bh) RAM mutation reached through thunk `0x800718BC`; this bounded continuation does not
claim that the PC port boots.
