# Project state

Factual capability coverage for the Crash Team Racing port. Epic intent lives in
`docs/project-goals.md`, atomic work in `docs/issues/`, ownership and placement in
`docs/codemap.md`, and the ordered binary-evidence chain in `docs/re-frontier.md`.

| ID | Capability / observable outcome | State | Dependencies | Goals |
|---|---|---|---|---|
| S001 | The selected USA disc and `SCUS_944.26` executable are reproducibly identified and provisioned | verified | — | G001, G004 |
| S002 | Independent CPU execution establishes deterministic retail boot boundaries | verified | S001 | G001, G004 |
| S003 | The shipping generated product executes the measured resident boot spine | partial | S001, S002 | G001, G004 |
| S004 | CTR projection and primitive-producer source boundaries are grounded in the selected executable | partial | S001 | G002, G003 |
| S005 | CTR frames are produced by a game-state native renderer | missing | S003, S004 | G001, G002, G003 |
| S006 | The native camera and projection support true widescreen | missing | S005 | G002 |
| S007 | Native camera and object transforms are interpolated for presentation | missing | S005 | G003 |
| S008 | The default CTR product reaches sustained playable gameplay with input and audio | missing | S003, S005 | G001 |

## Current focus

S003 is the current focus: restore the independently compared boot spine beyond the exact
pre-instruction boundary at `0x800772E0`, then advance toward the first frame-producing loop. The static
render-source boundary is recorded separately, but dynamic producer and frame ownership cannot be
established before that upstream execution path exists.

## Capability details

### S001 — Reproducible retail input

Evidence: claims C001 and C002 plus instruments I001 and I002 record the complete USA disc boot path,
516,096-byte executable extent, SHA-256 identity, PS-X EXE fields, transactional extraction, and
positive/negative provisioner fixtures. No game bytes are tracked.

### S002 — Independent boot execution

Evidence: claim C003 and instrument I003 record the independent Beetle/Mednafen CPU reaching the
first InitHeap boundary after 92,378 instructions and agreeing with the shipping symbolic decoder on
7/7 comparable fields. The oracle fixture also demonstrates the named hardware-stop answer.

### S003 — Shipping generated boot spine

Claims C013 and C017 record the shipping product observation and freshly emitted generated path. The generated CTR product executes the
identity-verified resident substrate through the explicitly
modeled A(39h) InitHeap and A(2Bh) memset leaves, executable swap, and indirect dispatch. An exact
clean-framework run at `99a42aa3` agrees on all 34 CPU fields at the pre-instruction boundary
`0x800772E0`, including the forced 33/34 negative control. The operator-run exact product then exited
zero after loading `SCUS_944.26`, entering `0x8007793C`, servicing the measured InitHeap call, and
reaching its supported stop at that same address; it explicitly reported that gameplay is unavailable.
Candidate gates for the initializer device
prefix and unchanged retail zero-fill body remain in the tree, and issue 0012 records the resolved
zero-fill observability defect.

Gap: exact `99a42aa3` lacks the oracle `--capture-devices` interface required to reproduce the
initializer device and zero-fill comparisons. Claims C014 and C015 are therefore falsified as
reproducible landed-source evidence, and execution beyond `0x800772E0` is not currently verified.
Issue 0014 tracks the missing shared capture dependency and the required 37/37 and 41/41 reruns. The
later startup path also reaches timer hardware that needs measured semantics; no frame loop, input
loop, or audio-producing gameplay has executed.

### S004 — Projection and primitive source evidence

Claim C016 and instrument I015 identify exact retail `SetGeomScreen` and `SetGeomOffset` leaves, the
view-derived projection producer at `0x80042910`, and address-registered `lensflare` primitive
producer `[0x80024C4C,0x80025138)`. The identity-gated static checker verifies complete signatures,
direct callers, and the CR24/CR25/CR26 plus RTPS/RTPT word census; its asset-free fixture rejects a
changed leaf, removed caller, and added inline projection write. CTR's runtime records the measured
projection calls through framework-owned typed handlers.

Gap: static evidence does not establish execution order, the active camera, a first visible frame,
or which indirect primitive callbacks execute. The 16/16/15 CR24/CR25/CR26 writes outside the two
typed leaves remain dynamically unattributed. Issue 0013 tracks the live attribution boundary.

### S005 — Game-state native renderer

Missing capability: CTR has no PC-owned frame driver, native primitive producer, render queue,
depth/order owner, or visible-frame presentation path. Its runtime declares the
`interpolatedNative()` product target so native/widescreen and temporal controls remain available;
that policy declaration is not evidence that those producers exist. The statically identified
`lensflare` callback is a candidate source boundary only; no guest OT/GP0/framebuffer fallback exists.

### S006 — True widescreen

Missing capability: CTR has no native camera/projection owner that can widen the horizontal view.
The recorded guest OFX/OFY/H values are evidence plumbing, not a widescreen implementation, and
post-projection guest output cannot recover the state required for a wide frame.

### S007 — Interpolated presentation

Missing capability: CTR has no authoritative simulation tick owner or consecutive native camera and
object-transform snapshots. There is therefore no grounded state pair a temporal presentation
decorator can interpolate without re-running guest code or consuming quantized GTE output.

### S008 — Playable default product

Missing capability: the default target terminates at its explicit bounded boot frontier. It does not
reach a visible frame, accept gameplay input, or produce gameplay audio.
