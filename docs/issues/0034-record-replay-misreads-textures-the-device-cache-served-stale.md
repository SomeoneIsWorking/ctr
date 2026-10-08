# Record replay differs from the device where a draw samples VRAM written since gpu.c's texture cache was last invalidated

Fixed 2026-10-08. Observed: Record path recordcheck at 1x 4:3 (`aspect=0`, `ires=1`) through intro, demo races and
menu showed 170 of 21,041 presents with `mismatched>0`, in runs from seq 11086, +-1 in one 5-bit channel, 12-300 pixels.
Expected 0, as Spyro 1/2 and Crash 1.

Reproduction: an offline replay of seq 11083-11085's GP0 words over the VRAM before them reproduced the same 12 pixels
(deterministic, a fresh device). Bisecting the words ended on a flat textured triangle at VRAM rows 380-383 that samples
a 15bpp page at (0,256), which is the displayed buffer's rows 380-384: a screen-feedback effect.

Cause: `psxport/vendor/beetle-psx/mednafen/psx/gpu_common.h: GetTexel_TM*` serves texels through a 256-line cache that only
an upload, copy, read or texpage change invalidates, and fills a line of four texels on the first miss; a draw also reads
pixels it wrote earlier. `psxport/runtime/psx/gpu/record_raster_setup.cpp` + `shaders_gpu/record.frag` sample the VRAM
snapshot taken at batch start, so a textured draw reading written tiles got the fresh or pre-draw texel where the device
got its cached or in-draw one. With the cache bypassed in the offline replay the device still differed from the record
(self-overlap), so the order is intrinsic, not a missing cache model alone.

Fix at the owner (`gpu/gp0_record_tap.cpp: settleLast`, `gpu/texture_feedback.*`): the tap tracks which 64x32 VRAM tiles
draws and fills wrote since the last cache invalidation (GP0 01, copy, upload, read, soft reset, and `SetTPage` changes
of page, 4bpp-or-not or TexDisable, seen on E1 and on every textured polygon's texpage word); a textured primitive whose texture page overlaps a written tile
is recorded as a `VramUpload` of the device's pixels over the primitive's draw bounds, so the replay ends on the device's
picture. Cost: such a primitive is no longer a primitive (not interpolated, not widened, upload-resolution at ires > 1).

Tests: `psxport/tests/test_record_raster.cpp: a_draw_that_samples_its_own_pixels`,
`a_draw_over_a_cached_texture_is_sampled_stale` (failed with 3118 and 570 pixels before, 0 after).

After: 1x 4:3, same route, fps60 off 21,202 presents and fps60 on 27,549 recordcheck lines, 0 mismatched in both. Spyro 2
(`s43`/`s43f`, walk) 2,624 and 2,634 lines, 599 composed, 0 mismatched.

Tests added for the invalidation points: `a_texpage_change_invalidates_the_cache_through_e1` and `..._through_a_polygon`
(the primitive stays a primitive; both failed with an upload before texpage tracking).

Primitives resolved to device pixels (CTR route, fps60 off, 21k frames): 355,020 without texpage invalidation, 97,443
with it. Spyro 2 route: 0 either way.

Open: a resolved primitive does not widen or interpolate, and 97k are still resolved on the CTR route (all after seq 12,000,
the races). `TextureFeedback::overlapsPage` tests the primitive's whole texture page; narrowing it to the UV rectangle the
primitive samples (through the texture window) is the next step, then tile size. A draw area wrapped past row 511 is not
tracked.
