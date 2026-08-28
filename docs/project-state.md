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

S003 is the current focus: the identity-verified product sustains presentation without letting guest
VSync return, and those presented images have now been captured and inspected. They were 0.00%
non-black because the resource loader never completed a single load — the retail libcd
read-completion callback was never delivered (issue 0019, claim C019). With the CdRead leaf owner
delivering it, the state-3 screen loader advances 2 -> 3 -> 4 -> 5 and the product fails fast at the
next honest boundary: a recomp-MISS on overlay code at `0x800B0B38`, which the emitter never
discovered (issue 0020). Recompiling the overlays the loader calls is the next dependency for a
visible frame. Independent differential execution still ends at `0x800772E0`; the live product result
does not extend that separate proof window.

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
The product now installs title/platform owners through the same composition path as the asset-free
runtime test, publishes retail VSync `0x80075350` to the mandatory fatal trap, and delegates one
finite transition per host step to `CtrFrameDriver`. Static RE grounds main loop `0x8003C58C`, state-3
frame owner `0x80035E70`, and its return/resume at `0x8003CEB4`. The driver preserves generated
startup, teardown, timing, and frame suffix supers, omits four extracted VSync routes, and the
production-path test advances through startup plus two repeating transitions. Each step rotates
exactly one framework fence; an empty field is explicitly unpresented, while real captured work is
committed through the GTE presenter. This establishes the
shipping ownership boundary; it does not extend independent execution evidence beyond `0x800772E0`.
Candidate gates for the initializer device
prefix and unchanged retail zero-fill body remain in the tree, and issue 0012 records the resolved
zero-fill observability defect.

Gap: exact `99a42aa3` lacks the oracle `--capture-devices` interface required to reproduce the
initializer device and zero-fill comparisons. Claims C014 and C015 are therefore falsified as
reproducible landed-source evidence, and execution beyond `0x800772E0` is not currently verified.
Issue 0014 tracks the missing shared capture dependency and the required 37/37 and 41/41 reruns. The
later startup path also reaches timer hardware that needs measured semantics. Identity-verified
product execution has now crossed the frame transition and first presentation, but sustained input,
visible content, and audio-producing gameplay remain unverified.
The retail denominator is 33 direct VSync sites in 20 owner functions, not four. The driver now
transcribes the exact state-0 `0x80031FDC` fifth-argument-`-1` prefix, turns its VSync(2) into two
finite native fields, and resumes at emitted continuations `0x80032074` plus
`0x8003C8D4`/`0x8003C984`. The first fresh-Clang run instead proved an earlier reachable route:
stock libcd CdSync `0x8007B6F0` called VSync(-1) at return `0x8007B72C`. CTR now declares native
synchronous CdSync/CdControl leaves (`0x8007B6F0`/`0x8007BC38`) without weakening the fatal VSync
trap. A second product run installed all five hardware leaves and passed that boundary, then exposed
a missing `FrameLoopShell::prepareProduct` call before the first step. Main and the production test
now perform that framework-owned preflight and pass the focused Clang gate. Their live rerun is
complete: PID 2571678 passed the CD leaves, product preflight, and InitHeap, then stopped at missing
shared BIOS A0:1A memcmp from caller `0x8001C5D4`. That generic psxport dependency now gates the next
CTR-specific caller. Its bytewise guest-memory implementation and focused Clang test now pass, and
CTR's relinked PID 2590436 advanced to the next retail VSync(-1), return `0x8007705C`, inside stock
libcd CdRead `0x80076F10`. Ghidra identifies three polling calls in that asynchronous controller/IRQ
body; immediate CdReadSync `0x800770AC` owns two more polls. The existing shared synchronous stock-
read owners now expose narrow direct-runtime entries; CTR binds both measured leaves, assigns
`PSXPORT_CTR_DISC` to its DiscState, and passes the shared plus focused Clang tests. Real-disc PID
2665477 live-passed both bindings and opened the CHD before the fatal trap identified the next
VSync(-1), return `0x800750B8`, in libgpu timeout arm `0x800750A8`. Ghidra proves that arm and its
companion check `0x800750DC` use VSync solely as the clock for guest deadline `0x8008AEBC` and poll
counter `0x8008AEC0`. CTR now binds both leaves to the host-owned `Timing::vblank` field counter and
aborts if the synchronous native GPU ever violates its drain contract; the focused Clang test is
green and the next live run is pending. PRESENT and VRAM captures armed for frames 0-4 produced no
files, so no picture was reached. PID 2714292 then passed both GPU-timeout owners and remained live
for a 20-second bound without completing a fence. PID 2718506's short boot watchdog located the
host-starving path exactly at `0x800293B8 <- 0x8002DD24 <- 0x8003C8D4`, after the first finite
resource wait resumed. Ghidra proves `0x8002DD24` contains two asynchronous resource-stage poll
loops. The driver now preserves their generated stage functions and guest frame while yielding one
host field after each false poll, then resumes at emitted continuation `0x8003C8FC`; the focused
Clang test covers both yields and restoration. Real-disc execution crossed that pump and exposed a
later startup-audio dependency: channel 4 becomes owed inside CTR's custom exception exit at PC
`0x8007B104`, return `0x80077494`, but the direct runtime has no legacy DMA callback-table field.
Static and dynamic evidence identify table `0x8008CB08`, SPU slot `0x8008CB18`, libspu wrapper
`0x8007AB34`, and application callback `0x8001C984`. CTR's DMA owner preserves DICR take/ack, HLE
`in_irq` ordering, R3000, and finite chained-transfer deferral. Because B0:17 non-locally unwinds
through `0x80077348`, the safe title seam is startup loop `0x8003C94C` after its wrapper poll. A
real-disc diagnostic crossed that seam, later CD IRQs, and multiple DMA2 GPU completions before
finding a third finite-resource caller at `0x800336F8`; another game process was active, so it is not
serialized product proof. Static retail code proves the same fifth argument `-1`; that continuation
is now emitted and admitted. Serialized PID 3197275 crossed it after 15,086 host-owned fields with
no guest-VSync violation, then correctly returned from that interior suffix before the host had
admitted its sole direct caller continuation. Static generated code grounds `0x8003CC98` as the
unique return in resident main. That exact reentry is now emitted and the owner requires it before
resuming; its production transition test and full Clang gate pass, while a live rerun remains
pending. The next serialized PID 3229162 crossed `0x8003CC98` without guest VSync and exposed the
timing leaf `0x8004B3A4` from its second direct caller at `0x8004B438`. Retail code proves this caller
is a plain elapsed-time wrapper with no adjacent VSync; only the existing `0x8003785C` caller owns
the conditional debug-VSync omission and frame fence. The override now preserves the same generated
timing super and returns normally for `0x8004B438`, while all other returns remain fatal. Focused and
full Clang gates pass. A first clean-fb08d30f rerun exposed that the scoped VSyncCallback owner had
no preserved-super adapter entry for retail `0x80077254`; the adapter and focused contract now name
that exact generated body. Serialized PID 3531982 crossed the corrected boundary, registered
callback `0x80034AA4`, initialized the Vulkan GTE presenter, and submitted a 960x720 headless product
image without reaching guest VSync. The run stopped at that answer, so no screenshot/pixel content
or sustained cadence is claimed. Seventeen other retail VSync sites remain fatal. Issue 0015 keeps
the loop contract open until the image is inspected and later native fields are live-proven.

Fresh integration against clean psxport
`319d30b62ba6bc417bb8edb518b5eaae97750825` is green in a new Clang 22.1.8 build tree using the
locked Python. The complete asset-free CTest set passes 8/8, including the framework-record positive
and mismatch controls. The normal `verify` target passes its 32/32 boot-comparator fixtures, 5/5
render-frontier fixtures, provisioning negative controls, framework smoke 8/8, runtime contract,
format/size policy over 27 files, and clang-tidy on 15/15 compile-backed first-party translation
units. Both real executable help forms print usage and exit zero before asset discovery. The pin,
shared checkout, and selected build record all name that exact framework commit. Issue 0018 records
and resolves the gate defect which initially read a stale fixed build directory instead of the active
CMake record. This remains non-runtime integration evidence; the earlier fb08d30f product run owns
the live frame/presentation result.

Claim C019 records the first inspection of those presented images and what it exposed. Present-stage
and guest-VRAM captures at fields 1-4 both measured 0.00% non-black over 691,200 and 110,592 pixels,
while `PSXPORT_GP0RAW`/`PSXPORT_PRIMDUMP` showed only 40 command words and two degenerate quads per
field: an empty ordering table, not a rasterization fault. Watchpoints on retail `gp`+0x188/0x18C
proved resident main entered state 3 once and its screen loader `0x80033610` then returned stage 2 for
161,226 consecutive fields. The cause was measured, not inferred: the loader busy flag at `gp`+0x138
never cleared because `FUN_800321B4` registers a libcd read-completion callback and relies on the CD
interrupt, and psxport's native synchronous CdRead delivered nothing — slot `0x8008AD10` held
`0x80032110` for the rest of the run. CTR now owns the CdRead leaf, running the shared transfer and
dispatching the exact registered retail callback with `CdlComplete`; the previous hand-transcribed
`0x80032594` owner is superseded and removed. A live real-disc rerun advanced the loader 2, 3, 4, 5
across fields 1-4 and then failed fast on a recomp-MISS at overlay address `0x800B0B38` (caller
`0x800368BC`), which the emitter reported as `0 overlay module(s)`. Issue 0019 records the resolved
stall.

Claim C020 then closes that overlay gap. CTR's overlays live inside `BIGFILE.BIG`, whose identity is
now verified and whose index parses as 608 monotonic (sector offset, byte size) entries;
`tools/extract_overlays.py` slices the entries the seed file names, and `emit_substrate.py` refuses
rather than emitting without them. The three measured loads — archive entries 225, 226 and 233 to
`0x8009F6FC`, `0x800A0CB8` and `0x800AB9F0` — recompiled as 1, 270 and 152 functions. The next
real-disc run resolved `0x800B0B38`, ran the loader's five pointer-only completion callbacks (seeded
after the miss moved to `0x80031B00`), performed four further module loads, and reached retail main
state 1. It then faulted inside scratchpad helper `0x8006D79C` on a null object pointer, which issue 0021
traces to the DELIVERY TIME of the callback added in issue 0019: retail cannot run it inside CdRead,
because the loader stores its allocated buffer into the queue entry only after the read call returns,
and the completion chain reads that same field to run the module's relocation pass. The owner now
records an owed completion and delivers it at the title's per-field seam, and before any subsequent
read. With that ordering the product passes the null dereference, performs its later module loads,
presents at least 16 fields, and reaches CTR's hand-written GTE assembly library, where it fails fast
on an unresolved computed-jump continuation (`0x8006ACE0` from `jr t2` at `0x8006C948`, issue 0022).
Fields are still black, so no visible content is claimed.

### S004 — Projection and primitive source evidence

Claim C016 and instrument I015 identify exact retail `SetGeomScreen` and `SetGeomOffset` leaves, the
view-derived projection producer at `0x80042910`, and address-registered `lensflare` primitive
producer `[0x80024C4C,0x80025138)`. The identity-gated static checker verifies complete signatures,
direct callers, and the CR24/CR25/CR26 plus RTPS/RTPT word census; its asset-free fixture rejects a
changed leaf, removed caller, and added inline projection write. CTR's runtime records the measured
projection calls through framework-owned typed handlers.
Claim C018 records that the frame driver now scopes an override at the measured projection producer
`0x80042910`, preserves
its raw generated super, and delegates its view publication to `ProjectionOwner`. That owner records
previous/current native width, height, centre, and H from the pre-GTE view and refuses if the retail
libgte state disagrees. Issue 0016 records the capability audit and owner boundary.

Gap: static evidence does not establish execution order, the active camera, a first visible frame,
or which indirect primitive callbacks execute. The 16/16/15 CR24/CR25/CR26 writes outside the two
typed leaves remain dynamically unattributed. Issue 0013 tracks the live attribution boundary.

### S005 — Game-state native renderer

Missing capability: CTR has a PC-owned finite frame-transition driver and has submitted its first
real compatibility-path frame through the framework GTE presenter, but the actual frame work is still
performed by generated guest behavior. It has no native primitive producer, native render queue, or
native depth/order owner. `PresentationOwner` rotates one fence per field, committing only captured
retail work and otherwise recording an unpresented field. The runtime declares GTE as its sole player
path and refuses Native/FPS60 rather than treating the target scope as implemented. The statically
identified `lensflare` callback is a candidate source boundary only; no guest OT/GP0/framebuffer
fallback exists, and the first submitted image has not yet been captured for pixel inspection.

### S006 — True widescreen

Missing capability: CTR has no native camera/projection owner that can widen the horizontal view.
The A/B-checked projection publication records guest view width/height/OFX/OFY/H, but those values
are evidence plumbing, not a widescreen implementation, and
post-projection guest output cannot recover the state required for a wide frame.

### S007 — Interpolated presentation

Missing capability: CTR has no authoritative native simulation tick owner or consecutive native
camera and object-transform snapshots. There is therefore no grounded state pair a temporal
presentation decorator can interpolate without re-running guest code or consuming quantized GTE output.

### S008 — Playable default product

Missing capability: the identity-verified product executes the host-owned frame loop and has submitted
one real 960x720 headless image through its truthful GTE player path. That run stopped immediately at
the first presentation boundary and did not capture pixels, establish sustained cadence, or exercise
playable input and gameplay audio. Native/FPS60 remain refused.
