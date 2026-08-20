# Crash Team Racing

PC-native PlayStation port of Crash Team Racing, built on
[psxport](https://github.com/SomeoneIsWorking/psxport).

Current status: the USA target executable is measured, its reproducible provisioner is verified on
real media, and an independent Beetle/Mednafen CPU has executed and cross-checked its crt0. The
project is still a framework scaffold: no extracted executable is tracked, no game seam or generated
substrate exists, and no port boot or enhancement is claimed yet.

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

There is no game translation unit yet, so the shared policy's explicit scaffold mode reports honest
zero-file format, size, and lint denominators. It begins checking files as game code is added.

## Provision the USA executable

Pass the untracked CHD directly, or omit it to resolve `PSXPORT_CTR_DISC`, `PSXPORT_DISC`, `.env`,
then a deterministic root `*.chd` drop-in:

```sh
python3 tools/provision.py /path/to/CTR-USA.chd
```

The tool extracts `SYSTEM.CNF` and `SCUS_944.26` transactionally into `scratch/raw/ctr/`, then
refuses unless the boot target, complete SHA-256, file size, and PS-X EXE header fields match the
measured USA executable. Copy `.env.example` to the gitignored `.env` for a persistent local path.

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
