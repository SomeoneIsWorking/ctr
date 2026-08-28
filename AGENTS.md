# CTR project specifics

The workspace authority in `../AGENTS.md` and framework-consumer authority in
`external/psxport/AGENTS.md` apply here. This file records only CTR-specific ownership decisions.

- The selected retail target is USA `SCUS_944.26`; all disc/executable inputs remain gitignored and
  every binary-derived claim crosses `tools/provision.py`'s complete identity gate.
- The host architecture follows the Dusklight reference named by the workspace authority, by
  responsibility: `CtrRuntime` composes process-level facts and products; simulation will own
  camera/object transforms; a native renderer will consume producer-owned primitives; temporal
  interpolation will decorate previous/current simulation state at presentation time. Do not
  interpolate GTE SXY/ordering-table output or use it as a native producer input.
- `game/core/platform_hle_plan.{h,cpp}` owns CTR's measured libgte leaves, stock libcd
  CdSync/CdControl leaves, fatal retail VSync address, and exact admitted windows. Shared native CD
  command effects and the non-replaceable VSync trap stay framework-owned. Recording guest
  projection is evidence plumbing, not widescreen or renderer ownership.
- `game/core/frame_driver.{h,cpp}` owns every finite product advance into retail main. The first step
  crosses startup; later steps resume at the state-3 return `0x8003CEB4`. Generated supers preserve
  CTR's measured frame order through owner `0x80035E70`, while the current title bridges omit three
  direct startup/teardown VSync calls and its conditional debug VSync. It also owns the statically
  reachable state-0 `0x80031FDC` VSync(2) path as a two-field finite wait plus preserved continuation.
  The emitted retail denominator is 33 direct sites across 20 owner functions; all other unextracted
  sites remain fatal and issue 0015 stays open until the product run proves the reachable set. Each
  completed step ends when the owner
  restores `0x8003CEB4`; the driver advances CTR's native field counter and samples input once at
  that finite host boundary. This is a repeating, unpresented fence, not native-renderer ownership.
- `game/core/async_disc_owner.{h,cpp}` owns the measured stock libcd CdRead leaf `0x80076F10`: it runs
  psxport's shared synchronous transfer and then dispatches whatever callback the retail
  `CdReadCallback` slot `0x8008AD10` holds, with libcd's `CdlComplete`, restoring the interrupted
  register context. It transcribes no callback effects; the retail body owns them. Without that
  delivery the resource loader's busy flag at `gp+0x138` never clears and every field presents black
  (issue 0019). `game/core/dma_callback_owner.{h,cpp}` owns only the measured channel-4
  libapi slot and preserves DICR, `in_irq`, R3000, and chained-completion ordering. The safe delivery
  seam is startup loop `0x8003C94C`; B0:17 unwinds through `0x80077348`, so code after that generated
  super is unreachable and must not be reinstated.
- `game/video/projection_owner.{h,cpp}` owns the measured pre-GTE projection publication at
  `0x80042910` and checks its preserved generated super against the view input. It records
  previous/current projection facts only; it is not a widescreen camera or interpolation source.
- `game/video/presentation_owner.{h,cpp}` owns exactly one `commitUnpresented` at the measured frame
  return. Replace it with a real commit only after a live run proves a visible captured queue and its
  field cadence.
- `game/core/runtime_composition.{h,cpp}` is the single production/test path that registers title
  owners, binds the direct DiscState to `PSXPORT_CTR_DISC`, and installs the title-declared render
  path. `FrameLoopShell::prepareProduct` is the sole final PlatformHLE/native-frame-loop preflight so
  it reinstalls the fatal VSync trap after title hooks. Capability bits describe implemented owners,
  not target scope: CTR currently exposes GTE only and refuses Native/FPS60 until their products
  exist.
- CTR's overlays are entries inside `BIGFILE.BIG`, not separate disc files.
  `tools/extract_overlays.py` owns that layer: archive identity, index parsing, and slicing the
  entries `game/recomp_seeds.json` names into `scratch/raw/ctr/overlays/BF<id>.BIN` at their exact
  index byte size. A stem names an ARCHIVE ENTRY, never a run; slicing the sector-padded image
  instead makes adjacent modules overlap. `tools/emit_substrate.py` runs it and refuses when a
  declared overlay has no image — never emit without overlays, because the product then fails much
  later as an unexplained recomp-MISS. New load bases come from `PSXPORT_DEBUG=cd` over a real boot.
- `tools/measure_render_frontier.py` is the authority for the current static projection/primitive
  boundary. Dynamic render ownership requires a serialized live trace after the boot frontier; do
  not launch concurrently with another game instance in the workspace.
