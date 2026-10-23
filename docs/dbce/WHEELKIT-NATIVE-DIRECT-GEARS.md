# Private native direct-gear source successor

October 23 source-only branch `nepo/fh1-native-direct-gears-20261023`, based on
trace `a4a9b7e1` and built source `abd3962d`. Paired local SDK
`d1fcb6d5a4de4c8987c1796b39bd915d7edcbe8d` descends from `e939542`.
The SDK is local only; its upstream remote does not carry these private commits.
The built `abd3962d/e939542` artifacts, owner checkout, profile and installation
remain unchanged. This successor has no coherent product build or runtime result.

## Activation and delivered controls

Ordinary builds remain private-adapter OFF. Even a private build needs the
existing `PINYON_SHIFT_WHEELKIT=1`, explicit profile and existing scratch report
folder. The new mode additionally requires `PINYON_SHIFT_WHEELKIT_NATIVE_GEARS=1`.
Before any provider is constructed, admission requires a usable profile with
`Transmission=H-pattern`, at least one valid explicit forward/reverse binding,
and no invalid gear/reverse/neutral bindings. A neutral-only profile is refused.
No binding, device identity, raw range, calibration or transmission save value
is invented. The old gamepad/paddle path remains the default.

| Control | Native-mode projection |
| --- | --- |
| `gear1` through `gear8` | Raw right X 1 through 8 while the explicit gate is held. |
| `reverse` | Raw right X -1 while held. |
| `neutral` | Raw right X 0 while held; all available bound gates released also means 0, matching SDL. |
| Invalid/unavailable/stale/inactive/rearming/test-neutral inputs, conflicting held gates | Raw right X 10, meaning no direct override. They never become reverse or neutral. |
| `steer`, `throttle`, `brake` | Existing left X, RT, LT projection. |
| `clutch` | Analog right Y 0..32767; no binary LB clutch in native mode. |
| `shiftUp`, `shiftDown` | LB/RB for the source-established native wheel layout 1. Actual guest layout must be confirmed. |
| `camera` | Omitted in native mode because the old RB mapping would become a shift. |
| Other existing button roles | Existing projection retained; rendered/menu behavior remains unqualified. |

An unbound optional gate is skipped. Every configured gear/neutral/reverse
action must be Live with a finite 0..1 value before a selection or implicit
neutral is emitted. Two held gate actions produce no override, including
neutral-plus-reverse. No pulses, reverse button guesses or guest-memory writes
are used. A missing device at bootstrap still refuses the whole adapter through
the existing exact identity/path admission.

Capabilities match the documented native wheel channel: subtype 2, the required
FFB protocol flag, right-X shifter and right-Y clutch masks -1, and SDL's default
left-X mask -1. No 900-degree hardware claim or force calibration is introduced.
The guest predicate/reader/caller and exact raw/internal gear distinction are
recorded in [the static trace](WHEELKIT-V061-SHIFTING.md).

The FFB bit is required for FH1's wheel/shifter class. This driver still forwards
no vibration or force output. Guest XInputdFF calls can reach the existing global
SDL force route for any separately attached native wheel: a future input-only
run must keep wheel output disabled, mode OFF, effects false, centering zero,
and pad output muted. This source result grants no force/feel acceptance.

## Merge policy and pure fixture

SDK `DeviceStatePolicy::exclusive_axes` defaults to zero. Only explicitly
claimed preferred axes can be exclusive; ordinary keyboard steering, pedals,
buttons and all default drivers retain their existing behavior. Native gears
claim right X/Y even on invalid snapshots, so another pad or keyboard stick
cannot turn a small gear code, clutch release or no-override value into another
input. Existing final priority application and feedback ordering are retained.
The new SDK/header must be built coherently with the app; do not replace one DLL
or mix the old built candidate with this source.

Run from this worktree, with the paired SDK checkout and installed Clang:

```powershell
python tests/input/direct_gears_seam.py `
  --sdk thirdparty/shiftglue-sdk `
  --cxx 'C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/Llvm/x64/bin/clang++.exe' `
  --out .local/direct-gears-fixture/positive
```

**133 checks pass.** The fixture compiles the actual gear helper and profile
reader plus extracted production projection, capability method, state merge and
final preferred-axis fold. It covers gear1..8, neutral/reverse/no override,
negative caps, profile admission, every unavailable status, all snapshot gates,
nonfinite/out-of-range levels, conflicts, unchanged default paddles/caps,
analog clutch and all six exclusive-axis masks. A negative SDK copy with only
right-X preservation disabled fails at keyboard gear contamination as expected.
Local logs: `.local/direct-gears-fixture/positive.log` and `negative-control.log`.

The fixture uses plain host integers for SDK big-endian fields, fake states and
a source-derived guest decoder. It does not execute guest code, instantiate a
provider, compile the complete app/SDK, prove serialized ABI or run a device.
App activation/constructor wiring is statically checked. Release/gitlink pins
and site source/link targets are checked; the site builds 37 pages with the
retained site venv. Plain system Python lacks Markdown. No product build was run.

## Remaining qualification

Keep the existing first paddle-only packet and Muzu's writer contract frozen.
This mode needs a separate coherent build, designated calibrated FH1 profile,
verified physical gates/clutch, actual native layout and guest shifting assist,
then an owner-noticed muted run checking vehicle/telemetry gear changes,
neutral/reverse/conflict/release, focus loss and cleanup. The existing saved
Moza axes remain uncalibrated; this source work did not qualify or export them.
No game, device, display, install, owner-state write or rig lease occurred here.
