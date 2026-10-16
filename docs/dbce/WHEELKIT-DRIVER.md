# Wheelkit input driver: source-only composition

October 16, 2026. Local `feat/wheelkit-snapshot` worktree:
`E:/Source/games/.worktrees/vida-fh1-wheelkit-snapshot-20261016`.
The concrete `WheelkitInputDriver` composes the existing
[snapshot](WHEELKIT-SNAPSHOT.md), [lifecycle](WHEELKIT-LIFECYCLE.md), and
reviewed SDK `e1a2531c` remap-only contract, followed by the isolated
`feat/title-input-priority` SDK successor `cb3b198b51c8bc01d5d06796c20de343d6b52f1d`.
Source review requires a keyboard pass-through correction before integration;
the 120-assertion result below is pre-correction evidence. No game registration is added.

## Delivery and source basis

The driver reads the latest lifecycle snapshot and converts only source-backed
default FH1 controls. `src/config/default_keys.h:14-16` documents the pad roles;
`src/ui/settings_menu.cpp`'s existing keyboard-control labels agree. The SDK
`input.h` supplies the matching guest button fields, and the game's existing
default-key table includes the four D-pad navigation directions.

| Shared action | Guest output |
| --- | --- |
| `steer` | left-stick X, -32768 through 32767 |
| `throttle`, `brake` | right/left trigger, 0 through 255 |
| `clutch` | LB |
| `handbrake`, `confirm` | A |
| `shiftUp`, `back` | B |
| `shiftDown` | X |
| `start` | Start |
| `camera` | RB |
| `navUp`, `navDown`, `navLeft`, `navRight` | corresponding D-pad bits |

Axes use clamping and nearest-integer quantization only; Core owns calibration
and resolution. Digital outputs, including pedal-shaped clutch/handbrake mapped
to binary LB/A, use a 0.5 threshold. Aliases sharing a guest button combine as
held levels. There are no generated shift pulses. These are the source's default
FH1 gamepad roles; alternate in-game controller layouts remain unqualified.

`lookBack`, `select`, direct gears/neutral/reverse, reset and horn have no
established delivery mapping here and emit nothing. Core's `modMenu` (the design's
host `settings` action) and `muteFfb` also emit nothing: the host-command queue
and force path do not exist yet. No Back+Start chord is synthesized. Metadata's
published action list remains empty pending runtime qualification.

Only live finite action values are projected. Unavailable actions, stale frames,
focus-invalid/stopped/not-started snapshots and a blocked guest active callback
produce neutral input. The state packet changes when the projected state changes,
including a live-to-stale transition, so a guest tracking packets can observe the
neutral state. Repeated reads preserve held levels and do not poll the provider.

The driver opts out of the SDK's second remap and remains a non-keyboard driver.
Its synthetic device ID is never reused by another driver instance. Synthetic
vibration accepts and ignores the guest request, including null requests, without
calling lifecycle/provider code. Capabilities report a standard pad (subtype 1),
only implemented controls and no force/motor capability. Subtype 2 is not enabled.

## Optional title policy

`WheelkitDeviceAssignment` wraps the caller's existing assignment and delegates
all routing unchanged. It defaults off. An explicitly enabled instance requests
wheel preference for user 0 only; the SDK also verifies that the selected device
actually belongs to that user. Other users and host-pad reads keep their original
selection/merge path.

The narrow shared SDK API is `PreferredDeviceForUser` (default invalid) and
`GetDeviceStateWithPolicy` (default existing state read with no priority axes).
The Wheelkit driver returns state, validity and a guest-axis mask from **one
snapshot**. The mask comes from Core's applied action statuses, independently of
value: `Live`, `Rearming` and `TestNeutral` identify resolved available bindings.
Thus centered steering/released pedals and a rearming zero remain bound, while
`Unbound`, invalid/unsupported, opening/missing and other unavailable channels
claim no priority. Non-live snapshots and a blocked guest callback are invalid.
Unknown/unmapped actions alone cannot make the preference valid.

This is sufficient for current usable binding presence; no extra parser or Core
interface is needed. It deliberately does not claim that a configured but missing
device is usable. The fixture observes actual `Unbound`, `DeviceMissing` and
`Rearming` statuses separately, rather than inferring binding presence from zero.

For valid user-0 preference, guest capability queries select the wheel even if
another pad/keyboard was most active. An invalid/stale preferred device is removed
from that query's fallback candidates, so historical activity cannot select it
again. Ordinary active/first-device selection then applies to remaining devices.
The driver still reports subtype 1 with no force capability.

Guest state first follows the ordinary merge, chord and remap rules. Only fields
in the preferred contribution's explicit mask replace that merged result, after
all devices, so a later keyboard or a larger pad value cannot overwrite bound
wheel steering/pedals. Unclaimed fields and buttons retain existing merge rules.
An unavailable channel falls back independently. Host reads, default assignments,
ordinary packet-number merging and rumble routing are unchanged. No global cvar,
default policy activation or second input layer is added.

## Focused result

The new fixture compiles the actual driver, lifecycle, snapshots, shared host/Core
and the reviewed SDK's actual InputSystem/remap/assignment sources. It reuses the
previous SDK fixture's private support imports and populated third-party headers;
the prior remap test target is neither built nor run. Existing runtime
`04B4117D` supplies unchanged ancillary services and active-device merge/tracking.
The shared scripted provider is wrapped only for owned lifetime, with no real
DirectInput or force backend.

The original adapter `af16f40f` result remains 169 assertions in two cases, with
its original log at `.local/wheelkit-driver-tests/Testing/Temporary/LastTest.log`.
It and the earlier snapshot/lifecycle/remap suites were not rerun or overwritten.
The current policy-only candidate uses a separate output and tag:

```powershell
cmake -S tests/input/driver -B .local/wheelkit-priority-tests `
  -G "Visual Studio 17 2022" -A x64 -T ClangCL `
  -DWHEELKIT_SDK_SOURCE=E:/Source/games/.worktrees/vida-shiftglue-priority-20261016 `
  -DDBCE_TOOLKIT_SOURCE=E:/Source/toolkits/dbce-wheel-mod-toolkit `
  -DSDK_DEPENDENCY_ROOT=E:/Source/games/dbce-mods-forza-horizon/thirdparty/shiftglue-sdk `
  -DSDK_RUNTIME_ARTIFACTS=E:/Source/games/dbce-mods-forza-horizon/out/build/win-amd64-release/rexglue-artifacts
cmake --build .local/wheelkit-priority-tests --config Release `
  --target pinyon_wheelkit_driver_tests --parallel 1
ctest --test-dir .local/wheelkit-priority-tests -C Release `
  -R '^wheelkit_priority$' --output-on-failure
```

ClangCL 19.1.5 `/W4` Release build passed with the pre-existing SDK
`xmemory.h` unused-parameter warning. Three new policy cases passed in about
0.06 s, 120 assertions in the recorded run (including explicit ready frames).
They cover disabled/missing preferences, bound zero against stronger pad/keyboard
input, unclaimed axes/buttons and host behavior, user-1 SlotAssignment routing,
capability preference/fallback on not-started/stale/unfocused/stopped sources,
actual unbound/missing-channel fallback, smaller wheel steering priority, and
Core-rearming zero as a bound axis.

Log: `.local/wheelkit-priority-tests/Testing/Temporary/LastTest.log`.
The original snapshot, lifecycle and SDK-remap logs and passes remain unchanged.

## Remaining integration boundaries

The game target, owner SDK checkout/gitlink (`1e8968f7`), installed files and
public refs remain unchanged. The SDK is composed from its reviewed isolated
worktree only. The toolkit reference remains clean local `65331dc1`, not a
shipping pin. No game, physical input/force or rendered UI is qualified.

The default InputSystem behavior still merges axes by larger magnitude and selects
capabilities from the active/first device. The optional source policy above is
isolated and must be integrated with a rebuilt SDK/consumers before any game claim.
Further shared SDK changes, including any needed for host callbacks, require
substantive source review. Admission, real-provider wiring/suppression, frame/focus
callbacks, a verified provider/driver monotonic clock domain, the settings-command
queue, distribution pinning and standalone F6
writer remain future work. Gore retains the runtime queue.
