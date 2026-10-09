# Replay library

Phase-keyed pad recordings (`PSXPORT_PAD_RECORD`), replayed with `PSXPORT_PAD_REPLAY=<file>`. Each starts at a cold boot on a
blank memory card and is deterministic frame for frame.

## gameplay/

- `arcade-crash-cove-drive.pad`: title, Arcade, Single, 1P, Easy, Crash, Crash Cove, then Cross held for 300 fields, left 150,
  straight 100, right 150, coast 60. The kart (`gGT+0x24EC` pointer, `driver+0x2D4` x/y/z in 1/256 units) ends at
  x=-10278.75 y=188.54 z=-3129.50 at presented field 6,916 (paused and stepped to that exact field; two replays agree). Re-record it
  if the field cadence changes: the file is frame-indexed.
