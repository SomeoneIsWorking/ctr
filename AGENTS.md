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
- `game/core/projection_hle_plan.{h,cpp}` owns only CTR's measured libgte leaf addresses and admitted
  retail window. Generic handlers stay framework-owned. Recording guest projection is evidence
  plumbing, not widescreen, native-renderer, camera, or frame ownership.
- `tools/measure_render_frontier.py` is the authority for the current static projection/primitive
  boundary. Dynamic render ownership requires a serialized live trace after the boot frontier; do
  not launch concurrently with another game instance in the workspace.
