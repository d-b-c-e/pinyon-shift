# Wheelkit adapter for Forza Horizon (Pinyon Shift): design note r2.1

Status: **unintegrated source work**, 2026-10-16 (r2.1). The snapshot boundary
has a [source-only fake-provider slice](WHEELKIT-SNAPSHOT.md), followed by the
[owner-thread/frame-notification lifecycle](WHEELKIT-LIFECYCLE.md). The driver,
SDK registration and game callbacks remain unimplemented. No runtime
qualification or working wheel support is claimed.
r1 (`7724e0d2`) followed Astra's direction (hcom 179782). r2 answers Astra's review
(hcom 180219, six findings). Each section names the finding it answers.

Identity: `forza-horizon-1-recomp`, "Forza Horizon (Pinyon Shift)",
`builtIntoGame = true`. It is separate from any retail or emulator entry. FH2's
`windows/` target shares this SDK (ShiftGlue `1e8968f7`), and FM3 (ReXGlue
`v0.10.0`) is the next reuse target. Burnout 3 (xboxrecomp) is a separate ABI family.

## 1. The shim: use the toolkit's existing component (finding 1)

The input shim already lives in the wheel toolkit:
`E:\Source\toolkits\dbce-wheel-mod-toolkit\native\shim`. It contains Core (profile
reader, device epochs and rearm, resolver), the DirectInput provider (nonexclusive,
background, never creates force effects), the host (`host/provider_host.hpp`,
bounded worker ownership, journal, recorder), and the C ABI (`shim_dll/dbce_shim.h`,
ABI 2, 31 actions). Its provenance is in `PROVENANCE.md`. FH1 is **not** an extraction
point. It is one more consumer of that tree.

- **Interface choice:** link the **in-process host** (`host/provider_host.hpp` plus
  `shim/`) statically into the Pinyon app, as the decomp-line consumers do. Do not load
  the managed-adapter DLL, which exists for C# games. Both keep the same lifecycle (P21
  bounded stop) and the same profile reader. Neither adds a parallel reader.
- **Pin:** vendor the toolkit at a tagged commit into `thirdparty/dbce-shim/` with its
  `VERSION`, the same way `lib/toolkit` is vendored elsewhere. Record the pin in the
  build metadata (section 6). A repin is a reviewed change.
- **Force:** the provider never creates effects. Force output stays a separate,
  later path (section 5).

## 2. Configuration without Wheelkit (finding 2)

The profile is the shared controls format: `native/controls/dbce_controls.hpp`,
`[Controls]` / schema 2, in `dbce-controls.ini` beside `pinyon_shift.exe`. **Wheelkit
is one editor of it, not its only writer.**

- **Standalone route:** a WHEEL page in Pinyon's existing in-game SETTINGS overlay
  (`src/ui/settings_menu.cpp`, opened with F6). It follows STD-030 (the F6 panel
  first) and the STD-004/STD-030 shared binding conventions: Bind and Clear, "Not
  bound", "Saved device disconnected", calibration, Save and failure text. It writes
  the same file through the same writer rules, so a profile made in-game and one made
  in Wheelkit are interchangeable.
- That page can be a later slice than the input path, but it is part of this design
  and of the release checklist. **No release ships without it.**

## 3. Thread ownership and the action snapshot (finding 3)

`dbce_shim.h`: a handle belongs to the thread that opened it, and every handle call
must be made on that thread. ShiftGlue's `InputSystem` is reached from guest
`XamInputGetState` calls (guest threads) and host UI reads, and its `devices_mutex_`
serializes them without fixing thread identity.

- **One adapter-owned input thread** opens the host, calls `tick()` on the shared
  cadence the decomp consumers use (one Core tick per game input frame, here
  signalled by the host's frame notification), and stops it with the bounded P21 stop.
  No guest or UI thread ever calls into the shim. There is no fixed-rate promise: guest
  reads can outpace ticks, and they then see the latest snapshot again.
- That thread publishes an **immutable snapshot**: a sequence number, a timestamp,
  axes and held button levels. Each tick builds a new one and publishes it atomically
  (for example a mutex-guarded swap of a `shared_ptr<const Snapshot>`, with readers
  copying the pointer under the same lock). A naive two-buffer index swap is **not**
  enough: a slow reader can still be copying when the writer wraps.
- `WheelkitInputDriver::GetDeviceState` only copies the latest snapshot. Buttons,
  gear shifts included, are the **shared held levels Core resolved**, passed through
  unchanged. No pulses are synthesized, so repeated shifts are neither stretched nor
  merged, and short ones are not lost to the adapter. A repeated guest read of the
  same snapshot sees the same levels, which is how a held physical button behaves.
- **Genuinely edge-only commands** (the host `settings` command in section 6, and
  nothing that reaches the guest) use their own bounded queue: sequence-numbered
  commands, delivered at most once to the host UI thread, with a small fixed
  capacity that drops the oldest and logs the drop.
- **Neutral and rearm:** a snapshot carries a validity reason. When the device is
  disconnected, the snapshot is stale (no tick for more than 100 ms), the window is
  unfocused (unless the profile allows background input) or the host is stopped, the
  driver returns centred steering, released pedals and no buttons. A reconnect or
  refocus rearms through Core's device-epoch rules: held inputs do not fire until
  they are released once.

## 4. The SDK's second remapping layer and capability selection (findings 4 and 5)

**Remap and chords.** `InputSystem::GetMergedState` applies `ApplyPadChord`
(`pad_chord_debug_overlay`, `pad_chord_game_menu`, which includes Back+Start) and
`ApplyPadRemap` to every non-keyboard driver's state, which would include ours.
Wheelkit actions are already resolved, so a second remap could move them again, or
the Back+Start chord could swallow them.

- **Policy:** a narrow, default-preserving opt-out. `InputDriver` gains
  `virtual bool resolves_own_actions() const { return false; }`, and
  `GetMergedState` skips `ApplyPadRemap` for such a driver. **The chords are kept**
  for the host overlay and game menu, so the player can always reach the host UI. The
  adapter maps those host commands to dedicated actions (section 6) instead of
  synthesizing Back+Start. This is a small, reviewed SDK change with identical
  behavior for every existing driver. It is **not** done by reporting the wheel as
  keyboard/MnK.
- **The kept chords do consume their buttons.** When a chord matches on the Wheelkit
  device, `ApplyPadChord` takes those buttons for the host, which is intended. So the
  test cannot expect the whole state untouched.
- **Tests:** a fake-driver unit test with a non-default `pad_remap` and both chords
  set. It checks that the Wheelkit device is not remapped (each resolved action keeps
  its guest button), that a matched chord still consumes exactly its own buttons, and
  that the ordinary pad is still remapped.

**Capabilities.** `InputSystem::GetCapabilities` answers from the user's last
*active* device, or the first assigned one. With MnK, a pad and the wheel on user 0,
the answer, including `SubType`, changes with activity.

- **Policy:** while the per-title wheel experiment is on, user 0's capabilities come
  from the Wheelkit device whenever it is admitted and valid. This is a title-scoped
  preference in the adapter's assignment, not an SDK-wide change. Otherwise ordinary
  selection applies. Merging keeps the SDK rule (pad and wheel axes merge), but the
  wheel's steering and pedal axes are not mixed with a pad's: the wheel device's
  values win for the axes it reports as bound.
- **Vibration fan-out:** `InputSystem::SetState` scales by `pad_rumble_strength` and
  sends the guest's motor pair to every device on the user. `WheelkitInputDriver::SetDeviceVibration`
  **accepts and ignores** it (synthetic, so it never decides the result). Rumble never
  reaches the wheel through this path, and any steering force is a separate channel
  (section 5).
- **Suppression:** every device bound in the profile (the wheel base, a separate
  shifter, a handbrake, pedals) is hidden from the SDL driver for this title only, by
  the identities the shim reports for its bound devices. Unrelated pads stay visible.
  The shim's own DirectInput reader is unaffected (SDL filtering does not touch it).

## 5. Observations and force (finding 6)

**Observed in the generated code** (`.local/generated/default`, traced by mudo; not
independently re-derived by the reviewer):

- FH1 imports `XamInputGetCapabilities` (two call sites), `XamInputGetCapabilitiesEx`,
  `XamInputGetState`, `XamInputRawState` and `XamInputSetState`.
- One producer, `sub_82BFC6F0`, builds the motor pair from `[obj+688]`/`[obj+692]`
  behind three gates. It sends through `sub_82BFA428` (overlapped, deduplicated, fewer
  than 30 in flight) or through `sub_830EB930`, the one direct `XamInputSetState` call.
- `sub_830EB930` has a **compatibility branch**: on a reported version `2.0.x` with
  `x < 5611`, it checks for `SubType == 2` with capability flag bits 0 and 1, and swaps
  the two words.

**Not established:** what those values mean on a wheel, and where they come from.
The branch shows a compatibility path. It does not show a force model. Wheel-specific
force semantics are **unproven**.

**Force plan:**

1. Input ships first, with **no** force output. The F6 page states that force feedback
   is unavailable in this game build.
2. Planned: the portfolio FFB layer (`WheelFfb.dll`, the shared force model,
   STD-025/STD-027/STD-031) fed from vehicle state in guest memory. This needs new
   reverse engineering of speed, steering, lateral g, slip and contact. It is a
   separate channel from guest vibration.
3. Investigation only: trace the writers of `[obj+688/692]`. Use them as "game
   forces" only if they prove to be a real force model, and then only through the same
   FFB layer and strength controls. Rumble is never converted into steering torque.
4. `SubType == 2` reporting stays an opt-in A/B experiment on the owned slot.

## 6. Metadata, actions and the guest mapping (findings 2 and 6)

```json
{
  "id": "forza-horizon-1-recomp",
  "name": "Forza Horizon (Pinyon Shift)",
  "engine": "Native Windows static recompilation (ShiftGlue / ReXGlue, Vulkan)",
  "builtIntoGame": true,
  "configAdmission": {
    "executable": "pinyon_shift.exe",
    "buildMetadata": "bin/pinyon_shift_build.json (fork commit, SDK commit, dbce-shim pin)",
    "controlsFile": "dbce-controls.ini beside the executable (shared format; edited in-game or by Wheelkit)"
  },
  "capabilities": { "wheelInput": "planned", "ffb": "unavailable", "tripleScreen": "planned",
                    "telemetry": "unavailable" },
  "actions": []
}
```

`actions` lists only actions with a verified delivery path. It stays empty until
fake-provider tests and a runtime check prove each one. The candidate mapping keeps
the shared canonical identifiers:

| Shared action | Guest delivery | Notes |
|---|---|---|
| `steer` | `sThumbLX` | signed, full range |
| `throttle` | right trigger | 0-255 |
| `brake` | left trigger | 0-255 |
| `handbrake` | A? (game binding to confirm) | verify FH1's default layout |
| `shiftUp` / `shiftDown` | B / X? | held-for-N-ticks presses |
| `confirm` / `back` / `start` | A / B / Start | menus |
| `lookBack`, `camera` | game bindings to confirm | |
| `settings` | **host command**, not a guest button | opens the F6 overlay directly; never Back+Start |

Wheelkit's generic discovery reports the engine as unverified. Specialized admission
checks the installed executable and build metadata plus this declaration. The applied
profile revision and build go in the adapter's own log. OutRun's receipt schema and
protocol IDs are not reused.

## 7. Order of work

These steps can proceed independently. Review covers shared-interface changes and the
concrete runtime plan, not every small step.

- **Source, no rig:** vendor the pinned shim; build `WheelkitInputDriver` and the
  input thread against the shim's fake provider; add the `resolves_own_actions` SDK
  change with its unit test; trace the force-path writers.
- **Needs the rig:** the actual driving baseline (keyboard and pad, with the owner),
  which is needed to *interpret* runtime results but does not block the source work
  above. Then a read-only device stage under the lease. Then fenced input runs. FFB
  comes later, on its own plan.
