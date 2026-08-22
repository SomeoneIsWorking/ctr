---
id: C008
kind: claim
status: holds
created: 2026-08-22
tags: runtime,inheritance
depends: game/core/ctr_runtime.cpp#CtrRuntime, game/core/crt0_port_trace.cpp#main, tests/test_ctr_runtime.cpp
---

## Claim

CTR installs a direct CtrRuntime derived from GameRuntime before Core construction; it exposes no legacy GameConfig/GameHooks views and routes the immutable validated trace target through bootInit without changing the verified execution boundary.

## Evidence

Clang 22.1.8 built the production runtime and trace harness against psxport 7f5d3f13. ctr_runtime_inheritance passed the installed-Core, null-legacy-view, null-context, and injected dispatch checks; canonical verify passed format 4/4, source caps 4/4, clang-tidy 3/3, psxport smoke 8/8; the full real-disc oracle/generated chain retained 34/34 at 0x80032DC0 with a forced 33/34 opposite.

## What would falsify it

The runtime ceases to derive GameRuntime, a legacy compatibility view becomes non-null, Core is constructed before installing it, bootInit dispatches a target other than the validated immutable target, or any real-disc boundary gate diverges.
