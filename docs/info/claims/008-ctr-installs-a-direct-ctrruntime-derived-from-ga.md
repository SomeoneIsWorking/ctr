---
id: C008
kind: claim
status: holds
created: 2026-08-22
tags: runtime,inheritance
depends: game/core/ctr_runtime.cpp#CtrRuntime, game/core/crt0_port_trace.cpp#main, tests/test_ctr_runtime.cpp
reconfirmed: 2026-08-24
verified_at: 2026-08-24 20:09:44
---

## Claim

CTR installs a direct CtrRuntime derived from GameRuntime before Core construction; it exposes no legacy GameConfig/GameHooks views, explicitly reports that guest VRAM is not picture content while CTR has no rendered frame, and routes the immutable validated trace target through bootInit without changing the verified execution boundary.

## Evidence

Clang 22.1.8 built the production runtime and trace harness against psxport 7f5d3f13. ctr_runtime_inheritance passed the installed-Core, null-legacy-view, null-context, and injected dispatch checks; canonical verify passed format 4/4, source caps 4/4, clang-tidy 3/3, psxport smoke 8/8; the full real-disc oracle/generated chain retained 34/34 at 0x80032DC0 with a forced 33/34 opposite.

## What would falsify it

The runtime ceases to derive GameRuntime, a legacy compatibility view becomes non-null, Core is constructed before installing it, CTR claims guest-VRAM picture ownership before it owns a rendered frame, bootInit dispatches a target other than the validated immutable target, or any real-disc boundary gate diverges.

## Re-confirmed 2026-08-24

Clang 22.1.8 canonical verify passed against recorded psxport bc8c8897; ctr_runtime_inheritance proved direct derived install, null legacy views, and explicit false guest-VRAM picture policy without a rendered-frame claim.

## Re-confirmed 2026-08-24

Post-landing verify passed direct CtrRuntime install/null legacy views and production guest-VRAM picture query false on pinned bc8c8897
