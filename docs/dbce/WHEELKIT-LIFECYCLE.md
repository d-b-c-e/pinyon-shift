# Wheelkit input owner lifecycle: source-only slice

October 16, 2026. Local branch `feat/wheelkit-snapshot`, isolated worktree
`E:/Source/games/.worktrees/vida-fh1-wheelkit-snapshot-20261016`. Builds on
`4dbcbe61`, the [immutable snapshot boundary](WHEELKIT-SNAPSHOT.md), under
the accepted [r2.1 adapter design](WHEELKIT-ADAPTER.md). Gore retains runtime
and installed-state ownership.

## Behavior

`src/input/wheelkit_input_lifecycle.{h,cpp}` owns one adapter input thread.
It constructs the existing `hosting::ProviderHost<NoCapture>`, ticks it and
closes/stops/destroys it on that same thread. The provider factory executes
there too; it must create a fresh owned provider for each host epoch. The host
uses the shared schema-2 reader, Core resolver and existing device worker.
No provider is created until the first frame notification.

The adapter thread waits on a condition variable. Unique increasing host-frame
IDs request Core ticks; guest/UI `Read` calls only read snapshots. A single
pending slot retains the newest notification if a tick is still in progress.
Overwritten notifications are counted in `coalesced_frames`; they do not trigger
a catch-up burst. `WaitForFrame` reports processed frame progress, not device
readiness or successful live delivery. The shared host's existing device-service
worker/cadence remains unchanged; no second timed input loop is introduced.

Focus loss immediately stops the current snapshot store and clears pending
frames. A generation latch survives a rapid loss/refocus pair and prevents an
old in-flight tick from publishing after focus changes. The owner resolves the
old host's bounded stop before creating a new host on a subsequent notification.
The new Core applies its existing startup/epoch rearm rules: a held shift must
be released before another press is delivered. There is no adapter action
filter or second rearm implementation. A fresh snapshot store belongs to each
host epoch, so Core sequence numbers restart with that epoch.

An explicit `allow_background` policy supplied by the eventual integration
keeps the current host when the window loses focus. This slice does not add a
profile reader or decide that policy from an unverified profile field.

`Stop(timeout)` closes admission and neutralizes snapshots immediately, then
waits for the adapter thread against one deadline using the shared `join_by`
primitive. On the input thread, the existing host closes control admission,
gets its final tick, and runs its unchanged P21 per-phase stop budgets. A
blocked tick can outlast the caller's deadline: the caller receives
`owner_joined=false`; the thread retains the host, provider and all shared state.
Calling `Stop` again can collect its eventual result. Destruction requests stop
and detaches a still-running owner instead of performing an unbounded join.

An unresolved shared-host stop ends the adapter owner and forbids replacement
provider creation. Factory/tick exceptions are reported and leave input neutral.
The status preserves the host lifetime report and failure; the shared host's
full `host-shutdown.json` is written to the explicitly supplied scratch folder.
The future game integration must handle unresolved process policy; this module
does not terminate or park the game itself.

## Focused verification

Only the new lifecycle target was built and run. The existing snapshot fixture
and `.local/wheelkit-snapshot-tests` evidence were not rerun or overwritten.
Toolkit reference remains the clean local source checkout
`65331dc1559d93e2f6aec523b023bd3a03660cca`, not a distribution pin.

```powershell
cmake -S tests/input -B .local/wheelkit-lifecycle-tests `
  -G "Visual Studio 17 2022" -A x64 -T ClangCL `
  -DDBCE_TOOLKIT_SOURCE=E:/Source/toolkits/dbce-wheel-mod-toolkit
cmake --build .local/wheelkit-lifecycle-tests --config Release `
  --target pinyon_wheelkit_lifecycle_tests --parallel 1
ctest --test-dir .local/wheelkit-lifecycle-tests -C Release `
  -R '^wheelkit_lifecycle$' --output-on-failure --timeout 15
```

ClangCL 19.1.5 Release build passed with `/W4`; the final focused fixture passed
in about 0.5 seconds (46 checks in the recorded run; the count includes explicit
frames waiting for the existing asynchronous device worker). The fixture wraps
the existing scripted provider only to supply `ProviderOwner` lifetime and
thread/blocking instrumentation. It uses the real in-process host and Core.

New behavior checked: factory/read thread ownership, no host before a frame,
no UI/timer-triggered Core ticks, duplicate notifications and bounded coalescing,
focus loss during a blocked old-epoch read, rapid refocus and Core-held rearm,
explicit background policy, late-frame refusal after stop, a blocked input tick
with a 25-ms caller stop bound and later successful join, factory exceptions,
and an unresolved P21 provider release that prevents reopening another host.
The last case unblocks and observes the retained provider's eventual release.

Original lifecycle log:
`.local/wheelkit-lifecycle-tests/Testing/Temporary/LastTest.log`.
Scratch shutdown reports are under
`%TEMP%/fh1-lifecycle-<test PID>/{focus,blocked-tick,background,unresolved-host}`.

These results establish source-only lifecycle behavior. The game target,
SDK submodule/registration and installed state are unchanged. No game callbacks,
real DirectInput provider, physical input/force, F6 UI, toolkit vendoring or
public push is part of this slice. The public site describes runtime behavior;
its wheel/force claims do not change for this unregistered source-only module.

Next source task: the default-preserving SDK remap-only opt-out and fake-driver
test that retains chord consumption. Frame/focus callback wiring, guest action
mapping, device suppression/admission, capability policy, a resolved toolkit
distribution pin and the standalone shared-writer F6 page remain integration
and release work. Runtime qualification stays with Gore's queue.
