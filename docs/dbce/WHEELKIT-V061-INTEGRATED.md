# Private Wheelkit adapter on the FH1 0.6.1 source candidate

October 23 reviewed source successor, branch/worktree
`nepo/fh1-v061-source-review-20261023`, based on frozen integration
`a3defa6347d2eb7f529e7f178837f808e9afe1d5`. That integration combines upstream-integration
candidate `f39a3d1502c9d2090cda0ee88895548c76258d08` with preserved adapter
`46a129ec` and the pinned-shim/fixture successor `e47c3546`.

The isolated candidate's SDK gitlink and release revision now name local
`e939542634798fe620d254f56382f6b4e8cae9a3` (parent `4329f0e1`). The paired SDK checkout is a
detached worktree under `thirdparty/shiftglue-sdk`; owner main, original SDK
checkout, installed binaries/configs and saves are untouched. SDK source remains
local only: its sole remote is upstream, which does not carry these private
prerequisites. No portable-source or public-release claim is made.

## Behavior retained

- Ordinary builds keep `PINYON_SHIFT_PRIVATE_WHEELKIT` off. The existing build
  route accepts explicit `-PrivateWheelkit` for a private candidate and records
  that compiled capability plus the shim pin/hash in its build metadata.
- Even that build creates no adapter without `PINYON_SHIFT_WHEELKIT=1`, an
  explicit profile and an existing scratch report directory. Tool mode and
  scripted render routes keep their previous paths. The profile/schema 2 reader,
  calibration, raw bytes and owner-thread immutable snapshot behavior are reused;
  no profile writer, migration or renamed persisted key was added.
- The existing factory wrapper now lives in `wheelkit_app_input.cpp`. Its one
  app caller supplies native preparation; fixtures supply the existing fake
  provider. Preparation precedes the base input factory and path admission
  precedes window attachment. Refusal keeps ordinary input and stops its owned
  adapter candidate. SDL gamepad and raw-wheel exclusions use the paired SDK.
- Focus, presenter-frame notification, close, shutdown and destructor hooks are
  integrated. The merge retains 0.6.1 HDR recovery, frame-inspector lifecycle,
  telemetry/DualSense feedback and save/shutdown work. Native wheel/FFB defaults
  and guest configuration files are unchanged from f39a3d1.
- The exact 16-file `ee8014d4` shared-shim closure, MIT/provenance and package
  metadata are retained from [the paired priority candidate](WHEELKIT-V061-PRIORITY.md).
  F6 standalone shared-profile writing and host settings-command delivery remain
  open; this adapter source does not claim those features.

## Focused gate

Source review found that the native SDL host centering spring ignored F6's OFF
and RUMBLE ONLY modes. The paired SDK now suppresses that spring in both modes
and stops an already running spring through the existing zero-percent path.
Returning to a force mode restores the saved centering value. Title strength
and the title-effects toggle remain independent of host centering in active
force modes; neither alone is an unattended mute contract. Defaults are unchanged.
The production handler and real mode parser, compiled against fake haptics,
fail the cold-OFF regression before the fix and pass **41 checks** after it,
including both mode transitions, inert repeated polling, preserved slider,
effect reuse, unavailable haptics and retained title-effect resynchronization.
Logs are in this worktree's `.local/spring-sdk-fix/`. No SDL/device is opened.

The new factory tag plus retained final-feedback regression pass **80 assertions
in two cases** against current SDK InputSystem and the pinned host/Core. Cases
cover disabled/tool mode, bootstrap exception, admission before native window
attachment, refusal after attachment, a null base result, an absent factory,
unrelated pad preservation and resolved candidate shutdown. No real SDL backend,
DirectInput device, game, window or owner profile is instantiated by this fixture.

Three exact-closure/build-switch checks and two existing source-package contract
checks pass again on this successor. Both post-commit pin-to-HEAD checks pass;
the release revision, gitlink and paired SDK HEAD agree. The build scripts and
local runner parse. The old a3defa6/4329f0e build candidate is held;
do not use its build recipe for this successor.

The old optional full-app object fixture stopped at a retained generated-header
API mismatch: older `pinyon_shift_pch.h` declares a mutable `PPCFuncMappings`,
while the current SDK and generator template declare it const. Owner-generated
files were not edited or substituted. The coherent build below compiled the
current generator and regenerated all three private guest module trees; the new
header declares `extern const PPCFuncMapping PPCFuncMappings[]`. Its full app
compile and link passed. The earlier failed object check is retained as evidence.

Earlier evidence remains in the frozen integration worktree's
`.local/nepo-integrated-fixture/`: configure/build logs and the
factory/feedback pass. Earlier 1,270-assertion priority/driver/integration and
87-check pre-open results remain in their original worktrees with their original
scope; they were not relabeled as an integrated game build.

## Completed coherent Windows build

The build passed on exact clean app
`abd3962d5c2ae101180fbb8d6124344c8da371ae` and SDK
`e939542634798fe620d254f56382f6b4e8cae9a3`, with Release, SSE4.1 and four
workers. Metadata records `wheelkit_private_adapter=true`, shim `ee8014d4`
and manifest SHA-256
`E65907B8C07C9818868A7E9CBC116A46550B2C5748EC5F6192EA2824AF728027`.
Later documentation commits do not change the recorded build source.

Artifacts are under `out/build/win-amd64-release/`:

| File | Bytes | SHA-256 |
| --- | ---: | --- |
| `pinyon_shift.exe` | 65,490,944 | `45F7A83498ECCECF4DF69DA7C2338FFEB326BBF58A1726F13B853BBA21D52051` |
| `rexruntime.dll` | 12,999,168 | `B49294E0C865526100A3B2F184A8732DA59408C2C77EDF0F873C27EE19D7B6EE` |
| `rexgpu-fh1.dll` | 12,255,232 | `3F3AF90DE13B4DC05CD9891E72D0481D189FA9841D4A75B6E666030CB6AD6B70` |
| `pinyon_shift_build.json` | 1,515 | `49EB942D3FF6443F04DF4DAA36BD18D3325BB01650AAF60B08832B1F17AA7718` |

The SDK also staged `upscalers/dlss/310.9.16/nvngx_dlss.dll` and
`upscalers/fidelityfx/1.1.4/amd_fidelityfx_vk.dll`, each with its license.
Any later deployment must preserve these subdirectories; a top-level DLL copy
omits them. The separate LaunchBox fix is commit
`b97b33c119e9415a0cad301a12cbfc8290d09d42`, now verified on `origin/master`;
its upscalers copy uses non-deleting `/E`. It has not been applied to the
owner install as part of this task.

Receipt, stdout/stderr, lease token and process identities are retained in
`.local/nepo-integrated-build/run-01/`. Payload exit code was zero; metadata
was written at 07:33:50Z on October 23. Exact lease
`72004561e50a4691bce471923ca543b0` was released at
**07:34:18.8050881Z**, after a complete safe census of 303 tracked identities:
zero remaining scoped children, `childrenQuiescent=true` and
`processQuerySafe=true`. The lease file was absent afterward. This run used no
game, screens, input injection, device, owner install/config or save writes.
Compiler warnings remain in the stdout log; this is not a warning-free claim.

Both generation passes are retained: the existing cold-build route bootstraps
the three modules, then CMake invokes its dependency-tracked `--ignore-stamp`
pass before compilation. No build logic was changed during this run.

The retained local recipe is `.local/nepo-integrated-build/Invoke-CoherentBuild.ps1`
with `Build-Payload.ps1` and pinned shared process-identity/lease helpers. Enter
only after the coordinator confirms exact predecessor EXIT, cleanup and the new
slot. It atomically acquires the existing shared lease and refuses an existing
lease; no old lock is deleted or reclaimed. An existing output attempt is retained.

Within that lease the payload makes independent dependency checkouts at the SDK's
exact Git pins from the already populated local object stores, reads the retained
toolchain through a private junction, and copies only the three required XEX inputs
into ignored private paths. It neither copies nor opens an owner save. It runs:

```powershell
./tools/build-preview.ps1 -Configuration Release -CpuBaseline sse4.1 `
  -Parallel 4 -PrivateWheelkit -JsonEvents
```

The existing route builds the SDK generator, regenerates private guest code,
builds the SDK/runtime/plugins and app coherently, then writes exact source,
executable, SDK, shim, compile-activation and guest-input provenance. The wrapper
uses the shared owned-process census, retains custody on uncertain cleanup,
and releases only its exact lease after verified quiescence. No game, screens,
input injection, device use or install is part of this build.

## Qualification handoff after build

Compilation, linking and provenance passed; runtime qualification remains open.
Use the exact artifacts above and obtain the fresh coordinator/owner screen
handoff for a save-safe qualification.
Use an isolated state copy/seed through the existing launcher; never pass the
owner's live save as a writable test state. Record a read-only schema 2 controls
profile copy and dedicated scratch reports. Keep unattended physical output muted
(`wheel_ffb_mode="off"`, `wheel_ffb_strength=0`, `wheel_ffb_effects=false`,
`wheel_ffb_centering=0`, `pad_rumble_strength=0`, `pad_ds_triggers="off"`,
no auxiliary force/motion), applied only to the private test state;
attended force assessment requires its own explicit handoff. Verify actual
enumeration/arrival isolation, neutral/rearm and delivered steering/pedal behavior
before making device or driving claims. Preserve owner display/config/save state
and release owned processes/handles before releasing the runtime lease.

The private adapter remains input-only: its selected physical paths are excluded
before native SDL opens them and its synthetic capabilities do not advertise
force feedback. Native SDL force output is a separate route. Source/fixture
results do not prove native device delivery, wheel feel or rendered F6 behavior.
Triples remains unqualified. The separate local doc-only trace `f478e73a`
(`zono/fh1-multiscreen-slots-20261023`) reports a game camera per linked instance
and unresolved SDK transport/runtime gates; it does not add a hook or network
implementation. Follow that handoff separately. The post-presentation observer
is not a guest scene/view submission route, and this build adds none.

## Bounded runtime packet for coordinator notice

**Prepared, held. No launch, lease, device enumeration, UDP bind, display change
or deployment.** This is engineering qualification; release/Play-and-Apply
acceptance remains separate. Follow the hub's shared automated-testing skill
and work policy. Preserve the passing build and fixtures without rerunning them.

### Independent preflight completed

- Built source stays app `abd3962d` / SDK `e939542`, with the exact hashes above.
  All **23** entries in `.local/nepo-integrated-build/run-01/artifacts.json`
  were rehashed successfully. Shared helper hashes still match `helper-pins.json`.
- Preparation receipt: `.local/nepo-runtime-handoff-20261023/preflight.json`,
  SHA-256 `CE05EDF4140D0CB0585A76D8E609DF1DA06765152FCD809E06796255191311DF`.
  It records `prepared_for_runtime=false` and the installed binary baseline.
- The read-only full game root is
  `E:/Source/toolkits/launchbox/Launchbox-Racing/Games/Windows/Forza Horizon (Pinyon Shift)/game`.
  Its three XEX hashes match the build inputs: default `DB40DF60`, SpeechFacade
  `D54F6AAE`, XMediaFacade `ED285BEE`; full hashes are in the receipt. Launch
  the new build directly, with no deployment over the install.
- The sibling installed `state` passed `title-update-profile.py check-base`
  (`refusal=null`). The existing `run-fh1-render-test.py::prepare_isolated_state`
  helper copied it into `.local/nepo-runtime-handoff-20261023/state-muted`.
  Twelve user/config source files matched their private copies and before/after
  hashes; owner state stayed unchanged. Schema 29 may migrate in this copy only.
- New native shader catalogs are absent. Allow a bounded cold start; no automatic
  hidden shader-preparation launch. Refresh owner/copy manifests before entry;
  if the owner has progressed, make a fresh copy without restoring over owner state.

### Entry gates still open

1. Coordinator-designated **FH1-qualified schema-2 controls INI**, exact source,
   revision/hash and calibrated steer/throttle/brake identities. Copy its bytes
   to `inputs/dbce-controls.ini` under the packet and hash them. No qualified
   export was found in the FH1 adapter review, retained FH1 warm/sustained
   receipts or current FH1 handoffs. The active `Moza KS Pro Triple` Wheelkit
   JSON is not a qualified INI. The explicit scratch export below preserves its
   saved values only; it grants no device/calibration acceptance.
2. Named native UI/capture and physical-input operators. This Codex session has
   no enabled native desktop-control surface; do not silently substitute REA,
   shell input or a different provider. The owner can be the physical operator.
3. Advance owner notice, fresh coordinator handoff naming predecessor EXIT and
   cleanup, and an atomically free shared lease. This document grants no entry.

### Shortest scratch profile path

An **unqualified** schema-2 draft now exists at packet path
`inputs/draft-controls-schema2.ini`, SHA-256
`33544C9C1B113A01B83E2364964973A5065F7AFB662864271EC66C9A360DD739`.
It is deliberately not the runtime `inputs/dbce-controls.ini` path. The 27
bindings were copied from the saved active profile through existing Wheelkit
`MappingPresetStore.Load` and `ProfileControlText.Binding(action, input, 2)`;
no new binding parser, identity, axis index, calibration or device read was used.
The JSON store's schema and the exported Controls schema are different formats.

The local `Export-ScratchProfile.ps1` adapter calls those existing production
methods in pinned `Wheelkit.Core.dll` (SHA-256
`5AF5564CC99AFF1CB1B83FBAD47DB2AACF21A7946D051BFDCD361D58B107C896`).
Its own SHA-256 is
`4D35195C43F757C6279C11B18BF027C08FC242290584A3B3F65CA33B3514FD01`.
It writes only scratch files, verifies the source copy/hash and refuses an
existing draft. Export passed and that refusal left the draft byte-identical.
Review `inputs/saved-mapping-profiles.json` and `inputs/draft-provenance.json`
locally; they retain exact source, revision and binding/calibration fields.
Source `mapping-profiles.json` stayed unchanged (SHA-256 `7AE6A254` prefix).
This is a pure offline export adapter, not a shipped FH1 export UI/CLI:
`Wheelkit.StandaloneSetup` does not currently register FH1.

The concrete continuation is an owner-noticed calibration handoff using existing
`AxisCalibrationFlow` and `MappingPresetStore` with the **scratch paths** supplied
explicitly, then rerunning the same formatter into a new review draft. Do not
open the installed UI's default writable profile store for this task. No
calibration/device interaction has been authorized or executed here.

| Fields requiring owner calibration/confirmation | Current draft / required observation |
| --- | --- |
| Every binding: `InstanceGuid`, `ProductGuid`, device name, kind/index, button/hat direction | Saved values only. Confirm the intended attached wheel, pedals and any separate device; never substitute a different connected identity. |
| `steer`: `Minimum`, `Maximum`, `Rest`, `Direction`, `Inverted`, `CalLow`, `CalCentre`, `CalHigh`, `Calibrated` | Uncalibrated; all three measured fields absent. Existing flow captures centered rest, then both locks. Confirm left/center/right and normalized -1/0/+1. |
| `throttle`, `brake`, `clutch`, `handbrake`: the same raw range/rest/travel/inversion and calibration fields | All four uncalibrated, measured fields absent. Existing flow captures released rest, full travel and release. Export writes released..full `cal=` in the measured direction; confirm 0 at release and 1 at full travel. Do not invent endpoints. |
| `Transmission` and requested actions | Saved choice is H-pattern. Direct gear/neutral/reverse delivery is not implemented by this FH1 adapter; owner must choose/review a supported automatic/sequential plan and actual paddles/buttons. Other saved unsupported actions do not become implemented by export. |

Keep `qualified=false` until that source/identity/calibration review completes.
Afterward supply the reviewed exact INI/hash to the coordinator; never silently
promote this draft or fill missing calibration with defaults. Operator and fresh
owner-notice gates still apply independently.

Suggested notice: "FH1 private engineering smoke: one 1280x720 window on the
primary display, up to three minutes to render, then one minute of F6 and
wheel-input checks and cleanup. Selected wheel/pedals and keyboard in use;
wheel force, rumble and motion muted. Private save copy; no display topology
change or installation. Physical FFB and triples are separate gates."

### One muted-output launch after entry gates

Impact: visible window/occlusion, native focus, GPU/CPU load and selected physical
input devices. Writes go only to private state/reports. The named run owner uses
the existing shared owned-process wrapper/census for launcher, game and listener.
Record exact PID/birth/image and actual render HWND/class with a fresh game
scene. Splash, console and MainWindowHandle alone are not RenderReady. Use only
the assigned provider's documented activation/capture route; require native
foreground and game focus before automated stimulus. This sequence has no replay
or injected driving input. Physical controls start centered/released.

Launch payload, with `$root` the source-review worktree and `$packet` its
`.local/nepo-runtime-handoff-20261023` folder. Refuse missing/mismatched profile
or artifact hashes first; keep these variables in the owned child environment
and restore them on exit:

```powershell
$env:PINYON_SHIFT_WHEELKIT = '1'
$env:PINYON_SHIFT_WHEELKIT_PROFILE = Join-Path $packet 'inputs/dbce-controls.ini'
$env:PINYON_SHIFT_WHEELKIT_REPORT_DIR = Join-Path $packet 'reports-muted'
$gameArgs = @(
  '--fullscreen=false', '--window_width=1280', '--window_height=720', '--monitor=1',
  '--input_backend=sdl', '--wheel_enabled=false',
  '--wheel_ffb_mode=off', '--wheel_ffb_strength=0', '--wheel_ffb_effects=false',
  '--wheel_ffb_centering=0', '--pad_rumble_strength=0', '--pad_ds_triggers=off',
  '--telemetry_udp=true', '--telemetry_udp_format=horizon',
  '--telemetry_udp_target=127.0.0.1:55301', '--telemetry_udp_moving_only=false'
)
& "$root/tools/launch-preview.ps1" -Configuration Release `
  -BuildDirectory "$root/out/build/win-amd64-release" `
  -GameRoot $verifiedInstalledGameRoot -StateRoot "$packet/state-muted" `
  -SkipShaderPreparation -GameArguments $gameArgs -Json
```

The existing launcher prepares mod/config copies only in that private state.
Use neither `-Hidden`, v4, a render-test script nor the installed LaunchBox
launcher. Scripted render mode takes a different input path and bypasses the
private adapter factory. Confirm auxiliary force/motion consumers remain muted;
do not send telemetry to the ordinary SimHub/motion port.

Reuse hub `tools/apply-qualification/udp-capture.py` (SHA-256
`51B8CAE99C1D5734CF2AABA21D03E688EFE279DACD898E62E60AD5BB4EA5A95E`),
with existing Python/NumPy. Immediately before launch, its owned child uses
`--port 55301 --seconds 240 --out <packet>/telemetry-muted/live.npz`.
Require a successful loopback bind; do not replace a conflicting listener.
Decode after cleanup using `--decode` and an epoch-time events file. This
collector pads/truncates to 324 bytes: its "sizes ok" text and historical
sentinel check do not establish FH1 wire-format acceptance. Use arrival and
correlated fields as delivery evidence.

| Phase / maximum | Required action and observation |
| --- | --- |
| RenderReady, 180 s from launch | Fresh actual game frame with PID/birth/HWND; retain startup/private migration logs. Bootstrap refusal blocks adapter attribution, while independent startup evidence remains valid. On cold-start timeout close and review the route; no unchanged retry. |
| F6/prompts, first 20 s after readiness | Open F6 once, visit CONTROLS / RACING WHEEL, capture layout/OFF/clipping, close and observe return to game. Capture an actual guest action prompt and record its glyph/text against the designated physical binding. Xbox glyphs do not prove physical wheel-button labels: the adapter advertises subtype 1. Native wheel counts can omit excluded adapter devices; keep that UI defect separate from steering response. Do not enter mapping/calibration or alter force settings. |
| Delivered input/vehicle response, next 30 s | With keyboard/pad driving input idle, the physical operator briefly presses/releases throttle, turns a small amount left/right, then brakes/releases in a safe driving scene. Mark epochs and capture fresh scene response. Require matching throttle/brake/steer telemetry, speed/visible vehicle response and return to neutral. Acquisition/counters alone are insufficient. If the save cannot reach a driving scene, report startup/UI only. No guessed gears or story unlocks. |
| Neutral/focus return, final 10 s | Release controls; if the assigned provider contract permits, perform one supported focus-loss/return check and observe released levels/no stuck command. No held-input reconnect or hardware unplug in this first smoke. Missing native/game focus witnesses leave this assertion incomplete. Begin cleanup within 5 s of the final useful assertion. |

Keep interactive work within 60 seconds after readiness. Preserve completed
gates and clean up if a later assertion cannot fit; do not extend occupation
with idle model waits. Report startup, interactive, cleanup and lease times
separately. Standalone shared-profile writing/settings-command delivery remain
absent and are not accepted by viewing the native wheel page.

### Cleanup and restoration

Release pedals/buttons and request ordinary game close through the supported
route, with a 30-second close budget. Use existing exact-identity cleanup on
failure; never kill an unrelated game or reused PID. Unknown identity, provider
shutdown or restoration retains custody/lease and goes to the coordinator.

Retain `reports-muted/host-shutdown.json` matching game PID/creation time, with
`state=resolved`, `inputReleased=true`, `held=0` and no unresolved items. The
current build uses `NoCapture`: this report proves shutdown, not a per-frame
input/path journal. Correlate it with physical input and telemetry to identify
the adapter session; exact live SDL path isolation needs its own runtime path
witness if unavailable. Preserve bootstrap/admission refusals and any
`wheelkit.shutdown.unresolved` event.

Close owned listener/capture through their documented routes. Require a complete
safe process census, zero scoped children and provider/input release. Compare
owner user/config hashes, installed executable receipt and display topology
before/after. Never copy private save/settings back. Restore the original
foreground target through the supported provider if its identity still matches;
release the provider session when supported. Release the exact lease before
decoding, documentation or archiving. Preserve private evidence on all outcomes.

### Physical FFB and triples are separate gates

Unattended work never enables force. OFF/RUMBLE spring suppression has 41
fake-haptic checks; muted smoke does not establish physical feel. Owner-attended
FFB needs a fresh explicit handoff, known base/tuning/output limits and emergency
stop. This input-only adapter excludes its paths from SDL: native FFB needs a
separately configured private SDL route with the adapter disabled, verified
bindings and the owner's chosen conservative settings. Do not enable it silently.

For triples, record the unchanged display baseline and ordinary single-view
window only. No resolution/Surround/layout changes, fullscreen spanning or
multiple instances. A wide image does not establish independent views. Zono's
local `f478e73a` trace and SDK XNet/address/port gaps remain the architecture
gate; source instrumentation and a separately authorized trace precede linked
instances. Wheelkit's `triplesOn` preference is not FH1 multi-view acceptance.
