---
id: C001
kind: claim
status: holds
created: 2026-08-20
tags: target,executable,ctr-01
depends: titles/ctr/README.md
reconfirmed: 2026-08-22
verified_at: 2026-08-22 18:35:48
---

## Claim

The selected CTR USA disc names SCUS_944.26 as its boot executable. That complete PS-X EXE has SHA-256 7b4aac0bf2f6310984e599295df17b457da5a23b270c20200cefef6079efb838, entry 0x8007793C, load 0x80010000, and text extent ending at 0x8008D800. This is disc/executable identity evidence, not a claim that the PC port boots.

## Evidence

SYSTEM.CNF extracted from the provisioned disc names cdrom:\SCUS_944.26;1; discdump reports LBA 24 and 516096 bytes. Fresh extraction matched two older workspace copies by SHA-256. The shipping crt0_extract reported the header/load map and a complete 8-of-8 boot group.

## What would falsify it

A fresh extraction from the selected USA disc changes SYSTEM.CNF, size, SHA-256, pc0, t_addr, or t_size, or proves this image is not the intended retail region.

## Re-confirmed 2026-08-21

Post-landing real USA provisioning selected SCUS_944.26 and re-verified the recorded SHA-256 and PS-X EXE identity.

## Re-confirmed 2026-08-21

Post-landing verify reprovisioned the selected USA executable and passed the complete identity manifest on psxport 9f1bb927.

## Re-confirmed 2026-08-21

Post-landing ce2c83ad asset gate reprovisioned the real USA executable and revalidated the complete identity contract.

## Re-confirmed 2026-08-22

Fresh real-disc provisioning during the 7f5d3f13 runtime migration selected SCUS_944.26 and reproduced the recorded SHA-256 and executable identity.

## Re-confirmed 2026-08-22

Current identity gate accepted exact 516096-byte SCUS_944.26 with SHA-256 7b4aac0bf2f6310984e599295df17b457da5a23b270c20200cefef6079efb838; the title documentation change only advances the verified execution frontier.

## Re-confirmed 2026-08-22

Post-commit Clang verification passed 87/87 CTest with exact ad5cf802 pin; real SCUS_944.26 oracle/generated windows remained deterministic, comparator controls passed 18/18, the new 0x800718BC boundary agreed 34/34, and forced resident.gp produced the named 33/34 difference.
