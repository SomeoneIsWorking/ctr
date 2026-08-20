---
id: I001
kind: instrument
status: trusted
created: 2026-08-20
---

## Instrument

Clang-built psxport discdump plus crt0_extract for CTR target identity and PS-X EXE load-map measurement

## Validated by

crt0_extract selftest ran 59 checks over two valid shapes and four negative classes, including wrong magic, short input, out-of-image entry, and incomplete prologue. discdump listed 47 files/directories, extracted SYSTEM.CNF and SCUS_944.26 with their declared sizes, and rejected a deliberately absent filename.

## Known failure modes

discdump establishes the filesystem identity of the supplied image, not whether that image is the intended retail revision. crt0_extract recognizes measured PSYQ startup shapes and reports incomplete/refused results for unsupported or malformed shapes; its selftest does not open a commercial executable or prove that a future game configuration uses the reported values. A new disc revision, an unsupported filesystem, or a startup prologue outside the measured shapes requires independent validation.
