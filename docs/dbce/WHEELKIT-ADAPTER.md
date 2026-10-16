# Wheelkit adapter for Forza Horizon (Pinyon Shift): design note r1

Status: **design only**, 2026-10-16. Nothing here is implemented, merged or tested.
It follows Astra's direction (hcom 179782): Wheelkit's shared input profile/Core at the
host input-provider boundary, then a thin per-title adapter into guest XInput. The
guest wheel subtype is an opt-in addition for the selected title, not a global SDK
change. No rumble is converted into steering force.

Identity: `forza-horizon-1-recomp`, "Forza Horizon (Pinyon Shift)",
`builtIntoGame = true`. It is separate from any retail or emulator entry. FH2's
`windows/` target shares this SDK (ShiftGlue `1e8968f7`), and FM3 (ReXGlue
`v0.10.0`, the same input lineage) is the next reuse target. Burnout 3 (xboxrecomp) is
a separate ABI family.

## 1. Provider seam (host side)

ShiftGlue routes the guest's XAM input calls (`src/kernel/xam/xam_input.cpp`) through
`rex::input::InputSystem` to a list of `InputDriver`s
(`include/rex/input/input_driver.h`):

| Driver call | Guest call it serves |
| --- | --- |
| `EnumerateDevices` | device discovery; `DeviceInfo` carries `ordinal`, `synthetic` |
| `GetDeviceCapabilities(id, flags, caps)` | `XamInputGetCapabilities(Ex)` |
| `GetDeviceState(id, state)` | `XamInputGetState` (and `XamInputRawState`, see section 2) |
| `SetDeviceVibration(id, vib)` | `XamInputSetState` (two 16-bit motor words) |
| `GetDeviceKeystroke` | `XamInputGetKeystroke(Ex)` |

`InputSystem::AddDriver` appends drivers (`src/input/input_system.cpp`: today SDL or
XInput by `input_backend`, then MnK, then NOP). `SlotAssignment` maps devices to guest
users by ordinal, and synthetic devices go to user 0
(`src/input/device_assignment.cpp`). `state_merge.cpp` merges several devices for one
user.

**Proposal:** a `WheelkitInputDriver` registered by the Pinyon app shell (title code,
not the SDK's default construction) only when the shared profile selects a wheel for
`forza-horizon-1-recomp`:

- It owns **one synthetic device on user 0**. Its state comes from the portfolio
  input shim Core: the C++ core on the OutRun decomp's `input-shim` branch,
  `windows/shim/` (`core.cpp`, `dinput_provider.*`, `control_pump.hpp`, `parse.cpp`,
  `shim.hpp`). It reads the Wheelkit-written profile (`dbce-controls.ini`), the device
  identity and the normalized actions. FH1 is its **second consumer**, which is the
  agreed trigger for moving that core into the toolkit (Astra 8815) instead of copying
  it.
- **No duplicate pad.** The bound wheel's SDL device must not also reach user 0 as a
  gamepad. Filter it inside the SDL driver for this title only, by the shim's chosen
  device identity (VID/PID plus instance). Do not use a global ignore list, which
  would also hide the wheel from Wheelkit's own reader.
- **Keyboard and ordinary pads keep their native paths.** MnK stays its own driver,
  and other pads stay on SDL for users 1-3 or as merged input on user 0, as today. The
  keyboard is never remapped by Wheelkit.
- **Actions advertised:** only what the adapter consumes: steer, throttle, brake,
  handbrake, gear up/down, confirm/back/start, camera, look back, and the F6 settings
  overlay key. Advertise nothing else.

## 2. Guest calls (traced in the generated code, `.local/generated/default`)

FH1 imports `XamInputGetCapabilities` (two call sites), `XamInputGetCapabilitiesEx`,
`XamInputGetState`, `XamInputRawState` and `XamInputSetState`.

- **Force/vibration output has one producer and two senders.**
  - `sub_82BFC6F0` builds the motor pair from a controller object: `left = [obj+688]`,
    `right = [obj+692]`, sent only when three gates are set (`[ctx+28273]`,
    `[obj+676]` and `[obj+5632]`), otherwise zeros.
  - If `[obj+4244]` is set it sends through `sub_82BFA428`, an **overlapped**
    set-state (`sub_8310CBC8`, result 0 or 997 = I/O pending). That sender skips
    repeats of the last pair (`[obj+1384/1386]`) and caps the number in flight
    (`[obj+1388] < 30`).
  - Otherwise it sends through `sub_830EB930`, the only direct `XamInputSetState` call.
- **The game knows about the Microsoft wheel.** `sub_830EB930` reads a version
  structure through `0x820008CC`. When it is `2.0.x` with `x < 5611`, it calls
  `XamInputGetCapabilities(user, XINPUT_FLAG_GAMEPAD)` and checks **`SubType == 2`
  (wheel)** with capability flag bits 0 and 1. On a match it **swaps the two motor
  words** before sending. So on a wheel the pair carries wheel-specific meaning, which
  an old wheel firmware or driver expected in the other order.
- **Not yet traced:** what writes `[obj+688]`/`[obj+692]` (a force model, or engine
  and road rumble); the semantics of the pair on a wheel (signed torque? magnitude plus
  direction?); what `XamInputRawState` reads; and whether any other code reads
  `SubType` to switch the steering model.

## 3. FFB path and gaps

1. **Do not treat the motor pair as steering torque** until section 2's open items are
   traced and the payload is understood on a real 360 wheel trace or in documentation.
   Microsoft's PC documentation describes subtype `0x02` input (steering on left stick
   X, accelerator RT, brake LT) and `XInputSetState` as vibration only. It does not
   establish the 360 guest semantics.
2. **Until then, use the portfolio FFB layer** (`WheelFfb.dll` and the shared force
   model, STD-025/STD-031) fed from vehicle state read in guest memory. That needs
   the speed, steering angle, lateral g, slip and collision addresses, which are new
   reverse-engineering work. Label the gap in the F6 panel: forces come from Wheelkit's
   model, not from the game's.
3. **If the game's wheel path proves to be a real force model** (`[obj+688/692]` from
   tyre and steering state), Wheelkit can apply it as an opt-in "game forces" mode
   through the same FFB layer, with the shared strength controls (STD-027).
4. **Reporting `SubType == 2`** is an opt-in experiment on the owned slot only. It may
   change the game's steering handling. Settle that by a recorded A/B before it is
   ever on by default.

## 4. Capability and identity metadata (proposed)

```json
{
  "id": "forza-horizon-1-recomp",
  "name": "Forza Horizon (Pinyon Shift)",
  "engine": "Native Windows static recompilation (ShiftGlue / ReXGlue, Vulkan)",
  "builtIntoGame": true,
  "configAdmission": {
    "executable": "pinyon_shift.exe",
    "buildMetadata": "bin/pinyon_shift_build.json (fork commit + SDK commit)",
    "controlsFile": "dbce-controls.ini beside the executable, written only by Wheelkit"
  },
  "capabilities": { "wheelInput": "declared", "ffb": "wheelkit-model (game forces: untraced)",
                    "tripleScreen": "planned (game multiscreen mode, FB-12.18)", "telemetry": "none" },
  "actions": ["steer", "throttle", "brake", "handbrake", "shiftUp", "shiftDown",
              "confirm", "back", "start", "camera", "lookBack", "settings"]
}
```

Generic discovery reports the engine as unverified. Specialized admission checks the
installed executable and build metadata plus this declaration. Record the applied
profile revision and build in the adapter's own log. Do not reuse OutRun's
receipt schema or protocol IDs.

## 5. Order of work (each step reviewed before the next)

1. An **actual driving baseline** without the adapter, on keyboard and pad, with the
   owner. It is required before any shim runtime result can be interpreted.
2. Finish section 2's traces, offline, in the generated code and the original XEX.
3. Move the shim core into the toolkit (with Astra), then `WheelkitInputDriver` on
   fakes (a pure unit test, no device).
4. The read-only device stage on the rig, under the lease.
5. Fenced game runs: input first, FFB later.
