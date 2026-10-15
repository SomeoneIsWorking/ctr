# Features — CTR: Crash Team Racing

Title hops only. Each row is where CTR joins a psxport feature hop (`psxport F-X.n`, psxport
`docs/features.md`); a feature with no row runs on the framework alone.

## F-BOOT

| hop | joins | owner |
|---|---|---|
| 1 | psxport F-BOOT.1 | `game/product/catalog.cpp:createCatalog` — the `TitleIdentity` from `titles/ctr/executable.json` and a `ctr::Runtime` |
| 2 | psxport F-BOOT.6 | `game/runtime/runtime.cpp:Runtime::registerOverrides` — `StockVSync` `0x80075350` (counter `0x8008C754`), `StockLibgte` `0x8007782C`/`0x8007781C`, `GuestInterruptLibcd`, then the platform leaves |
| 3 | psxport F-BOOT.6 | `game/runtime/session.cpp:Session` — per-Core `SceneCut` and `DiscReadOwner` |

## F-TURN

| hop | joins | owner |
|---|---|---|
| 1 | psxport F-TURN.2 | `game/frame/frame_driver.cpp:FrameDriver::stepFrame` — field overrides, host work, the owed continuation, one field from the admitted entry or `0x8003CEB4`, one commit |
| 2 | psxport F-TURN.2 | `game/frame/frame_suffix.cpp:FrameSuffix::resume` — the retail suffix `0x80037880` once its DrawSync and two-field VSync waits clear |

## F-CD

| hop | joins | owner |
|---|---|---|
| 1 | psxport F-CD.1 | `game/runtime/runtime.cpp:Runtime` — stock `CdReadSync` `0x800770AC`, ready slot `0x8008C420` delivered by the guest's INT1 with status 1 |
| 2 | psxport F-CD.1 | `game/disc/async_disc_owner.cpp:readWithCompletionCallback` — `CdRead` `0x80076F10`: the stock transfer, then the completion callback `[0x8008AD10]` owed at the drive's read time |
| 3 | psxport F-CD.1 | `game/runtime/runtime.cpp:Runtime` — stock `CD_cw` `0x8007BC38`; `game/execution/platform_leaves.cpp:cdSync` — `0x8007B6F0` |
| 4 | psxport F-CD.3 | `game/disc/overlay_image_owner.cpp:OverlayImageOwner` — BIGFILE entries published as image generations after their callback |

## F-RENDER

| hop | joins | owner |
|---|---|---|
| 1 | psxport F-RENDER.1 | `game/runtime/runtime.cpp:Runtime::renderCapabilities` — Record, no producers |

## F-FPS60

| hop | joins | owner |
|---|---|---|
| 2 | psxport F-FPS60.2 | `game/runtime/runtime.cpp:Runtime::sealedFrameIsCut` — `game/video/scene_cut.cpp:SceneCut` over main state, level id and loading bit |

## F-WIDE

| hop | joins | owner |
|---|---|---|
| 1 | psxport F-WIDE.1 | `game/video/widescreen_owner.cpp:WidescreenOwner::planFor` — the plan from the guest's own view extent |
| 2 | psxport F-WIDE.1 | `game/video/projection_owner.cpp:ProjectionOwner::publish` — `0x80042910` widened after the retail comparison |
| 3 | psxport F-WIDE.1 | `game/video/geometry_projection_owner.cpp:GeometryProjectionOwner::observe` — `H` widened at the GTE perspective op |

Race widescreen (view frustum, HUD anchors) is parked, issue 0038.
