# Crash Team Racing

PC-native PlayStation port of Crash Team Racing, built on
[psxport](https://github.com/SomeoneIsWorking/psxport).

Current status: the USA target executable is measured, its reproducible provisioner is verified on
real media, and an independent Beetle/Mednafen CPU has executed and cross-checked its crt0. The
shipping recompiler emits the gitignored resident substrate; the pre-BIOS boundary agrees on 34/34
fields, an explicit A(39h) return continuation agrees through the next call on 108/108 fields, and
bounded resident execution agrees at `0x800779E4`, `0x80032DC0`, and the startup service's next call
`0x8001D06C` on 34/34 fields. The state-zero path then agrees 34/34 at executable initialization
thunk `0x800718BC`, explicitly models its A(2Bh) memset, and crosses the first indirect dispatcher
with 34/34 CPU agreement at `0x800772E0` on a clean landed framework. Candidate gates
for the initializer's IRQ/DPCR prefix and poison-checked retail zero-fill remain implemented, but
the landed framework lacks their required oracle device-capture interface, so they are blocked and
do not extend the current reproducible frontier. The shipping `ctr_port` product is configured to
boot the verified executable through a one-shot stop at `0x800772E0`; this no-launch batch rebuilt that
product but did not re-observe the product run. An identity-gated static render check now locates CTR's two libgte projection
leaves, its view-derived projection producer, and an address-registered lens-flare primitive
producer; the runtime records projection calls through framework-owned handlers. This is not a
visible frame or native renderer. CTR explicitly declares its interpolated-native target so the
framework retains the native/widescreen and temporal controls while that implementation advances.
Gameplay, widescreen, interpolation, and broader
hardware-dependent execution are not implemented yet.

## Run the current product

Install `uv` plus the native C/C++ dependencies, then provide the CTR USA CHD through
`PSXPORT_CTR_DISC`, `PSXPORT_DISC`, `.env`, or a root `*.chd` drop-in and run:

```sh
./run.sh
```

Zero arguments provision and verify the executable, emit the resident substrate, build only the
shipping `ctr_port` target, and launch it. `run.sh` is a slim `uv run --frozen` shim; all setup uses
the same locked interpreter. The launcher accepts any selected C/C++ compiler that passes its C11
and C++20 capability probes; it has no compiler identity whitelist or blacklist. Use
`./run.sh --prepare-only` for a non-launching cold-path check or `./run.sh --headless` for the bounded
no-window product path.

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

## Measure the render frontier without launching

After provisioning the measured executable, run the identity-gated static check:

```sh
cmake --build build --target ctr05_render_frontier_check
```

It checks the complete retail bodies and callers of `SetGeomScreen`/`SetGeomOffset`, projection
producer `[0x80042910,0x80042974)`, the address-taken `lensflare` producer
`[0x80024C4C,0x80025138)`, its registration wrapper, three RTPT sites and ordering-table tag writes,
plus the whole raw CR24/CR25/CR26 word census. The corresponding asset-free selftest proves that a
changed leaf, removed caller, or added inline projection write produces the opposite answer.

The measured runtime plan records setter calls under exact half-open window
`[0x8007781C,0x80077844)`. It deliberately does not claim the other 16/16/15 raw CR24/CR25/CR26
writes, execution order, camera ownership, native primitive ownership, or a visible frame. Those
require one serialized live capture after the boot/hardware frontier advances. Widescreen then
belongs in the native camera/projection producer, and interpolation belongs between previous/current
simulation transforms and presentation—not in quantized GTE output.

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
PSXPORT_CTR_DISC=/path/to/CTR-USA.chd \
  CCACHE_DISABLE=1 cmake --build build --target ctr04_startup_zero_fill_next_call_check
```

The final target currently refuses on exact clean framework commit `99a42aa3`, whose `oracle_trace`
does not expose its required `--capture-devices` option. Issue 0014 tracks that shared dependency.

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
forced 33/34 opposite. Later gates explicitly model that A(2Bh) RAM side effect, cross executable
swap, and reach indirect dispatcher target `0x800772E0` with 34/34 agreement plus a 33/34
forced-opposite control. The initializer-device and zero-fill targets remain available as candidate
gates, but exact clean framework commit `99a42aa3` refuses their required
`oracle_trace --capture-devices` option. Issue 0014 tracks landing that shared dependency and
rerunning the 37/37 and 41/41 comparisons; until then, neither `0x800777E8` nor `0x80080260` is part
of the reproducible frontier.

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

Disc images and extracted executables are never committed. The current reproducible pre-instruction
frontier is indirect dispatcher target `0x800772E0`. Candidate device and poisoned-zero-fill replay
code is deliberately retained, but it is blocked on the missing landed oracle capture dependency;
the configured bounded product does not claim `0x800777E8`, `0x80080260`, or gameplay.
