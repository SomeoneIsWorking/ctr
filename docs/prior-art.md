# Prior art

Outside work on CTR USA `SCUS_944.26`; a reference names addresses, our measurement decides behaviour.

Our target: `SCUS_944.26`, SHA-256 `7b4aac0bf2f6310984e599295df17b457da5a23b270c20200cefef6079efb838`, load `0x80010000`.
Address checks ran `decomp_pipeline.py --function-at` on our image (2026-10-13): `0x8007781C` (3 insns), `0x80073398`
(28), `0x80075350` (94), `0x80024C4C` (body to `0x80025137`, matching our lens-flare producer boundary) are all
function entries.

| project | URL | kind | license | target build | build match (addresses checked) | what we may take | notes |
|---|---|---|---|---|---|---|---|
| CTR-tools/CTR-ModSDK | https://github.com/CTR-tools/CTR-ModSDK | modding SDK plus non-matching decomp (Ghidra C per section, `decompile/`) | GPL-3.0 | build `926` = USA retail | matched: `symbols/gcc-syms926.txt` `SetGeomScreen 0x8007781C`, `SetGeomOffset 0x8007782C`, `VSync 0x80075350`, `DrawOTag 0x80073398`, `PushBuffer_SetPsyqGeom 0x80042910`, `FLARE_ThTick 0x80024C4C`, `RenderBucket_Execute 0x8006AAA8`, `start 0x8007793C` | names only | Last push 2026-07-05; README says decomp work moved to CTR-in-C and ctr-native. Our repo has no license, so GPL code stays out. |
| CTR-tools/ctr-native | https://github.com/CTR-tools/ctr-native | native PC port built from the ModSDK decomp | GPL-3.0 | USA `926` via ModSDK | not checked directly (shares ModSDK symbols) | evidence only | Active (2026-10-12). Behavioural reference for owners we replace natively; no code. |
| CTR-tools/CTR-in-C | https://github.com/CTR-tools/CTR-in-C | non-matching decomp, fork of CTR-ModSDK | none in tree; fork of a GPL-3.0 repo | USA `926` | not checked directly (fork of ModSDK) | names only | Has `FUNCTIONS.MD` and `STRUCTURES.MD`. |
| kkv0n/CTR-Matching | https://github.com/kkv0n/CTR-Matching | splat configs and function lists, EXE plus overlays 221-233 | none | NTSC-U `SCUS-94426` (no hash recorded) | matched: `syms/funcs_SCUS_94426.txt` has entries at `0x8007781C`, `0x8007782C`, `0x80075350`, `0x80073398`, `0x80042910`, `0x80024C4C`, `0x8006AAA8`, `0x8007793C` | evidence only | Proof of concept, last push 2026-08-12. Per-overlay function lists cover `BIGFILE.BIG` overlays 221-233; overlay vram 225 `0x8009F6FC`, 226 `0x800A0CB8`, 230 `0x800AB9F0` match `titles/ctr/overlays.json`. |
| CTR-tools/CTR-tools | https://github.com/CTR-tools/CTR-tools | C# format parsers and tools (BIGFILE, LEV, CTR models) | GPL-2.0 | data formats, all regions | not applicable (no EXE addresses) | evidence only | Format reference for `BIGFILE.BIG` and level data. |
| LorenzoAlfaro/CTRStaticRecomp | https://github.com/LorenzoAlfaro/CTRStaticRecomp | static recompilation (MIPS to C, HLE runtime) | MIT | USA `SCUS-94426` plus overlays 221-233 (no hash published) | not checked directly: its function seeds come from ModSDK `syms926.txt` (matched above) | evidence only | Created 2026-10-01. |
| NyperYuhgard/CTR-PC-Port | https://github.com/NyperYuhgard/CTR-PC-Port | native PC port from copies of the ModSDK decomp source | no LICENSE file; carries GPL-3.0 ModSDK-derived code and MIT PsyCross parts (`THIRD_PARTY_NOTICES.md`) | USA via ModSDK | not checked (shares ModSDK symbols) | evidence only | Last push 2026-08-03. |
| mateusfavarin/CrashTeamEditor | https://github.com/mateusfavarin/CrashTeamEditor | track editor | MIT | data formats | not applicable | evidence only (level format) | |
| icebound777/CTR-Randomizer-Standalone | https://github.com/icebound777/CTR-Randomizer-Standalone | randomizer | MIT | not stated | not checked | evidence only | |

Not found (2026-10-13): a matching CTR decompilation (none on decomp.dev; only Crash Tag Team Racing is listed); a
permissively licensed CTR symbol map; IDA or Ghidra databases for `SCUS_944.26` outside CTR-tools. Searched GitHub
repositories ("crash team racing", "ctr decomp", serial), decomp.dev, and the web. A search result proxied through
`*.web.val.run` with credentials in the URL was not opened.
