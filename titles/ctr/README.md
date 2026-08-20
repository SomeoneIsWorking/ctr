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
seam; none is wired into shipping code yet.

## Reproduce the measurement

After the root README's Clang configure, set `CTR_DISC` to the untracked CHD, then run:

```sh
CCACHE_DISABLE=1 cmake --build build --target discdump crt0_extract
build/psxport_build/tools/discdump list "$CTR_DISC"
build/psxport_build/tools/discdump get SYSTEM.CNF "$CTR_DISC" scratch/raw/ctr
build/psxport_build/tools/discdump get SCUS_944.26 "$CTR_DISC" scratch/raw/ctr
sha256sum scratch/raw/ctr/SCUS_944.26
build/psxport_build/tools/crt0_extract scratch/raw/ctr/SCUS_944.26
```

No disc-derived file belongs in git. This measurement does not establish that a CTR port boots or
that a recompiled substrate exists.
