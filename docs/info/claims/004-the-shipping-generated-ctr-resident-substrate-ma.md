---
id: C004
kind: claim
status: holds
created: 2026-08-21
tags: ctr-04,recompiler,differential
depends: CMakeLists.txt, game/core/crt0_port_trace.cpp, tools/compare_crt0_trace.py, tools/emit_substrate.py
verified_at: 2026-08-21 13:11:01
reconfirmed: 2026-08-21
---

## Claim

The shipping-generated CTR USA resident substrate executes the measured header entry to the same
first-call boundary as the independent oracle, with all 34 compared PC/register fields equal. This is
a pre-BIOS crt0 result, not a claim that the port boots.

## Evidence

The identity gate accepted only SHA-256
`7b4aac0bf2f6310984e599295df17b457da5a23b270c20200cefef6079efb838`. Recompiler version
2026-08-12.1 emitted 1,236 functions across eight shards from the executable-header entry and direct
call discovery with no explicit game seeds. The real-disc `ctr04_check` compared boundary PC, all 31
mutable GPRs, `lo`, and `hi`: 34/34 agreed. Its forced `gp=0` run reported exactly one disagreement,
and the tracer refused a target absent from the generated registry. The emitter separately reported
28 unresolved `lw $ra` bases; this claim covers only the executed first-call window.

## What would falsify it

The executable identity, shipping emitter, seed manifest, generated tracer, framework loader, oracle,
or comparison code changes and the complete real-disc gate is not rerun; any one of 34 fields differs;
the forced opposite is accepted as agreement; or malformed input/non-entry targets cease to refuse.

## Re-confirmed 2026-08-21

Post-landing ctr04_check passed oracle-versus-generated 34/34 fields at 0x80080620; forced gp=0 produced the required named 33/34 disagreement.

## Re-confirmed 2026-08-21

Post-landing Clang build and ctr04_check preserved generated-versus-oracle 34/34 with forced 33/34.

## Re-confirmed 2026-08-21

Re-verified after framework deterministic CD pacing landed at exact ce2c83ad: Clang 22.1.8 rebuilt the shipping generated trace substrate; ctr04_check re-provisioned the identity-bound USA executable, preserved 34/34 oracle/generated first-call agreement, and the forced gp=0 control produced exactly one named disagreement at 33/34.
