---
id: C015
kind: claim
status: falsified
created: 2026-08-26
tags: ctr04,zero-fill,memory-boundary
depends: psxport.pin, CMakeLists.txt, tools/compare_crt0_trace.py#main, tools/resident_replay.py#build_replay, game/core/crt0_port_trace.cpp#main, game/core/bootstrap_frontier.cpp#runBootstrapToSupportedFrontier
falsified_on: 2026-08-26
---

## Claim

CTR's retail function `0x800777E8` clears exactly 1,050 words `[0x8008AF98,0x8008C000)` and generated
execution reaches next call `0x80080260` with oracle-identical CPU, device, and memory evidence;
`ctr_port`'s supported frontier is the same proper function boundary.

## Evidence

On identity-verified SCUS_944.26, the replay checked the original callsite, complete 36-byte body,
return continuation, and initially-zero destination before injecting `0xA5` only at the callsite.
Two replay oracle runs were deterministic. Generated execution used the shipping body and agreed on
34 CPU, 3 device, and 4 memory fields (41/41), including zero residual words after 4,200 checked
bytes. Forced `memory.nonzero_words=1` produced exactly one named 40/41 mismatch. The Clang build
rebuilt `ctr_port` with its one-shot frontier at `0x80080260`; no player binary was run.

## What would falsify it

Executable identity, checked callsite/body/return bytes, poison/checker semantics, generated
zero-fill implementation, device or memory schemas, boundary fields, or product frontier changes;
repeated captures differ; the forced residual is not isolated; or `ctr_port` fails to build/reach
exactly `0x80080260`.

## FALSIFIED 2026-08-26

The zero-fill chain includes the initializer device boundary and cannot reproduce through framework
`99a42aa3` because `oracle_trace` lacks `--capture-devices`; prior 41/41 evidence depended on an
unlanded framework worktree.

> Anything that cited this claim as proof must be re-checked. Grep the repo for it.
