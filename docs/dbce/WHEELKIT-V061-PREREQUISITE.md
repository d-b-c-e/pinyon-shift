# Wheelkit resolved-action prerequisite on FH1 0.6.1

App handoff branch `nepo/fh1-wheelkit-prereqs-20261023` starts from
`f39a3d1502c9d2090cda0ee88895548c76258d08`; its source/SDK gitlink remains
unchanged. Owner main, installed game, settings and saves remain untouched.

## Reuse existing work

The earlier private app candidate `46a129ecc78444bf8b432ce039277cb47ea3cf0d`
in `vida-fh1-wheelkit-snapshot-20261016` already contains snapshot, frame-driven
owner-thread lifecycle, driver and default-off app wiring. Its original notes
retain23 snapshot checks, the lifecycle checks, the remap163-assertion case,
the corrected64-assertion keyboard/priority regression, and928 integration
assertions in two fake/object cases. Those are historical, bounded results;
they were not rerun or promoted to v0.6.1/runtime acceptance here.

That driver's `resolves_own_actions()` override relies on the earlier SDK seam
`e1a2531ca4e16debece245dbdf6f6ebd0e449042`. The 0.6.1 SDK59bc11f5 lacks it and
unconditionally remaps each non-keyboard driver's guest actions after host
chords. The smallest next prerequisite is therefore to port the existing seam,
not to create another shim/profile reader or reimplement the earlier driver.

## Source candidate and focused check

SDK commit `93a7e43237bcab07ebb49fcb3a0824feeee7ba0f`, local branch
`nepo/fh1-resolved-remap-v061-20261023`, isolated worktree
`E:/Source/games/.worktrees/nepo-fh1-resolved-remap-v061-20261023`, based exactly
on `59bc11f546a6ea67eefcad974d0c14041b2ab20d`.

Production changes are the existing default-false virtual method and the
`ApplyPadRemap` guard only. Host chords retain their positions and consume
their matched buttons; ordinary pads, keyboard/host reads, native wheel
mapping and FFB defaults remain unchanged. The new virtual method requires
a coherent SDK/consumer rebuild before use in a game.

The old focused fixture applies cleanly. Its first link failed because current
InputSystem needs newer DualSense helpers absent from the retained support DLL.
The corrected fixture compiles current `dualsense.cpp` and excludes its symbols
and flags from the support import library. No helpers or feedback were mocked.
ClangCL19.1.5 Release `/W4` build and the existing `[resolved-actions]` case
pass: **163 assertions, one case**, no skips. It covers each button, analog
mapping, chord consumption/rearm, keyboard/host behavior and mixed drivers.
Existing warnings are unused `host_page_number` and `native_input_clock`.

The fixture recompiles InputSystem, pad remap, assignment and DualSense helpers,
using only FakeDriver. Import inspection finds234 imported symbols and none
from those rebuilt entry points. Ancillary logging/cvars/bind/merge tracking
still use the older retained runtime; this is not a fully rebuilt SDK or ABI
qualification. The executable creates no SDL/native backend or physical device.

Reproduce with `tests/unit/input/remap_seam/CMakeLists.txt` in that SDK branch,
explicit `SDK_DEPENDENCY_ROOT` and `SDK_RUNTIME_ARTIFACTS` from the retained
canonical SDK/game build. Exact configure, original failed build, corrected
build, test and import logs are in its ignored `out/nepo-remap-v061/`.

## Delivery boundary

The SDK commit is **local only**: its sole remote is the upstream SDK, and no
owner SDK fork was found. No upstream publication or shipping-pin change was
made. This pushed app branch is the durable handoff to the exact retained SDK
branch; it does not claim the SDK source was pushed or integrated into f39a3d1.
The older app/SDK candidates and native 0.6.1 wheel path are preserved.

Next integration still needs a resolved distribution pin for the shared shim,
the reviewed priority/snapshot/isolation SDK successors reconciled with 0.6.1,
and a coherent SDK/game build. Standalone F6 profile writing, settings-command
delivery and real-device arrival/hotplug ordering remain separate open work.
No game/screens/rig, full build, install, force output or lease was used here;
all owned check processes exited. No runtime or physical acceptance is implied.

The subsequent [isolation reconciliation](WHEELKIT-V061-ISOLATION.md) ports the
old pre-attach admission seam and closes the new raw-wheel opening gap. It
records the concrete shim-pin blocker and the still-unported priority API;
it does not enable or integrate the earlier app adapter.
