# Source-only first-run packet for the built private candidate

This packet names exact built app `b7e36099` / SDK `e939542`; see the
[coherent build receipt](PLAIN-PLAY-BUILD-RECEIPT.md). It is **not runtime ready**.
No game, device, screen, install, save, live activation or Wheelkit admission was
used or changed while preparing it. The frozen build checkout remains `b7e36099`.
Native direct gears and later native-force safety SDKs are separate successors.

Packet root, beneath the built worktree:
`.local/nepo-first-run-packet-20261023/`.

## Concrete files and pins

| Packet file | SHA-256 |
| --- | --- |
| `staging-plan.json` | `BE66579DD970D3B8B646EB007272E144420065D1E6B9EBDBF9F58DFD8B08006F` |
| `game-arguments.json` | `5C570A2268C5F99B4874A9D18A5D748C2A0364604B570220CD4783FECB52E5EE` |
| `inputs/dbce-controls.ini.review` | `A8D76BAB42619628401F94A7AC07BCEFC6622F57C847FF54668E496188A88DB5` |
| `inputs/profile-provenance.json` | `85BBA6ED9BF8B8A1835E3E418E9900E3FF68338D8DC8CD22AF7F38D4819CC5EE` |
| `Export-ReviewProfile.ps1` | `7FAEE52E8F28C3D9FFE17891E3526BF51937EB87AC7CB4651EC872545CBEA1EA` |

The staging plan names all 23 files from build `run-01/artifacts.json`
(`F2EEB89F...`) and their destination under a future private layout:

```text
private-layout/
  bin/       all 23 artifacts, retaining upscalers/<vendor>/<version>/ paths
  game/      complete private base-game assets, not just three XEX files
  state/     fresh private non-owner state or an independently authorized seed
```

The binaries already exist in the build output. No second binary layout, game
assets or save/state bytes have been staged here. The state/seed and supported
native operator are still missing entry prerequisites; the earlier owner-save
copy is not used by this packet.

At a later authorized staging handoff, copy the closure into private `bin` and
verify every hash. Only then would the operator place the reviewed INI beside
that private executable as **`bin/dbce-controls.ini`**. The `.review` name is
deliberate: it is not recognized by automatic activation where it sits now.
Do not place it beside the owner executable or alter the original build output.

The review file has a first `[Activation]` containing exactly:

```ini
Executable = 3196CFE4DE418BA84AF0137543099F3586E7B455E88F6F73701CD33357F5EC80
Runtime = 31A41222B8EC6356EA3679BA96F99E03465D1CB38F4D41D1C348BDF735990C60
```

This is a manually prepared **review artifact**, not a Wheelkit Apply/admission
receipt. Builds remains unmodified. The production parser/hasher/profile planner
was compiled in a small inert verifier: the bytes match both built modules,
Schema 1 is usable, 15 supported actions bind two devices, no invalid keys or
direct-gear actions, and no provider was constructed. Retained result:
`profile-check.log` (`FCDA9325F5247E5DAB9804F7A472626D0387D3F80AF68F9BEF35177CB144655D`).

## Saved profile and its limits

The source is the prior private mapping snapshot with SHA-256
`7AE6A25436E98A4601AED5510FF6B8131441D54A56322DDAEA2669214032D4F2`.
The existing MappingPresetStore/ProfileControlText formatter was reused from
Core DLL `5AF5564C...`; it read a private copy and wrote only this packet.
The original 27-binding snapshot is preserved, while the review INI exports
only the 15 actions this build delivers. No binding, axis value or endpoint
was invented. Profile: **Moza KS Pro Triple**, revision
`EB856210B4C344D2A086401246D42D16E237E3980CA24DB43577062F6C99D0F0`.

All five saved analog axes have `Calibrated=false` and null measured endpoints:

| Action | Saved axis | Saved range | Saved rest | Saved travel |
| --- | ---: | --- | ---: | ---: |
| Steer | 0 | 0..65535 | 33265 | -1 |
| Throttle | 2 | 0..65535 | 0 | +1 |
| Brake | 5 | 0..65535 | 1743 | +1 |
| Clutch | 6 | 0..65535 | 0 | +1 |
| Handbrake | 7 | 0..65535 | 0 | +1 |

Schema 1 is supported; those saved values are not measured calibration proof.
Before attributing vehicle response, the operator must confirm current identity,
range, released values, travel and steering sign. A discrepancy stops that
assertion; the packet does not silently rebind, invert or calibrate anything.

Shift-up/down are saved on **DS-8X buttons 9/10**, not R12 paddles. Both DS-8X
and R12 must satisfy the existing exact-instance/product/path bootstrap. A missing
bound device refuses the whole adapter. The saved H-pattern label remains metadata:
this build delivers B/X incremental shifts, binary LB clutch, A handbrake,
left-X steering and RT/LT pedals. It delivers no direct H gates, neutral/reverse
selection or analog native clutch. The actual game assist/layout must be confirmed;
automatic mode can omit manual shifts but does not prove them.

## Bounded operator run, only after a fresh handoff

Require owner notice, one shared runtime lease, a named supported native operator,
complete private base-game assets, private non-owner state/seed with a controllable
car, and a complete owned-process census. No new launcher or input injector is
proposed. Use the existing owned-process/native-operator route.

For direct plain Play, start the **private** executable from private `bin` with
both root variables and `PINYON_SHIFT_WHEELKIT` genuinely absent in its child
environment. That exercises sibling-root selection and hash-bound automatic
activation. Clear inherited profile/report/render-test variables. Never set an
empty enable variable as a substitute for removing it: a present non-1 disables
automatic activation. A hash mismatch must keep ordinary input, not use stale
controls. The existing developer `=1` route is a separate diagnostic and bypasses
the Activation check; do not count it as plain-Play proof.

`game-arguments.json` pins one 1280x720 primary-monitor window, SDL input,
`wheel_enabled=false`, wheel mode OFF, strength/effects/centering zero,
pad rumble zero and adaptive triggers OFF. Set the corresponding `REX_*` child
environment values to the same muted values so inherited overrides cannot change
the effective mode. Verify the effective runtime values. Physical and auxiliary
force/motion output stays muted. Restore the launching environment afterward.

Telemetry goes only to the private passive collector at `127.0.0.1:55301`, Horizon
layout, moving-only false. Mirror those values in the child's `REX_TELEMETRY_*`
environment too. Do not send the first input run to SimHub/motion ports. Reuse hub
`tools/apply-qualification/udp-capture.py`; it pads/truncates to 324, so arrival
and correlated fields are useful but its recorded size is not wire-ABI proof.

Allow 180 seconds for RenderReady, 60 seconds for focused assertions, 30 seconds
to close, starting cleanup within five seconds after the last assertion. Retain
PID/birth/window identity, source/manifest/profile hashes and bootstrap logs.
Check F6 and actual prompts (gamepad/Xbox glyphs are expected, not wheel-label
proof), steer/throttle/brake against car motion and telemetry, optional actual
DS-8X shifting, then focus-loss neutralization and close. A bootstrap refusal is
ordinary-input fallback, not private wheel success. A cold timeout ends the
bounded attempt; do not extend it or enter render-test mode, which bypasses the
private input factory. Shader preparation, if needed, is a separate operation.

## Preflight, rollback and boundary

Rehash staged files/profile and verify base XEX `DB40DF60...`; refuse v4 state.
Record private state/config hashes, effective output mutes, planned process census
and current foreground/display state before launching. No owner config/install,
bindings, calibration, save or topology is changed. Do not reuse the owner's
LaunchBox entry, apply the review file to an owner install, or admit a build here.

On close or failure, stop only owned PID/birth identities, release input/provider
ownership, retain `state/wheelkit/host-shutdown.json` with matching identity,
resolved/inputReleased true, held=0 and no unresolved devices, then verify the
complete child census. Stop the passive collector, restore environment/foreground
through the supported operator, retain private outputs and release the lease only
after quiescence. No owner-state rollback writer is necessary for this private
plan; any unexpected owner-file change is a failed boundary to investigate,
not something to overwrite silently.

These are prepared source artifacts and static checks, not runtime acceptance.
Native FFB, physical force feel, separate native steering and direct H-pattern
remain outside this input-only packet.
