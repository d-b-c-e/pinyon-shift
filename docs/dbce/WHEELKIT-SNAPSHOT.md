# Wheelkit snapshot boundary: source-only slice

October 16, 2026. Branch `feat/wheelkit-snapshot`, isolated worktree
`E:/Source/games/.worktrees/vida-fh1-wheelkit-snapshot-20261016`, based on canonical
fork `fdcb05b2d377b6f67c214e9502c6f0731b42cd21`. Accepted design:
[WHEELKIT-ADAPTER.md](WHEELKIT-ADAPTER.md), r2.1 (`155520d2`).

## Implemented boundary

`src/input/wheelkit_snapshot.{h,cpp}` copies one shared Core frame's sequence,
tick timestamp, resolved action values and statuses into an immutable snapshot.
Publication and pointer acquisition use the same mutex; a slow reader owns its
snapshot even after many later publications. No reusable two-buffer storage is
exposed. Reads copy neither a provider handle nor Core's mutable frame.

Digital actions are Core's held levels. `Frame::down` and `Frame::up` are not
consumed or turned into pulses. A second read sees the same level; a subsequent
Core tick with the same held button still publishes that level. Missing or
otherwise unavailable actions return zero independently, preserving their status
and other devices' usable actions.

The input owner supplies lifecycle/focus validity to `Publish`. Non-live validity
neutralizes all action values. `Read` applies the strict greater-than-100-ms
freshness limit and rejects a clock reversal. Neutral reads retain the original
sequence/time for diagnosis and never mutate a previously published snapshot.
`Stop` is terminal for the store: later publication cannot revive it.

## Integration contract and limits

- A single adapter input thread must call Core and publish once per notified
  input frame. This slice does not create that thread or register an SDK driver.
- Call `Read` on every guest/UI state request using the provider's same monotonic
  microsecond clock. Do not cache a prior live read across future state requests.
- The owner must publish non-live validity on focus/device/lifecycle loss and
  stop the store on shutdown. This class does not observe windows or devices.
- Core is the only resolver and epoch/rearm authority. The future host must
  arrange Core rearm on refocus before publishing live data again. Passing
  `Unfocused` here gates delivery; it does not itself reset a Core device epoch.
- No guest button mapping, settings-command queue, capability override, native
  device suppression, remap/chord change, F6 page or force output is included.
  All guest mappings and runtime delivery remain unqualified.
- The game target and default native controls are unchanged. These files are
  compiled only by the explicit standalone fixture. No shim DLL is built or
  published, and no game, save, installed config or physical device is touched.

## Focused verification

The fixture compiles the actual toolkit `Core`, profile reader and existing
scripted provider, with no parallel parser or provider. The explicitly inspected
local toolkit was clean at `65331dc1559d93e2f6aec523b023bd3a03660cca`.
This reference is test provenance, **not a resolved distribution/vendor pin**.
The existing SDK submodule remains at `1e8968f7` and is not built or changed.

From the isolated worktree:

```powershell
cmake -S tests/input -B .local/wheelkit-snapshot-tests `
  -G "Visual Studio 17 2022" -A x64 -T ClangCL `
  -DDBCE_TOOLKIT_SOURCE=E:/Source/toolkits/dbce-wheel-mod-toolkit
cmake --build .local/wheelkit-snapshot-tests --config Release `
  --target pinyon_wheelkit_snapshot_tests --parallel 1
ctest --test-dir .local/wheelkit-snapshot-tests -C Release --output-on-failure
```

ClangCL 19.1.5 build passed; 23 focused checks passed. They cover immutable
retained snapshots, complete concurrent publication, repeated held shifts,
per-action neutralization, exact freshness boundary/clock overflow cases,
caller validity and terminal stop. Actual schema-2 Core/provider checks cover
startup-held rearm, resolved steering/throttle, shift hold/release, device loss,
and reconnect release/repress. These are offline results, not game/physical input
or rendered UI evidence.

Original test log:
`.local/wheelkit-snapshot-tests/Testing/Temporary/LastTest.log`.

The following [owner-thread/frame-notification slice](WHEELKIT-LIFECYCLE.md) now
wraps the shared host and implements focus rearm through a resolved host restart.
The source and original 23-check snapshot evidence above remain unchanged.
Next add the SDK remap-only opt-out with retained chords and its fake-driver
test before registering the driver. Gore retains the runtime queue. Distribution
pinning, native suppression, assignment/capabilities and the standalone F6 writer
remain later release gates. No public push or install is part of this handoff.
