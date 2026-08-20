---
id: 1
title: CTR RE-frontier tool parses zero steps from the prose-only roadmap
status: resolved
symptom: re_frontier.py next reports zero ready and zero blocked steps even though docs/re-frontier.md lists seven numbered tasks; check correctly refuses a zero-entry parse
tags: workflow,re-frontier,registry
created: 2026-08-20
updated: 2026-08-20
---

## Root cause

`docs/re-frontier.md` was a prose numbered list, while `re_frontier.py` only indexes structured heading/field entries.

## What was tried / dead ends

The zero-entry parse was not accepted as an empty roadmap: `re_frontier.py check` correctly failed it, which ruled out treating `next` alone as evidence that no work remained.

## Resolution

### Resolution (2026-08-20)
Rewriting the same seven steps as CTR-01 through CTR-07 makes check parse all 7, next name CTR-02, and hacks report zero tracked debt.
