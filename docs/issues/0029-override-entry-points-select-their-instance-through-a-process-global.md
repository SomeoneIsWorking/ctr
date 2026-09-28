# 0029 — the field driver's override entry points select their instance through a process global

Found while extracting the field owners out of `game/core/frame_driver.cpp`. **Not fixed here**: the
fix is a framework signature change, and `psxport/` is owned by another arm.

## What the code does

`psx::cpu::NativeFunction` is a plain `void (*)(Core *)` with no user data, so each of the field's
eleven overrides needs its own entry point. Those entry points cannot be lambdas that capture their
driver, so each reaches the driver it belongs to through a class static:

    CtrFrameDriver *CtrFrameDriver::active_ = nullptr;                    // frame_driver.cpp
    ...
    void CtrFrameDriver::onStartupGpuVSync(Core *core) {
      runVsyncBridge(*core, active_->runtime_, kStartupGpuVSyncBridge);   // active_, not *core
    }

The claim is taken and released by `ScopedActiveDriver` around one `stepFrame`, and a nested driver
is refused.

## Why that is the wrong shape

`psxport/AGENTS.md`, under **Lightrec dependency boundary**:

> One live `Core` owns one Lightrec state and its callback context. No process-global active core,
> register pointer, current image tag, override table, clock, or invalidation target may select a
> different instance implicitly.

`active_` is a process-global that selects a different instance implicitly. It is handed a `Core *`
and then ignores it in favour of the process's one driver. The entry point that fires is chosen by
the `Core` being executed, but the owner it dispatches to is chosen by a global — so with two `Core`s
live in one process, a call reached on Core A's override key can be serviced by Core B's driver, its
`CtrRuntime`, and its projection/widescreen/presentation owners.

## Why it is not reachable today, stated so this is not read as a live fault

Every test and every product path in this repository constructs exactly one `Game` and therefore one
`Core` per process, and the nested-driver refusal catches the one shape that would need two drivers
for the *same* type. The hazard needs two `Core`s of any kind in one process — a differential or
oracle harness co-resident with the product, or a future multi-instance test — which is a shape the
repository does not have yet. So the exposure is latent, and the cost of leaving it is that the next
multi-`Core` work inherits a global that silently picks the wrong instance.

## What a fix would be, and why it is not a title-local change

The seam is `psx::cpu::NativeFunction`. Options, in increasing cost:

1. Give the framework's override entry point the owning context alongside the `Core` (an
   `install` overload taking a `void *` context, or a typed callback). This is the correct shape and
   it is a `psxport` API change, so it needs the framework arm and a search of **every** PSX
   consumer, not just this repository.
2. Resolve the driver from the `Core` instead of from a global — e.g. a `Game`-owned downcast on the
   `FrameDriver` it already holds. Title-local, but it needs a framework-visible accessor anyway, so
   it is really (1) with a narrower surface.
3. Leave the global and make it per-`Core` state rather than class state. That keeps the framework
   signature and still violates the "no process-global selects the instance" rule in spirit, because
   the lookup would have to be keyed by something the callback does not receive.

Recommended: (1), owned by the framework arm, with this issue as the consumer that asked for it.
