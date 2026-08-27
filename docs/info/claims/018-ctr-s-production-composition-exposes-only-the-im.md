---
id: C018
kind: claim
status: holds
created: 2026-08-27
tags: ctr06,projection,presentation,capabilities
depends: game/core/ctr_runtime.cpp#renderCapabilities, game/core/runtime_composition.cpp#installRuntimeOwners, game/core/frame_driver.cpp#publishMeasuredProjection, game/video/projection_owner.cpp#publish, game/video/presentation_owner.cpp#finishUnpresented
---

## Claim

CTR's production composition exposes only the implemented GTE player path, preserves retail projection publication 0x80042910 as an A/B super, and rotates one explicit unpresented fence per finite title step

## Evidence

Fresh Clang ctr_runtime_test exercised the real composition path: Native/temporal capabilities were absent and effective RenderPath was Gte; the measured 320x240/H=512 view publication agreed with its injected retail body, a forced contradictory publication aborted, prior/current sequences advanced, and presentation fence counts were exactly 1/2/3 across startup/repeat/teardown. Fresh shipping ctr_port linked the raw gen_func_80042910 super; CTest passed 4/4 and cpp-policy format/size/clang-tidy passed 10/10. No player binary was launched, so this proves source ownership and capability refusal, not visible gameplay.

## What would falsify it

The title capability profile, render-path installation, admitted 0x80042910 callers, projection input formula, preserved super route, measured 0x8003CEB4 fence, or production transition test changes; or a player run shows the fence/published projection is not the asserted retail boundary
