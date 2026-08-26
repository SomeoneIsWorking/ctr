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
(`ra=0x8003C5D8`), again detecting a forced `gp` disagreement as 33/34. The next decompile proves
that service's startup idle path from four exact executable-backed values and reaches `0x8001D06C`
(`ra=0x8003C5E0`) with deterministic 34/34 agreement; its forced control is 33/34. The next service
returns on its BSS-zero pending word, state zero selects exact jump-table entry `0x8003C614`, and the
generated/oracle boundary agrees 34/34 at executable A(2Bh) thunk `0x800718BC`
(`ra=0x8003C624`), again with a 33/34 forced control. The later chain explicitly models that memset,
crosses executable swap and indirect dispatch, and agrees 34/34 at target `0x800772E0`; its forced
register mismatch produces the sole 33/34 mismatch. Candidate initializer-device and poisoned
zero-fill gates remain implemented, but clean framework commit `99a42aa3` lacks their required
`oracle_trace --capture-devices` interface. They therefore do not currently verify `0x800777E8` or
`0x80080260`; issue 0014 tracks the shared dependency and required reruns.

The same identity gate now anchors the first render-source investigation. Retail
`SetGeomScreen [0x8007781C,0x80077828)` and `SetGeomOffset [0x8007782C,0x80077844)` are called by
boot setup (OFX=256, OFY=120, H=320) and view-derived producer `[0x80042910,0x80042974)`. Ghidra
decompilation identifies `[0x80024C4C,0x80025138)` as an address-registered `lensflare` callback
which projects three triangles, writes four GPU packets, and links them into the ordering table.
The complete text-word census contains 17 CR24 writes, 17 CR25 writes, 16 CR26 writes, 18 RTPS
commands, and 38 RTPT commands. These are static source boundaries only: no execution order, first
visible frame, native renderer, widescreen, or interpolation is claimed.

## Reproduce the measurement

After the root README's normal verifier has built the shipping `discdump`, run:

```sh
python3 tools/provision.py /path/to/CTR-USA.chd
```

Omit the argument to use `PSXPORT_CTR_DISC`, `PSXPORT_DISC`, `.env`, or a root `*.chd` drop-in. The
provisioner reproduces and verifies every executable field above plus the `SYSTEM.CNF` boot target.
No disc-derived file belongs in git, and this does not establish that a CTR port boots.

Run `cmake --build build --target ctr05_render_frontier_check` to reproduce the projection/primitive
measurement without launching the game. The check refuses any executable identity, signature,
caller, or projection-register census other than the facts above.

To reproduce the independent execution after configuring the Clang build, use the root README's
`oracle_boot_check` target. It provisions and verifies this exact executable before the oracle runs.
