---
id: 14
title: CTR device and zero-fill replay requires unlanded oracle device capture
status: wontfix
symptom: Exact landed framework commits through 99a42aa3 refuse the zero-fill chain at startup-init-device because oracle_trace does not accept --capture-devices.
state_items: S003
tags: ctr04,oracle,device-capture,zero-fill,framework
created: 2026-08-26
updated: 2026-09-04
---

## Root cause

The CPU/device comparator was developed against an isolated psxport worktree based on `9c2e3f1c`
which added `oracle_trace --capture-devices`, but that generic oracle CLI slice was never landed.
CTR's recorded framework and landed `99a42aa3` expose the underlying `oracle_capture_devices` shim
only to `oracle_spike`; `oracle_trace` refuses the option before any title comparison occurs. The
zero-fill window includes the earlier initializer-device evidence, so it cannot bypass this missing
dependency without weakening the chain.

## What was tried / dead ends

An isolated clean Clang build against `99a42aa3` passed CTR's format, structure, clang-tidy, runtime,
provisioning, comparator-selftest, and render-frontier gates. The chained real-executable target then
passed every reproducible boundary through `0x800772E0` and refused at
`ctr04_startup_init_device_next_call_check` with `oracle_trace: unknown option --capture-devices`.
The only retained binary containing that option is under the gitignored
`scratch/build-ctr-device2/` tree whose `psxport_resolved.txt` names the removed
`oracle-dma-resume` worktree.

Dropping device fields from the zero-fill comparison would sever its required upstream evidence and
is not an acceptable workaround.

The operator-run exact `99a42aa3` product exited zero after loading `SCUS_944.26`, entering
`0x8007793C`, servicing InitHeap, and reaching its supported frontier at `0x800772E0`. That confirms
the bounded product path recorded by C013, but it does not supply the missing oracle device state or
advance the independently compared frontier past the pre-instruction boundary at that address.

## Disposition

Do not land or rerun this generated-path extension. Claims C014/C015 remain falsified and the
independent frontier remains `0x800772E0`. New device and zero-fill evidence must compare the
native/Lightrec product against the separately built independent oracle at a reached boundary, with
the same positive and controlled-negative observability. This issue records why the old result is not
evidence; issue 0024 owns the replacement execution path.
