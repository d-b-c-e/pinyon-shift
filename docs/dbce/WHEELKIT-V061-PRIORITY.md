# FH1 0.6.1 priority API and shared-shim source pin

October 23 source candidate. This branch preserves private adapter `46a129ec`
and checks its actual input classes against the current SDK; it does not merge
the adapter's app/window hooks into the 0.6.1 game candidate. Owner main, SDK
checkout/gitlinks, installed game, settings and profiles are unchanged.

## Exact sources

| Component | Source |
|---|---|
| SDK priority port | `4329f0e12d8c75012b9fb38322d72369618da581`, local branch/worktree `nepo/fh1-priority-v061-20261023` |
| SDK parent | `0301093df68b13a4b79474d45e2c0241ecc49ac2`, includes the remap seam and both SDL admission guards on SDK59bc11f5 |
| Private app/fixtures | branch/worktree `nepo/fh1-wheelkit-priority-fixture-20261023`, based on `46a129ecc78444bf8b432ce039277cb47ea3cf0d` |
| Shared shim | `ee8014d4ca7f3b38604b0fcc2baef460aabf7021`, tracked `thirdparty/dbce-shim/` |
| Closure manifest SHA-256 | `E65907B8C07C9818868A7E9CBC116A46550B2C5748EC5F6192EA2824AF728027` |

The SDK production diff reuses `cb3b198b` plus keyboard correction `e29cfb4a`:
three files, 79 insertions/one deletion. Preference defaults to invalid; existing
drivers retain state reads and claim no priority axes. The adapter returns state,
validity and bound-axis metadata from the same immutable snapshot. Its frame
ownership, 100 ms stale policy and schema 2 reader are unchanged and read-only.

The current SDK conflict was at the final merge. `UpdateFeedbackLocked` remains
after preferred-axis and keyboard contributions are applied and before state is
returned. Current session-probe wrappers and physical-before-synthetic capability
fallback are preserved. The old fixture's six fallback expectations changed from
keyboard subtype 9 to physical pad subtype 7 to match that upstream behavior;
the initial failure log is retained. No default behavior was restored to the
older SDK's behavior.

## Distribution closure

`VERSION` and `PROVENANCE.json` record the exact toolkit commit, MIT license,
roots/include directories, Git blob IDs and SHA-256 hashes. The copy contains
16 exact Git blobs (212,813 bytes), including the updated ProviderHost and new
Candidate, the existing controls reader, Core/parse/DirectInput sources and
headers, original provenance/license and the existing fake-provider fixture.
All quoted includes resolve inside this closure. Unused toolkit subsystems,
the shared binding editor and managed shim DLL are not copied.

Private product CMake now uses the tracked source directory instead of the
mutable external toolkit path. Ordinary builds keep the adapter off. Existing
fixture CMake defaults to that same copy and compiles current `dualsense.cpp`
with current InputSystem. Source packaging includes the closure/license and
records the shim commit and closure-manifest hash in `source-provenance.json`.
This records available source, not whether a game binary enables the adapter.
No launcher package or game binary was built or distributed here.

## Checks

- ClangCL 19.1.5 Release `/W4`, one worker: **1,270 assertions in nine cases**
  across existing driver, priority, keyboard/order, integration and the new
  feedback tag. Readiness loops can vary assertion counts; this is the retained
  passing run. Existing Core/device/schema2/rearm/admission behavior is exercised
  through the pinned host with the existing scripted provider, not real devices.
- New feedback case: ordinary pad brake 255 produces ABS effect family 6;
  preferred resting wheel brake 0 produces family 1; keyboard brake 81 produces
  family 6. Host reads retain physical-pad state without emitting feedback.
  Moving the actual feedback call before priority fails `6 == 1`; restored
  source passes **20 assertions** in the focused case.
- 271 imports inspected: current InputSystem, assignment, remap and DualSense
  methods are compiled here rather than imported from old runtime `04B4117D`.
  Ancillary tracking/logging/cvars/UI services still use that support runtime.
- Two exact-blob/include-closure and private-build/package checks pass. Two
  existing source-payload contract checks pass. The actual source-provenance
  construction emits the expected pin/hash without running package publication.
- The preserved app branch's 21-page site builds; 865 local links/anchors
  resolve and the source-pin paragraph is generated. This is static HTML
  validation, not a browser/UI check.

Logs: `.local/nepo-priority-v061/` in the private app worktree, including initial
fallback failure, corrected pass, deliberately wrong feedback order and restored
pass. Existing unused `host_page_number`, `native_input_clock` and missing
`admission` aggregate-initializer warnings remain. No original evidence replaced.

Reproduce via `tests/input/driver`, using the SDK worktree above as
`WHEELKIT_SDK_SOURCE` and the previously retained explicit
`SDK_DEPENDENCY_ROOT`/`SDK_RUNTIME_ARTIFACTS`. Omit `DBCE_TOOLKIT_SOURCE` to use
the tracked closure. Run the wheelkit-driver, wheelkit-priority,
wheelkit-keyboard-priority, wheelkit-priority-feedback and wheelkit-integration
tags. Run `python -m unittest discover -s tools/tests -p test_wheelkit_distribution.py -v`
for the pin check.

## Next delivery gate

The priority API and exact shim source closure now have a paired private source
candidate. Next merge the preserved adapter/app hooks into the 0.6.1 app candidate,
carry this pin through its packaging route, and coherently rebuild SDK/consumers.
The SDK commit is local only; its sole remote is upstream and was not changed.
Standalone F6 shared-profile writing, settings-command delivery, real device
arrival/hotplug/isolation and runtime/physical acceptance remain separate gates.
No full build, game, screens, device, force output, rig lease or install was used;
all owned check processes exited.
