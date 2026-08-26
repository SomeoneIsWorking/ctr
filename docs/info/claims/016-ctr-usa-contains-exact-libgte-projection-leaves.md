---
id: C016
kind: claim
status: holds
created: 2026-08-26
tags: ctr05,projection,native-renderer
depends: tools/measure_render_frontier.py#measure, game/core/projection_hle_plan.cpp#projectionHlePlan
---

## Claim

CTR USA contains exact libgte projection leaves at 0x8007781C/0x8007782C, dynamic projection producer 0x80042910, and an address-registered lensflare primitive producer 0x80024C4C which projects three triangles and links four 0x0C-tagged packets into an ordering table

## Evidence

tools/measure_render_frontier.py accepts identity-verified SCUS_944.26, checks the exact leaf/producer/registrar signatures and direct-call sets, and reports the complete CR24/25/26 and RTPS/RTPT word census; Ghidra DecompDump independently decompiled 0x80042910, 0x80024C4C, and registrar 0x80025138

## What would falsify it

the selected executable identity, any checked signature/caller/census, function extent, Ghidra semantics, or registration path changes; or dynamic execution shows 0x80024C4C is not the registered lensflare primitive producer
