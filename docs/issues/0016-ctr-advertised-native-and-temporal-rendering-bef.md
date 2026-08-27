---
id: 16
title: CTR advertised native and temporal rendering before it owned their producers
status: resolved
symptom: The player runtime declared interpolatedNative even though CTR had no game-state primitive producer, visible presentation commit, or temporal transform source
state_items: S004,S005,S006,S007
tags: capabilities,native-renderer,widescreen,interpolation,projection,presentation
created: 2026-08-27
updated: 2026-08-27
---

## Root cause

`CtrRuntime::renderCapabilities()` returned `interpolatedNative()` as a statement of the requested end state rather than an inventory of shipping owners. The player therefore accepted Native and FPS60 even though CTR had only static producer evidence and an unpresented loop fence.

## Resolution

The runtime now declares GTE as its sole player path with Native and temporal interpolation unsupported, and production composition installs that policy through `render_path_install`. The measured dynamic projection function `0x80042910` is now a frame-scoped override with its raw generated super preserved. `ProjectionOwner` captures the pre-GTE view width, height, centre, and screen distance, then refuses if the retail libgte publication disagrees. `PresentationOwner` rotates exactly one framework `commitUnpresented` fence at the measured frame-owner return `0x8003CEB4`. The production-path test proves capability refusal, A/B projection publication, prior/current projection history, and one unpresented fence per finite host step.

## Exact remaining visible frontier

The first fresh-Clang product run loaded the identity-verified executable but stopped during stock
libcd initialization at fatal VSync(-1), before any visible queue or projection caller executed.
The product still has no dynamically proven visible queue, native camera/transform producer, native
primitive producer, native depth/order owner, widescreen camera/culling/layout plan, or temporal
presentation decorator. After issue 0015 advances boot through the native synchronous CD leaves, a
serialized run must prove which `0x8004205C` callback and projection caller execute, whether the
measured frame owner leaves captured queue items at `0x8003CEB4`, and the exact display-field
cadence. Only then may the unpresented fence become `presentation.commit`, and Native/lerp
capability bits remain false until their corresponding owners are implemented.
