---
id: 39
title: The Crash Cove replay ends off its recorded kart position
status: open
tags: replay,determinism
created: 2026-10-14
---

## Observed
`replays/gameplay/arcade-crash-cove-drive.pad` on `92b7d76`: at field 6916 the kart is at
(-8243.6, 24.7, -5649.2); the replay README records (-10278.75, 188.54, -3129.50). Visible and headless
runs agree with each other, so the replay is deterministic on current code.

## Next
Trace which input or field the run first diverges at against the README's recording conditions; update the
README if the recording predates a deliberate timing change, otherwise fix the cause.
