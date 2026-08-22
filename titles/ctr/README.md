# Crash Team Racing

## Measured target

The selected target is the supplied USA disc image (NTSC-U/C). Its `SYSTEM.CNF` names
`cdrom:\SCUS_944.26;1`, and the root directory places that executable at LBA 24.

| Field | Measured value |
|---|---|
| Executable | `SCUS_944.26` |
| Disc extent | LBA 24, 516,096 bytes |
| SHA-256 (complete PS-X EXE) | `7b4aac0bf2f6310984e599295df17b457da5a23b270c20200cefef6079efb838` |
| Entry (`pc0`) | `0x8007793C` |
| Load address (`t_addr`) | `0x80010000` |
| Text size (`t_size`) | `0x0007D800` bytes |
| Text extent | `[0x80010000, 0x8008D800)` |
| Header stack | `0x801FFFF0` |

The framework's shipping crt0 decoder also measured a complete 8-of-8 boot group: BSS
`[0x8008D668, 0x8009F6FC)`, GP `0x8008CF6C`, stack top `0x807FFFF8`, heap base `0x8009F6FC`, heap
size `0x007588FC`, and libc initialiser `0x80080620`. These values are evidence for the future game
seam; none is wired into shipping game code yet. The independent Beetle/Mednafen CPU subsequently
executed the real crt0 to its InitHeap boundary and agreed with the symbolic decoder on all seven
comparable fields: GP, libc target, BIOS function, InitHeap `a0`, planned SP, planned `a0`, and planned
`a1` heap size. The shipping generated substrate subsequently executed the header entry to that same
first-call boundary and agreed with the oracle on the boundary PC, all 31 mutable GPRs, and `lo`/`hi`
(34/34 fields). The next gate preserves that proof, independently repeats the oracle twice, explicitly
models the observed A(39h) return, and reaches the first subsequent call at `0x8003C58C`; the initial,
modeled-return, and post-return states agree with generated execution on 108/108 fields. This remains a
bounded crt0 continuation, not a port boot. Independent disassembly then proved the exact first
resident prefix has no RAM reads before its call. The canonical oracle and generated execution agree
there on 34/34 fields at `0x800779E4` (`ra=0x8003C5B0`), with a 33/34 forced-opposite control.
Ghidra then proved that runtime initializer's complete path and the caller continuation through the
next call. A byte- and input-checked replay executes both and agrees 34/34 at `0x80032DC0`
(`ra=0x8003C5D8`), again detecting a forced `gp` disagreement as 33/34.

## Reproduce the measurement

After the root README's normal verifier has built the shipping `discdump`, run:

```sh
python3 tools/provision.py /path/to/CTR-USA.chd
```

Omit the argument to use `PSXPORT_CTR_DISC`, `PSXPORT_DISC`, `.env`, or a root `*.chd` drop-in. The
provisioner reproduces and verifies every executable field above plus the `SYSTEM.CNF` boot target.
No disc-derived file belongs in git, and this does not establish that a CTR port boots.

To reproduce the independent execution after configuring the Clang build, use the root README's
`oracle_boot_check` target. It provisions and verifies this exact executable before the oracle runs.
