---
id: C002
kind: claim
status: holds
created: 2026-08-21
tags:
depends: tools/provision.py#provision
reconfirmed: 2026-08-21
verified_at: 2026-08-21 02:45:45
---

## Claim

CTR provisioning resolves the selected disc, extracts SYSTEM.CNF and SCUS_944.26 transactionally, and accepts only the measured USA executable identity.

## Evidence

tools/provision.py selftest accepted a matching PS-X EXE fixture and rejected a one-byte mutation plus a wrong SYSTEM.CNF target. On the real CTR USA CHD it extracted SYSTEM.CNF naming SCUS_944.26;1 and a 516096-byte executable with SHA-256 7b4aac0bf2f6310984e599295df17b457da5a23b270c20200cefef6079efb838, entry 0x8007793C, load 0x80010000, text size 0x7D800, and stack 0x801FFFF0. A Spider-Man 2 CHD was refused because SCUS_944.26 was absent, and the previously verified output remained unchanged.

## What would falsify it

A supported resolution route selects the wrong candidate, a fresh CTR USA extraction changes SYSTEM.CNF/hash/header fields, a wrong disc or mutated executable is accepted, or tools/provision.py changes without rerunning both-answer and real-disc gates.

## Re-confirmed 2026-08-21 02:40:01

Re-verified after integration: normal Clang verify ran the provisioner's acceptance/rejection selftest, and oracle_boot_check re-provisioned the real USA CHD through the shipping discdump with the recorded SHA-256 before oracle execution.

## Re-confirmed 2026-08-21

Post-landing recheck retains the normal provision selftest and the real identity-bound CTR oracle result: 7/7 fields agreed after 92,378 independent CPU steps.
