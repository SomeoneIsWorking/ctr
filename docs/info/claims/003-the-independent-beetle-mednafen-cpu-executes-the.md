---
id: C003
kind: claim
status: holds
created: 2026-08-21
tags: ctr-03,oracle,boot
depends: CMakeLists.txt, tools/provision.py
reconfirmed: 2026-08-21
verified_at: 2026-08-21 11:21:05
---

## Claim

The independent Beetle/Mednafen CPU executes the measured CTR USA crt0 to its InitHeap boundary and agrees with the symbolic decoder on all seven comparable fields; this proves the first oracle boot window, not a PC port boot.

## Evidence

CMake oracle_boot_check first re-provisioned the SHA-256-bound SCUS_944.26, then oracle_trace left mapped text at step 92378 with pc=0x000000A0. crossvalidate_crt0.py reported 7 agree, 0 disagree, 0 unseen for GP, libcInit target, BIOS function 0x39, InitHeap a0, planned SP, planned a0, and planned a1 heap size.

## What would falsify it

The target accepts an executable other than the measured identity, the independent execution or symbolic decoder changes any compared field, the oracle ceases to expose its positive and hardware-stop answers, or a generated port trace disagrees at this window.

## Re-confirmed 2026-08-21 02:40:01

Re-verified through the integrated Clang-built CMake target: oracle_spike ran 22/22 positive/negative/stepping/mirroring checks, then the real identity-bound SCUS_944.26 left mapped text at InitHeap after 92378 steps and crossvalidate_crt0 reported 7 agree, 0 disagree, 0 unseen.

## Re-confirmed 2026-08-21

Post-landing recheck retains oracle_spike 22/22 and the real CTR crt0 cross-check at 7 agree, 0 disagree, 0 unseen.

## Re-confirmed 2026-08-21 — CTR-04

The real-disc oracle gate again reported 7/7, and the new independently captured generated boundary
agreed with the oracle on 34/34 register/PC fields.

## Re-confirmed 2026-08-21

Post-landing oracle_boot_check passed the 22/22 oracle fixture and real CTR crt0 symbolic-versus-executed comparison 7/7 at the InitHeap boundary.

## Re-confirmed 2026-08-21

Post-landing oracle boot gate preserved the deterministic pre-BIOS 34/34 boundary and explicit InitHeap continuation.
