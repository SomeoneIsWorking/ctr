---
id: 22
title: CTR GTE library uses t2 as an alternate link register
status: resolved
symptom: Runtime execution reached interior continuation 0x8006ACE0 from jr t2 at 0x8006C948
state_items: S003,S005
tags: computed-jump,gte,frontier,binary-evidence
created: 2026-08-28
resolved: 2026-08-28
---

## Binary fact

CTR's hand-written GTE library uses `t2` as a return-address register. At `0x8006ACD8`,
`jalr t2,v1` writes continuation `0x8006ACE0` to `t2`; `$ra` remains available for a caller-owned
parameter-block pointer. The reached helper later returns with `jr t2`.

Runtime-built 244-byte-stride parameter blocks contain nine observed interior library entries:
`0x8006A52C`, `0x8006A8E0`, `0x8006AD88`, `0x8006B030`, `0x8006BF30`, `0x8006C948`,
`0x8006D428`, `0x8006D55C`, and `0x8006D59C`. A runtime executor therefore must honor ordinary
guest indirect control flow and the guest register convention; these addresses are evidence, not a
title-specific dispatch table.

## Result

The observed chain repeatedly crossed `0x8006A52C -> 0x8006C948 -> 0x8006ACE0` and later reached
the distinct corrupt-input fault retained in issue 0023. The native/Lightrec product must reproduce
that behavior through dynamic translation before this historical observation can be cited as current
product evidence.
