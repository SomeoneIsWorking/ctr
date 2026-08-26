---
id: I014
kind: instrument
status: trusted
created: 2026-08-26
---

## Instrument

CTR poisoned retail zero-fill comparator

## Validated by

The selftest checks deterministic instrumentation, refusal on changed retail body bytes, complete
memory-schema parsing, and a visible forced residue (32/32 total checks). On identity-verified
SCUS_944.26, two independent replay runs agreed with generated execution on 41/41 fields at
`0x80080260`; forced `memory.nonzero_words=1` changed only that field and reported 40/41. During
validation the first checker omitted the PSX `lw` load delay and reported the wrong answer (1,050
residual words); adding the required `nop` made the known zero result observable, demonstrating that
the instrument can produce both answers.

## Known failure modes

The oracle device-capture support used by this chained window must be present in the selected
`oracle_trace`; a framework build without `--capture-devices` refuses before comparison.
