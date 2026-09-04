---
id: 15
title: CTR native frame loop leaves residual guest VSync routes
status: investigating
symptom: Product passed native stock CdRead/CdReadSync, then reached libgpu timeout-arm VSync(-1) from 0x800750B8
state_items: S003,S005,S009,S010
tags: frame-loop,vsync,platform-hle,ctr-runtime
created: 2026-08-27
updated: 2026-08-28
---

## Root cause

CTR originally had no title `FrameDriver`. The current driver corrected that structural defect, but
its extraction boundary was inferred from four obvious VSync sites instead of auditing the complete
retail callsite denominator. The identity-verified executable contains 33 direct VSync callsites
across 20 owner functions; the driver first extracted four and now has a fifth source owner pending
live proof. Its first identity-verified live run instead reached an earlier stock libcd route:
`CdInit 0x8007C208` called `CdControl 0x8007BC38`, which entered `CdSync 0x8007B6F0` and called
VSync(-1) from return `0x8007B72C`.

One residual route is statically proven on the state-0 startup path. Continuation `0x8003C7F4`
calls `0x80031FDC` from `0x8003C8D4` and `0x8003C984` with fifth argument `-1`; that exact branch
unconditionally calls VSync(2), returning at `0x80032074`. Because the framework's mandatory trap
correctly aborts every guest VSync mode, CTR must own a finite two-field wait and resume the preserved
guest continuation without invoking VSync.

## What was tried / dead ends

Treating the existing `0x800772E0` stop as a completed native frame would invent the unmeasured
input, audio, render, and presentation phases. Running guest VSync as a timing source is also invalid:
the host shell owns iteration and the framework contract deliberately terminates every VSync mode.

## Current implementation

Production and the runtime test now use `ctr::installRuntimeOwners`, which initializes PlatformHLE
and requires the native-loop contract. CTR declares retail VSync `0x80075350`; the framework binds it
to its non-replaceable fatal trap. `CtrRuntime::createFrameDriver` supplies `CtrFrameDriver`, and the
framework shell delegates exactly one finite title step per host iteration.

Static RE grounds resident main `0x8003C58C`, loop top `0x8003C5D0`, state-3 frame owner
`0x80035E70`, its timing return `0x8003785C`, post-VSync suffix `0x80037880`, and main resume
`0x8003CEB4`. The driver preserves raw generated supers for the three startup/teardown predecessors
and frame timing function, reproduces their exact jal/delay-slot register and tick effects, and omits
only VSync(0), VSync(0), VSync(30), and the frame owner's conditional debug VSync(0). The first step
crosses startup; later host steps resume at `0x8003CEB4`. Each finite step ends when the generated
frame owner restores that return, which is an unpresented repeating fence rather than a native frame.
`PresentationOwner` now rotates the framework's explicit `commitUnpresented` fence exactly once at
that return; it emits, presents, and paces nothing.

The asset-free Clang runtime test exercises the production composition path, proves that representative
VSync modes `-1`, `0`, `1`, `4`, and `30` terminate in child processes, requires the title driver,
and covers first-startup, repeated, and teardown transitions while checking scoped overrides and
bridge register effects. The generated shipping product links with those preserved supers and
re-entry seeds. The driver now advances the title's compatibility field counter and samples host
input at each finite step. Fresh Clang product PID 2512718 loaded the verified executable and entered
resident main, then the mandatory trap caught VSync(-1) at `0x80075350` with return `0x8007B72C`.
The trap diagnostic subsequently faulted while walking an out-of-range guest stack, so the observed
process status was 139; the first causal failure remains the logged guest-VSync violation.

Static Ghidra decompilation identifies `0x8007B6F0` as CTR's stock libcd CdSync and `0x8007BC38` as
CdControl. Both contain two VSync(-1) callsites while waiting for controller IRQ state which the PC
CD model deliberately does not emulate. CTR's PlatformHLE plan now replaces those measured library
leaves with synchronous native completion: CdControl keeps shared command effects and CTR's zero-on-
success contract, while CdSync returns ready status 2 and initializes its eight result bytes. The
fatal VSync leaf itself is unchanged. Fresh-Clang PID 2554905 installed all five declared hardware
leaves and advanced past the earlier `0x8007B72C` violation. It then refused before title execution
because `main` called `FrameLoopShell::step` without the new mandatory `prepareProduct` preflight.
Production and the runtime test now call that framework owner after all title hooks are installed;
their Clang build and focused composition test pass. Fresh-Clang PID 2571678 then passed both the CD
leaves and product-loop preflight, serviced InitHeap, and stopped at the next fail-fast dependency:
shared BIOS A0:1A memcmp, caller `0x8001C5D4`, arguments `(0x80081000, 0x8008CFB8, 3)`. That generic
libc leaf belongs in psxport. Its shared owner implemented bytewise guest-memory comparison and
passed the focused Clang test; CTR has rebuilt against that tree, with the next serialized product
run proving it advances. PID 2590436 then stopped at VSync(-1), return `0x8007705C`, inside stock
libcd CdRead `0x80076F10`. Ghidra proves that function owns three VSync(-1) queries around its
asynchronous controller/IRQ read state. Its direct callers immediately follow with CdReadSync
`0x800770AC`, which owns two more VSync(-1) polls and returns sectors remaining. The framework
already has authoritative synchronous stock CdRead and CdReadSync implementations, but they are
now exposed as narrow typed entries rather than copied into CTR. CTR binds the measured leaves
directly to those shared owners and assigns
`PSXPORT_CTR_DISC` directly to its native DiscState so the shared owner can resolve the real CHD.
Their shared and CTR-focused Clang tests pass. Fresh-Clang real-disc PID 2665477 installed seven
hardware-sync owners, opened the CTR CHD, and advanced through both former stock-read violations.
It stopped at the next distinct VSync(-1): return `0x800750B8` inside PsyQ libgpu timeout arm
`0x800750A8`. Ghidra proves that leaf only reads the field counter, stores counter+240 at
`0x8008AEBC`, and clears poll count `0x8008AEC0`; its companion `0x800750DC` reads the same clock
before enforcing the deadline and `0xF0000` poll limit. CTR now binds both leaves to exact title
owners which consume the host-owned `Timing::vblank` counter. An unexpired check preserves the
guest globals and returns zero; an expired queue aborts because it contradicts the synchronous
native-GPU contract instead of concealing the producer failure or entering retail hardware reset.
The focused Clang composition test covers arm, check, and the fatal negative case. This source owns
two more retail routes. PID 2714292 then passed them and remained live for the full 20-second bound,
but completed no presentation fence and produced no capture before its exact PID was terminated.
A three-second diagnostic rerun, PID 2718506, exited through the boot watchdog with the exact chain
`0x800293B8 <- 0x8002DD24 <- 0x8003C8D4 <- CtrFrameDriver::resumeBootResourceWait`.
Ghidra proves `0x8002DD24` owns two do/while polls around asynchronous resource stages
`0x800293B8` and `0x80029CA4`; keeping either loop inside one guest dispatch starves the host field
and CD service which can make the poll advance. The title driver now preserves the generated setup,
stage functions, guest stack, register effects, and tick counts, but yields one host step after each
false poll and resumes the caller at freshly emitted continuation `0x8003C8FC`. The focused Clang
test exercises both yields and exact continuation. Its live run is pending, and 17 other direct
VSync callsites remain fatal.

Subsequent identity-verified real-disc runs crossed the resource pump, both synchronous disc stages,
and the original fatal VSync caller. State zero then waited at `0x8003C94C` while XA progressed but
its task word `0x8008D708` remained 3. GDB measured channel 4 owed inside CTR's custom exception
exit at PC `0x8007B104`, return `0x80077494`, with DICR `0x90940000`. Static and live RAM evidence
agree that libapi's DMA table starts at `0x8008CB08`, channel 4's slot is `0x8008CB18`, its libspu
wrapper is `0x8007AB34`, and the application callback is `0x8001C984`.

The direct runtime intentionally has no legacy `GameConfig`, so generic `Hle::irqPoll` has no DMA
callback-table address. `DmaCallbackOwner` now owns CTR's measured channel-4 slot, acknowledges only
an owed completion, dispatches under the same `in_irq` guard as HLE, restores R3000, and leaves a
synchronously chained transfer owed for a later host field. A post-super hook at saved continuation
`0x80077348` was a dead end because B0:17 performs a non-local ReturnFromException unwind through
that generated super. The grounded safe seam is startup loop `0x8003C94C`: its wrapper first lets
the custom-exit poll finish and clear `in_irq`, then CTR delivers DMA4 before the generated body's
second generic poll can consume it without a callback table. Focused Clang tests cover nested
refusal, take/ack, context restoration, chained deferral, and this finite loop override.

The next real-disc diagnostic passed that audio boundary, later CD IRQ service, and multiple DMA2
GPU completions before the caller guard found a third `0x80031FDC` return at `0x800336F8`. Another
game process was already active, so this advances diagnosis but is not serialized product proof.
Static retail code at `0x800336DC` explicitly stores fifth argument `-1`, so it is the same finite
VSync(2) branch, not a new policy. That measured continuation is now emitted and admitted; other
callers remain fatal. Serialized PID 3197275 then crossed `0x800336F8` after 15,086 host-owned fields
without a guest-VSync violation. The continuation legitimately returned, and the existing guard
aborted because it treated all three admitted suffixes as non-returning. Generated retail code
grounds the distinction: `0x800336F8` is inside `0x80033610`, and that function's sole direct caller
resumes at `0x8003CC98`. The verified-executable emitter now admits that exact reentry, and the title
owner requires it before dispatching the caller continuation. A production transition contract
proves the two native wait fields, saved-frame restoration, suffix return, and next frame fence. No
PRESENT or guest-VRAM capture was produced before this boundary; all 15,086 reconciled ledger frames
contained zero captured and zero presented primitives. A serialized rerun remains required.

Serialized PID 3229162 crossed the newly emitted `0x8003CC98` path and next reached timing leaf
`0x8004B3A4` from return `0x8004B438`; the exact-caller guard aborted because it had admitted only
the frame-owner return `0x8003785C`. This is the same retail leaf but not another VSync route:
generated `0x8004B41C` is its sole other direct caller, uses the result as an elapsed-time query, and
returns without any adjacent VSync. The title override now runs the preserved super and returns
normally for `0x8004B438`; only `0x8003785C` omits the measured conditional debug VSync and completes
the frame. All other returns remain fatal. The focused composition contract exercises both paths and
the full Clang gate passes. PID 3229162 had no guest-VSync violation and reconciled 10,136 empty
ledger frames with zero dropped layers, but produced no capture or visible presentation.

The combined title batch is integrated locally against clean psxport
`3c342ec3c738d6af7a626a35dce07b2c35a9f840`: a new Clang 22.1.8 tree builds every target, its full
asset-free CTest passes 7/7, and the normal `verify` target passes the boot/render negative-control
fixtures, provisioning contracts, 8/8 framework smoke checks, runtime contract, formatting and size
policy, and clang-tidy on 14/14 compile-backed first-party translation units. Both executable help
spellings exit zero before assets, and exact pin/check provenance passes. No game was launched for
this integration gate. The later serialized run above advances the resolution boundary to
`0x8003CC98`.

Independent differential execution evidence still stops at `0x800772E0`. The source driver transcribes the
exact fifth-argument-`-1` prefix, yields two native fields, and resumes through freshly emitted
entries `0x80032074`, `0x8003C8D4`, and `0x8003C984`; that recorded route remains unverified. The
native/Lightrec product must reach or falsify it without another static-product run.

## Resolution boundary

Rerun the finite resource wait through newly grounded timing return `0x8004B438` with the fatal VSync
trap intact. Record the first subsequent dispatch boundary and whether a real presentation or
capture appears. Earlier capture-armed runs produced zero files, correctly proving that no picture
or presentation fence was reached before this boundary.
For each reachable caller, extract the smallest owner which preserves the generated prefix/suffix,
models its finite native wait, and resumes through an emitted continuation. Do not replace VSync
itself with a successful handler. Resolve only after a bounded product run advances sustained
native-owned fields without reaching any residual guest VSync.

### Note (2026-08-28)
Serialized real-product evidence on clean psxport fb08d30f advanced the frame owner twice. PID 3505789 first exposed a title adapter defect: the scoped 0x80077254 VSyncCallback registration owner requested its generated super, but `recomp_register` did not admit that measured address. The adapter now dispatches `gen_func_80077254`, and the focused runtime contract requires `FrameCallbackOwner` to request exactly that super. A fresh Clang product build against clean fb08d30f passed the focused contract.

Isolated PID 3531982 then crossed 0x80077254, registered retail VSyncCallback 0x80034AA4, initialized the actual Vulkan GTE presenter, and submitted the first 960x720 headless product image. No fatal guest VSync occurred before that presentation. The exact PID was terminated immediately after the answer. This proves a real product presentation boundary, but no screenshot/pixel inspection was captured, so visible CTR content is not yet claimed.

The direct diagnostic also reported all four RmlUI assets absent because the executable was intentionally invoked without the shipping launcher. The launcher already selected the framework root only when ambient `PSXPORT_ASSET_DIR` was absent; that allowed a poisoned inherited path to pair the product with the wrong checkout. `tools/run.py` now assigns the resolved framework checkout unconditionally at final exec, with missing and poisoned-environment regressions.

Resolution boundary: preserve the measured native owners and fatal VSync trap while issue 0024 moves
the product to Lightrec and replaces `FrameCompleted` unwinding with typed executor exits. Do not run
the static product again. Resolve the remaining reachable VSync set from the dynamic product's first
representative gameplay path; native, widescreen, and interpolation remain unavailable until their
real producers exist.
