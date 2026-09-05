---
id: C016
kind: claim
status: holds
created: 2026-08-26
tags: ctr05,projection,native-renderer
depends: game/core/native_ownership.h, game/core/platform_hle_plan.cpp#platformHlePlan
---

## Claim

CTR USA contains exact libgte projection leaves at 0x8007781C/0x8007782C, dynamic projection producer 0x80042910, and an address-registered lensflare primitive producer 0x80024C4C which projects three triangles and links four 0x0C-tagged packets into an ordering table

## Evidence

The retained identity-verified measurement checked the exact leaf, producer and registrar
signatures, direct-call sets, and complete CR24/25/26 and RTPS/RTPT word census. Ghidra independently
decompiled 0x80042910, 0x80024C4C, and registrar 0x80025138. The measured addresses are consumed by
the title's native ownership and platform-HLE plans; the removed one-off measurement tool is not a
product dependency.

## What would falsify it

the selected executable identity, any checked signature/caller/census, function extent, Ghidra semantics, or registration path changes; or dynamic execution shows 0x80024C4C is not the registered lensflare primitive producer
