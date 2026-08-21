# Crash Team Racing

PC-native PlayStation port of Crash Team Racing, built on
[psxport](https://github.com/SomeoneIsWorking/psxport).

Current status: the USA target executable is measured, its reproducible provisioner is verified on
real media, and an independent Beetle/Mednafen CPU has executed and cross-checked its crt0. The
shipping recompiler now emits the gitignored resident substrate, and the first port-side comparator
agrees with the oracle on all 34 call-boundary fields. There is still no game seam or port boot, and
no hardware-dependent execution or enhancement is claimed.

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
```

`ctr04_check` re-provisions the executable, executes the oracle to its first call, supplies that
observed target to the generated registry, and compares the PC plus all 31 mutable GPRs and `lo`/`hi`.
It then forces one captured `gp` value to the opposite answer and requires a named disagreement. The
generated trace executable also refuses a target absent from the generated registry.

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

Disc images and extracted executables are never committed. The remaining boot frontier is a
generated CTR substrate compared against this independent trace; executing crt0 in the oracle does
not claim that the PC port boots.
