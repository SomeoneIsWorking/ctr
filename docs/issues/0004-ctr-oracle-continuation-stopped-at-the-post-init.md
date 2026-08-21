---
id: 4
title: CTR oracle continuation stopped at the post-InitHeap callee
status: resolved
symptom: The canonical true oracle could capture 0x8003C58C after modeled InitHeap but intentionally stopped before executing that resident function.
tags: ctr,oracle,resume,crt0
created: 2026-08-21
updated: 2026-08-21
---

## Root cause

The framework continuation surface is deliberately bounded at the first post-return call. Re-entering
the original executable at `0x8003C58C` with registers alone would normally be invalid because the
replay RAM is not the post-crt0 RAM.

## Resolution

Independent Ghidra disassembly proved the exact `0x8003C58C..0x8003C5AC` prefix performs only
register arithmetic and stack stores before its `jal`. The CTR-owned replay builder validates those
exact bytes, restores every GPR plus `lo`/`hi` in an original aligned zero run, excludes the written
stack range, and jumps to that prefix. Canonical `oracle_trace --capture-call 1` remains the sole
call-boundary parser and discovers `0x800779E4`. Any changed prefix, insufficient zero run, or
unavailable exact scratch register refuses. This proves only the first call boundary, not general RAM
continuation.

## Static evidence

The original executable was imported as MIPS little-endian at header base `0x8000F800` so its
payload maps to `0x80010000`. Ghidra `Disasm.py` reported this complete bounded prefix:

```text
8003c58c  addiu sp,sp,-0x40
8003c590  sw ra,0x38(sp)
8003c594  sw s5,0x34(sp)
8003c598  sw s4,0x30(sp)
8003c59c  sw s3,0x2c(sp)
8003c5a0  sw s2,0x28(sp)
8003c5a4  sw s1,0x24(sp)
8003c5a8  jal 0x800779e4
8003c5ac  sw s0,0x20(sp)
```

The tracked byte-word tuple in `compare_crt0_trace.py` is the mechanical stale-proof check for this
evidence; the canonical oracle still owns executed call detection.
