---
id: 17
title: CTR project-information query is not self-contained
status: open
symptom: CTR has no tools/info.py, so its required registry brief only runs by borrowing the sibling Spyro checkout's command
state_items: S003
tags: tooling,project-info,portability
created: 2026-08-28
updated: 2026-08-28
---

## Evidence

`tools/` contains no `info.py`, while the project registries live under `docs/`. The required unified query succeeds only through `../spyro/tools/info.py` from the workspace layout. A bare CTR clone therefore cannot run the project-information entry point documented by the workspace workflow.

## Proper owner

Converge the project-information implementation into the shared canonical tool authority and give each port a portable entry point. Do not copy Spyro’s implementation into CTR; that would create a second policy/tool owner.

## Resolution condition

From a bare CTR clone, one documented command queries goals, state, issues, codemap, RE frontier, claims, and instruments without requiring a sibling game checkout or a machine-specific path.
