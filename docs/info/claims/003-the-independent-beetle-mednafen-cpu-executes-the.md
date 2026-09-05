---
id: C003
kind: claim
status: holds
created: 2026-08-21
tags: ctr-03,oracle,boot
depends: tools/provision.py, titles/ctr/README.md
reconfirmed: 2026-08-24
verified_at: 2026-08-24 20:09:43
---

## Claim

The independent Beetle/Mednafen CPU executes the measured CTR USA crt0 to its InitHeap boundary and agrees with the symbolic decoder on all seven comparable fields; this proves the first oracle boot window, not a PC port boot.

## Evidence

CMake oracle_boot_check first re-provisioned the SHA-256-bound SCUS_944.26, then oracle_trace left mapped text at step 92378 with pc=0x000000A0. crossvalidate_crt0.py reported 7 agree, 0 disagree, 0 unseen for GP, libcInit target, BIOS function 0x39, InitHeap a0, planned SP, planned a0, and planned a1 heap size.

## What would falsify it

The target accepts an executable other than the measured identity, the independent execution or
symbolic decoder changes any compared field, or the oracle ceases to expose its positive and
hardware-stop answers.

## Re-confirmed 2026-08-24

Repeated identity-bound independent runs retained the 92,378-instruction InitHeap stop, the 7/7
symbolic comparison, and both positive and named hardware-stop fixture answers.
