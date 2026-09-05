# Crash Team Racing measured target

The selected target is the North American disc whose `SYSTEM.CNF` names
`cdrom:\SCUS_944.26;1`.

| Field | Measured value |
|---|---|
| Executable | `SCUS_944.26` |
| Disc extent | LBA 24, 516,096 bytes |
| SHA-256 | `7b4aac0bf2f6310984e599295df17b457da5a23b270c20200cefef6079efb838` |
| Entry | `0x8007793C` |
| Load address | `0x80010000` |
| Text extent | `[0x80010000,0x8008D800)` |
| Header stack | `0x801FFFF0` |

Measured crt0 facts include BSS `[0x8008D668,0x8009F6FC)`, GP `0x8008CF6C`, stack top
`0x807FFFF8`, heap base `0x8009F6FC`, heap size `0x007588FC`, and libc initializer `0x80080620`.
The independent CPU reaches InitHeap after 92,378 instructions. Recorded comparison evidence then
reaches `0x8003C58C`, `0x800779E4`, `0x80032DC0`, `0x8001D06C`, `0x800718BC`, and pre-instruction
`0x800772E0`; exact field counts and negative controls remain in `../../docs/re-frontier.md`.

Render-source evidence grounds `SetGeomScreen [0x8007781C,0x80077828)`, `SetGeomOffset
[0x8007782C,0x80077844)`, projection publication `[0x80042910,0x80042974)`, and lens-flare producer
`[0x80024C4C,0x80025138)`. These are source boundaries, not a native renderer.

The product maps this authenticated executable into psxport's Lightrec executor. Runtime activation
of the measured `BIGFILE.BIG` images remains incomplete. Do not emit, build, or run generated guest
code, and do not expose an interpreter mode. See `../../docs/migration.md`.
