# FH1 0.6.1 shifting: private adapter and native wheel contracts

The trace below remains pinned to the built `abd3962d/e939542` candidate.
An unbuilt, default-off [native direct-gear successor](WHEELKIT-NATIVE-DIRECT-GEARS.md)
now implements the source protocol separately. Its FFB capability bit is an
explicit guest wheel-protocol requirement, not physical force support.

Source-only trace, October 23, on app
`abd3962d5c2ae101180fbb8d6124344c8da371ae` and SDK
`e939542634798fe620d254f56382f6b4e8cae9a3`. No code, bindings, save, build,
game, device, display or lease was changed or exercised.

**The current private adapter supplies incremental shifts only.** FH1 itself
has a direct H-shifter channel through its native wheel contract, but the
private adapter neither advertises that capability nor supplies its gear value.
Changing `Transmission = H-pattern` does not connect those contracts.

## Current private adapter

| Stage | Exact source anchor | Result |
| --- | --- | --- |
| Profile metadata | `thirdparty/dbce-shim/native/controls/dbce_controls.hpp:391` | Saves `Transmission` in the parsed section. It is not a guest transmission-setting write. |
| Binding admission | `thirdparty/dbce-shim/native/shim/shim/parse.cpp:152`, `plan_profile` | Plans all bound devices/actions without selecting an H-pattern/sequential gearbox. A missing separately bound shifter can still refuse the whole adapter at bootstrap. |
| Action resolution | `thirdparty/dbce-shim/native/shim/shim/core.cpp:504`, `Core::resolve`; `src/input/wheelkit_snapshot.cpp:13` | Resolves each binding independently and copies its action/status. `Transmission` has no mode-dependent shift suppression in this pinned path. Thus a saved H-pattern label does not disable valid `shiftUp`/`shiftDown` bindings. |
| Guest projection | `src/input/wheelkit_input_driver.cpp:24`, button table, and `Project:56` | `shiftUp` becomes B, `shiftDown` becomes X, clutch becomes LB at half travel. Steering is left X; throttle/brake are RT/LT. `gear1`-`gear8`, `neutral` and `reverse` are never read into the projected pad. Its zero-initialized right X/Y remain zero. No gate-to-paddle pulses are synthesized. |
| Capabilities | `src/input/wheelkit_input_driver.cpp:122`, `GetDeviceCapabilities` | Gamepad subtype 1, no FFB flag/motors, no right-X shifter capability. The unrelated native `input_report_wheel_subtype` switch does not rewrite this driver's hardcoded subtype. |
| Merge/XAM | SDK `src/input/input_system.cpp:430` and `src/kernel/xam/xam_input.cpp`, `XamInputGetState_entry` | Preferred bound steer/pedal axes retain priority; unclaimed right axes keep ordinary merging. There is no later shared-action-to-direct-gear conversion. |

The stored profile label is therefore separate from **the game's own shifting
assist and controller layout**. This source path does not select the game's
Automatic, Manual or clutch policy. No numeric guest setting, owner save field,
or unseen menu state is inferred from the INI. `src/config/default_keys.h:14`
documents B shift-up, X shift-down and LB clutch for the ordinary pad layout;
that layout and actual game shifting mode must be checked during the attended
run. The native wheel layout can use different shift buttons and analog clutch.

An exporter can remove bindings before this pipeline: for example, existing
Wheelkit `DriveShimControls.Gears` selects paddles for Sequential and gates for
H-pattern. Such another game's export can lack `shiftUp`/`shiftDown`, even when
the original saved rig profile contains them. This Core will not reconstruct
missing bindings. Inspect the actual reviewed FH1 INI; do not substitute another
game's profile export or assume the label alone proves its contents.

## Existing native wheel channel

This is an existing source contract, not an implementation added by this task:

- SDK `src/input/sdl/sdl_input_driver.cpp:1358` creates a subtype-2 wheel with
  the FFB capability flag. At `:1375` it advertises right X as -1 when any
  native shifter gate is bound, otherwise 0. A private successor must keep this
  guest protocol marker separate from any claim of physical force delivery.
- `src/input/sdl/wheel_mapping.cpp:81`, `TargetForName`, recognizes native
  `GEAR1`-`GEAR7` and `REVERSE`. `sdl_input_driver.cpp:1590` starts with neutral
  and at `:1622` selects a held gate; `:1639` puts the signed request in right X.
  Native clutch uses right Y (`:1597`), not the private adapter's binary LB.
- The guest's shifter predicate `sub_82BF8F28` requires its internal wheel
  class and shifter flag. `sub_82BFCDD8` then reads the raw signed right X.
  The existing **H-shifter gear** finding in `site/findings/systems.toml`
  records that contract and earlier upstream T248/DragonRise evidence; it is
  not a physical result for this private candidate.
- Caller `sub_82487650` initializes the requested-gear byte at output `+24`
  to **10** (no direct override). Only after the shifter predicate does it
  translate raw **-1 to 0** (reverse), raw **0 to 9** (neutral), and forward
  values below 9 to the requested gear. It separately collects the two shift
  actions into output `+20`/`+21`. These packet values differ from raw channel
  values: do not put the internal neutral byte 9 into raw right X.

The bounded guest check used the fresh private generated trees from the passing
coherent build, matching base XEX SHA-256
`DB40DF605ADE49A612B35A7A24C38F6004BCB17A88ED6B48288DE16DF9E3987C`:
`pinyon_shift_recomp.49.cpp:21126` (predicate), `.116.cpp:20396` (reader),
and `.74.cpp:1216` (control-packet caller). Those generated files remain private;
only this analysis is recorded. The existing reader's v4 counterpart is
`sub_82CBC320`; this task did not establish a v4 mapping for the caller or run v4.

## Smallest playable mapping and remaining gate

Use the existing **gamepad/paddle** contract for the first private input check:

| Logical action | Existing output | Needed owner observation |
| --- | --- | --- |
| `steer`, `throttle`, `brake` | Left X, RT, LT | Calibrated direction, rest and travel, then delivered input and vehicle response. |
| `shiftUp`, `shiftDown` | B, X | Actual bound paddle/button identities, the game's matching pad layout and manual shifting without clutch for this minimal check. Confirm a gear change/release in telemetry and the rendered scene. |
| `clutch` | Binary LB | Optional later manual-with-clutch qualification; this is not the native wheel's analog clutch. |
| H-pattern gates, reverse, neutral | No private output | Keep unsupported. Do not rebind held gates into repeated shift pulses, invent a reverse/neutral button, or write guest gear state. |

An owner-confirmed automatic-shifting game setting can omit manual shift checks;
that does not establish either paddle or H-pattern acceptance. For a paddle
check, prefer a reviewed sequential profile label so the saved intent is clear,
but relabeling alone changes no behavior in this pinned Core. This task changed
neither the saved H-pattern label nor any binding/calibration value.

Direct H-pattern with the **private adapter** requires a coherent capability,
right-X gear, clutch/layout and merge-policy implementation and qualification;
it is not a missing INI value. The existing **native SDL** alternative needs
its own reviewed `wheel_*` device/gate bindings with the private adapter disabled,
because the adapter excludes its selected physical paths from SDL. No bindings
were inferred from active hardware or the saved Moza JSON here.

Leave H-pattern as an attended gate: designated calibrated profile, exact device
identities, actual game transmission/layout, neutral/reverse/conflicting-gate
behavior and safe release all need their bounded qualification. Source support
in the native route does not promote the current private runtime packet, its
uncalibrated draft, or physical force acceptance. No new build or runtime was run.
