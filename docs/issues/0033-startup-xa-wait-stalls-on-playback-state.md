# Startup XA wait stalls on the playback-state word

Fixed 2026-10-08. State zero waits at `0x8003C94C` on `[0x8008D708]`, which only CTR's libcd callbacks move.

Cause: `psxport/runtime/psx/cd/cdc_native.cpp: drive_consume_sector` sent an XA audio sector the Setfilter
rejected to the data FIFO and raised INT1; Beetle (`cdc.c` `DS_READING`) drops it silently. The extra
INT1s drove CTR's data-ready callback `0x8001C7FC` on sectors the stream never asked for, so the
playback-state machine never reached its finish. `cdc_sector_route` now returns data, selected XA or
dropped XA, and both the read and the peek path use it.

Second cause: `cd_ready_delivery.cpp` ran the guest callback on the interrupted gp/sp. CTR's polygon
renderer uses both as scratch with interrupts enabled, so the callback ran on garbage. The callback now
runs in the BIOS HookEntryInt context (`bios_interrupt_enter_handler_context`).

Third cause (next stall): the SPU channel-4 DMA completion was consumed by the generic IRQ path because the
framework registry held no callback. `SpuDmaCallbackRegistration` publishes the guest's slot.

Tests: `tests/test_cdc_xa_filter.cpp` (route, drive level), `tests/ctr_field_owners.cpp` (registration).

Fourth cause (2026-10-08): `psxport/runtime/psx/cd/cdc_native.cpp: write_request_register` dropped the sector buffer
on a BFRD clear and never emptied the FIFO on a re-assert; Beetle flushes the FIFO on a request write without
bit 7 or 6 and reloads the sector buffer on re-assert. `cd_ready_delivery.cpp` now clears the request register
as libcd's INT1 handler does. Tests: `test_cd_ready_delivery`, `test_cdc_continuous_read`, `test_cdc_bfrd_split_dma`.

Fifth cause: `game/disc/async_disc_owner.cpp: DiscReadOwner` delivered the read-completion callback only at field
start, but the loader polls for it inside one field. `deliverIfDue` now delivers mid-field once the drive time
elapsed (`FrameDriver::dispatchField` budget exit). Tests: `ctr_frame_budget_read_completion`, `ctr_frame_budget_read_not_due`.

Sixth cause: `game/video/presentation_owner.cpp: finishField` skipped `commit` without a GTE capture, so the Record
path never sealed a frame and `frame` stayed 0. It now always commits.

Overlays: BF0229 (LBA 53753, 17 sectors, 0x800A0CB8), BF0230 (LBA 53770, 22 sectors, 0x800AB9F0) and BF0231
(0x800AB9F0) are measured in `titles/ctr/overlays.json`. Alternates share a slot; `tools/extract_overlays.py` no
longer refuses RAM overlap (`OverlayImageOwner` retires the overwritten generation).

Result: CTR presents frames and reaches the intro, demo races and the main menu.

Open: `recordcheck` at 1x 4:3 shows 170 mismatched present lines of ~21,000 (fps60 off) and ~19,500 (on), identical
in both, +-1 per 5-bit channel in 12 to 130 pixel clusters from seq 11086 in 3D-heavy frames. No primitive of the
displayed record covers them; the cause is not pinned (suspect dither or gouraud interpolation in
`shaders_gpu/record.frag` against Beetle, or the hidden-buffer record).
