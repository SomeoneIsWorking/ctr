# 0027 — The projection census is reported from 1 of the 13 places a field can die

Found while extracting the field owners out of `game/core/frame_driver.cpp` into
`game/core/field_boundary.*`, `startup_resource_load.*`, `startup_resource_pump.*`,
`startup_audio_wait.*`, `frame_suffix.*` and `vsync_bridge.*`. **Not fixed here** — that extraction
was behaviour-preserving by instruction, and this is a diagnostics defect, not a readability one.

## What the code claims

`CtrFrameDriver::refuseUnfinishedField` carries the comment that states the placement rule:

    // A run that cannot finish still measured something, and the projection census is the one
    // measurement that decides whether a native producer has any pre-GTE state to consume at all.
    // Reporting it HERE rather than at a clean shutdown is deliberate: the run that needs the number
    // most is the one that dies, and a report attached to orderly exit would be silent exactly then.
    projection_.reportCensus();
    std::abort();

The rule is right. The implementation is one call site out of thirteen.

## What was measured

`grep -rn "reportCensus" game/` names exactly **1** call site in the whole product. Every other
refusal in the field path reaches `std::abort()` without it:

| refusal | file |
|---|---|
| a pending exit whose reason is not `FrameBoundary` | `game/core/field_boundary.cpp` |
| a bridge reached from an unmeasured return address | `game/core/retail_return.cpp` |
| nested state-zero resource waits | `game/core/startup_resource_load.cpp` |
| the resource-wait suffix failing to return, and both suffix return identities | `game/core/startup_resource_load.cpp` |
| nested resource pumps, and a pump resumed without a measured phase | `game/core/startup_resource_pump.cpp` |
| the resource-pump suffix continuation identity | `game/core/startup_resource_pump.cpp` |
| nested state-zero audio waits | `game/core/startup_audio_wait.cpp` |
| the frame-timing super's return identity | `game/core/frame_suffix.cpp` |
| a suffix resumed with no owned continuation, a suffix failing to return, the suffix return identity | `game/core/frame_suffix.cpp` |
| a budget exit that cannot be resumed, or that consumed nothing | `game/core/frame_driver.cpp` |
| a nested CTR frame driver | `game/core/frame_driver.cpp` |
| an override that could not be installed or removed | `game/core/field_override_scope.cpp` |
| a result that is not a frame boundary, or a callee that did not return | `game/core/ctr_runtime.cpp` |

## Why this matters for the capability ledger, not just for tidiness

`docs/project-state.md` treats the census as the decisive measurement for S005:

> the projection census is the one measurement that decides whether a native producer has any
> pre-GTE state to consume at all

and the recorded "0 of 73,695 prims are 3D" reading rests on census readings. The likeliest
way this port dies is a **return-identity refusal**, because the live frontier in issue 0023 is a
corrupt render list reaching a GTE call, not a timing problem. A run that dies at
`refuseUnexpectedRetailReturn` prints the offending address and nothing else, so the one number that
decides S005 is absent from precisely the run whose evidence matters most — which is the failure
mode the comment above was written to prevent.

## Why it was not fixed here

The obvious fix — call `reportCensus()` before every `std::abort()` — is wrong as stated. The census
owner lives in `ProjectionOwner`, which is a `CtrFrameDriver` member, and eight of the thirteen
refusals are inside owners that do not hold it. Giving every owner a `ProjectionOwner&` to call on
its way out would spread one reporting obligation across eight files, which is the "copy a policy
into a second place" failure rather than a fix.

The shape that does not spread it: one process- or run-scoped teardown that reports the census on
the way to `std::abort()`, reached through a single named refusal, so that the obligation has exactly
one home and the count of sites that can skip it goes to zero. That is a design decision with a real
API consequence, so it is recorded here rather than taken.

## Related

- The `std::abort()` refusals themselves have no automated coverage in this repository, for the same
  reason: an in-process `main()` with no death-test facility. `docs/project-state.md` already
  records this for `ProjectionOwner`'s `std::abort()`.
