# Wheelkit input driver: source-only composition

October 16, 2026. Local `feat/wheelkit-snapshot` worktree:
`E:/Source/games/.worktrees/vida-fh1-wheelkit-snapshot-20261016`.
The concrete `WheelkitInputDriver` composes the existing
[snapshot](WHEELKIT-SNAPSHOT.md), [lifecycle](WHEELKIT-LIFECYCLE.md), and
reviewed SDK `e1a2531c` remap-only contract. No game registration is added.

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

## Focused result

The new fixture compiles the actual driver, lifecycle, snapshots, shared host/Core
and the reviewed SDK's actual InputSystem/remap/assignment sources. It reuses the
previous SDK fixture's private support imports and populated third-party headers;
the prior remap test target is neither built nor run. Existing runtime
`04B4117D` supplies unchanged ancillary services and active-device merge/tracking.
The shared scripted provider is wrapped only for owned lifetime, with no real
DirectInput or force backend.

```powershell
cmake -S tests/input/driver -B .local/wheelkit-driver-tests `
  -G "Visual Studio 17 2022" -A x64 -T ClangCL `
  -DWHEELKIT_SDK_SOURCE=E:/Source/games/.worktrees/vida-shiftglue-remap-20261016 `
  -DDBCE_TOOLKIT_SOURCE=E:/Source/toolkits/dbce-wheel-mod-toolkit `
  -DSDK_DEPENDENCY_ROOT=E:/Source/games/dbce-mods-forza-horizon/thirdparty/shiftglue-sdk `
  -DSDK_RUNTIME_ARTIFACTS=E:/Source/games/dbce-mods-forza-horizon/out/build/win-amd64-release/rexglue-artifacts
cmake --build .local/wheelkit-driver-tests --config Release `
  --target pinyon_wheelkit_driver_tests --parallel 1
ctest --test-dir .local/wheelkit-driver-tests -C Release `
  -R '^wheelkit_driver$' --output-on-failure
```

ClangCL 19.1.5 `/W4` Release build passed with the pre-existing SDK
`xmemory.h` unused-parameter warning. Two focused cases passed in about 0.05 s,
169 assertions in the recorded run (the count includes explicit ready frames).
Checks include every mapped button, held shifts/repeated reads without new polls,
axis endpoints/midrange, unsupported-action silence, stale/active/focus/device-loss
and stopped neutralization, packet changes, standard no-force capabilities,
synthetic rumble acceptance and ordinary-pad error/scaling, keyboard/pad remap
preservation and native controls continuing after the adapter stops.

Log: `.local/wheelkit-driver-tests/Testing/Temporary/LastTest.log`.
The original snapshot, lifecycle and SDK-remap logs and passes remain unchanged.

## Remaining integration boundaries

The game target, owner SDK checkout/gitlink (`1e8968f7`), installed files and
public refs remain unchanged. The SDK is composed from its reviewed isolated
worktree only. The toolkit reference remains clean local `65331dc1`, not a
shipping pin. No game, physical input/force or rendered UI is qualified.

Current InputSystem policy still merges axes by larger magnitude and selects
capabilities from the active/first device. The focused fixture explicitly checks
that an active ordinary pad can answer capabilities. This slice does not provide
the design's bound-wheel-axis priority or title-scoped wheel-capability preference.
Any SDK production change needed for those policies or host callbacks requires
substantive source review. Admission, real-provider wiring/suppression, frame/focus
callbacks, a verified provider/driver monotonic clock domain, the settings-command
queue, distribution pinning and standalone F6
writer remain future work. Gore retains the runtime queue.
