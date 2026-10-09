# Replay library

Phase-keyed pad recordings (`PSXPORT_PAD_RECORD`), replayed with `PSXPORT_PAD_REPLAY=<file>`. Each starts at a cold boot on a
blank memory card and is deterministic frame for frame.

## gameplay/

- `arcade-crash-cove-drive.pad`: title, Arcade, Single, 1P, Easy, Crash, Crash Cove, then Cross held for 300 fields, left 150,
  straight 100, right 150, coast 60. The kart (`gGT+0x24EC` pointer, `driver+0x2D4` x/y/z in 1/256 units) ends at
  x=-8261.29 y=32.07 z=-5587.13 about 6,485 presented fields in. Re-record it if the field
  cadence changes (issue 0037): the file is frame-indexed.
