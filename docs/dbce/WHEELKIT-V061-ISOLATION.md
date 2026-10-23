# Wheelkit isolation reconciliation on FH1 0.6.1

Private source correction, October 23. App handoff starts from `b262ca5e`;
its shipping SDK gitlink remains `59bc11f546a6ea67eefcad974d0c14041b2ab20d`.
No owner profile, installed game, upstream SDK remote or shipping pin changed.

## Concrete correction

The earlier app candidate `46a129ecc78444bf8b432ce039277cb47ea3cf0d` expects
SDK device-path admission `2c427444238c3b4be10f55284b736d0ae1506fcb`. That
policy guarded `SDL_OpenGamepad` only. SDK `59bc11f5` adds a separate raw wheel,
pedals and shifter path through `OnJoystickAddedLocked` and `SDL_OpenJoystick`.
Porting the old filter unchanged would let a shim-bound device open on this
second native path, including its later wheel/force setup.

Local SDK commit **`0301093df68b13a4b79474d45e2c0241ecc49ac2`**, branch and
worktree `nepo/fh1-device-isolation-v061-20261023`, is based on the already
checked remap seam `93a7e432`. It reuses the old optional pre-attach policy
and also checks the raw joystick path before classification or opening.
The port retains current raw-wheel and DualSense code at the merge conflicts.

- Unconfigured drivers keep native behavior without querying admission paths.
- With exclusions, exact ASCII case-insensitive OS interface paths are hidden
  from both SDL admission routes. Missing/empty candidate paths are withheld.
- Unrelated paths remain available; existing raw-wheel classification and
  duplicate-instance checks remain in place.
- Configuration is refused after a window attaches or SDL events initialize.
  A refused update retains the previous policy. An empty policy rolls back
  exclusions before attachment.
- The API is opt-in and unsupported drivers return false. The existing app's
  exactly-one-accepting-driver check remains necessary; no app was enabled here.

This adds a virtual method and SDL fields. A coherent SDK/consumer rebuild is
required before using this candidate; do not mix the retained ancillary DLL
with production code from this branch.

## Priority, snapshots and profiles

The earlier adapter's snapshot ownership and schema are already intentional:
immutable `shared_ptr<const WheelkitSnapshot>` publication, 100 ms freshness,
one frame-driven owner thread, and `read_profile(..., 2)` both at preflight
and in ProviderHost. Profile bytes are read-only, bounded input; this work
adds no writer, renamed key, migration or alternate parser.

The earlier priority successor `e29cfb4a2d104961077c9bec4f5a615d5b1263ab`
adds `PreferredDeviceForUser` and `GetDeviceStateWithPolicy`. The latter
returns validity and bound axes from the same snapshot as state, so a bound
centered zero cannot be confused with an unbound axis. The corrected merge
preserves keyboard contributions and host-pad reads. Those APIs are still
absent from this new SDK branch: the old adapter cannot yet compile against
it. Its historical 64 priority/keyboard assertions and 928 app integration
assertions remain earlier evidence, not v0.6.1 results.

When porting that reviewed priority delta, retain SDK `59bc11f5`'s new
`UpdateFeedbackLocked(user_index, ids, merged)` call after the final merge.
It feeds native DualSense feedback and must see the final guest axes. Do not
replace current `GetMergedState` wholesale with the earlier SDK function.

## Shared-shim distribution blocker

`65331dc1559d93e2f6aec523b023bd3a03660cca` in the old fixture notes was
explicitly test provenance, not a distribution pin. Private CMake accepts an
explicit mutable `PINYON_SHIFT_WHEELKIT_SOURCE`; it validates file presence,
not an exact source revision or closure. No tracked `thirdparty/dbce-shim`
distribution or version manifest exists in this 0.6.1 app branch.

The inspected toolkit is `ee8014d4ca7f3b38604b0fcc2baef460aabf7021`. Compared
with `65331dc`, `native/shim/host/provider_host.hpp` now includes the new
`shim/candidate.hpp`, constructs/services a Candidate and accepts optional
declaration extras. Controls also gained the shared binding editor. The
existing Core, parse, DirectInput provider and controls reader source did not
change in that comparison. This supports reusing the reader; it does not
qualify the changed host or establish a distribution pin.

Next delivery needs an exact toolkit revision and complete licensed source
closure recorded in the app's existing build/package metadata, then the
existing private fixtures against that closure. Do not treat a successful
build against the moving toolkit checkout as a shipped pin. Toolkit owner
changes and its existing provenance records were preserved.

## Focused evidence and limits

Clang 19.1.5, Windows x64:

- `tests/unit/input/device_path_seam.py`: **87 checks**, compiling actual
  production configuration and pre-open handler prefixes with fake SDL opens;
  `-std=c++17 -Wall -Wextra -Werror`. Both arrival routes, missing paths,
  case folding, unrelated/prefix-similar paths, duplicate instances, native
  defaults, pre-attach rollback and late refusal are covered. Removing only
  the new raw-path guard makes the fixture fail **raw joystick admission**.
- Existing focused `remap_seam` target: **171 assertions in two cases**,
  including the preserved 163 remap/chord assertions and 8 new actual
  InputSystem admission assertions. Release `/W4`, one build worker.
- Import inspection: 234 ancillary imports; no rebuilt InputSystem,
  remap/admission or DualSense methods imported from the old support DLL.
  The support runtime and fixture limitation are unchanged from the
  [prerequisite handoff](WHEELKIT-V061-PREREQUISITE.md).
- App documentation: 37-page site build and 4,086 local links/anchors pass;
  the private scope appears in the generated build page.

Reproduce the SDL fixture in the SDK worktree with a configured compiler:

```powershell
python tests/unit/input/device_path_seam.py --cxx <clang++.exe> --out out/admission
```

Configure/build/run the existing `tests/unit/input/remap_seam` fixture with
the same explicit dependency and runtime roots as `93a7e432`. Logs are retained
in `out/nepo-remap-v061/` and `out-device-path*.log` in the isolated SDK worktree.
The pinned LLVM 20 environment helper's capability probe stalled and was
stopped; checks completed with the already available VS Clang 19 toolchain.
Existing unused `host_page_number` and `native_input_clock` warnings remain.

The SDL check ends at fake opens; it does not compile the entire SDL backend
or exercise enumeration, hotplug scheduling, haptics or real device paths.
No full SDK/game build, install, screens, game, rig lease or physical output
was used. All owned check processes exited. SDK source is local only because
its sole remote is upstream; this pushed app handoff does not publish it.

The next source gate is the reviewed priority delta plus the resolved shim
distribution closure. Coherent linking, real-device arrival/isolation,
standalone shared-writer F6 UI and settings-command delivery remain separate
delivery gates. No runtime or physical acceptance is implied.
