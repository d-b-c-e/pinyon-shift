# Private Wheelkit adapter on the FH1 0.6.1 source candidate

This record describes the frozen `abd3962d/e939542` candidate. The isolated
native direct-gear source successor on this branch is unbuilt; see
[its separate handoff](WHEELKIT-NATIVE-DIRECT-GEARS.md). Do not substitute its
SDK or source into the frozen runtime packet.

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
checks passed at a3defa6. The successor's release revision and gitlink must match
the paired SDK HEAD; the pin-to-HEAD check runs after this commit. The build
scripts and local runner parse. The old a3defa6/4329f0e build candidate is held;
do not use its build recipe for this successor.

The old optional full-app object fixture stopped at a retained generated-header
API mismatch: older `pinyon_shift_pch.h` declares a mutable `PPCFuncMappings`,
while the current SDK and generator template declare it const. Owner-generated
files were not edited or substituted. The queued coherent build must compile
the current generator and regenerate all three private guest module trees before
its SDK/app compile. This object check is not reported as passing.

Earlier evidence remains in the frozen integration worktree's
`.local/nepo-integrated-fixture/`: configure/build logs and the
factory/feedback pass. Earlier 1,270-assertion priority/driver/integration and
87-check pre-open results remain in their original worktrees with their original
scope; they were not relabeled as an integrated game build.

## One coherent Windows build

The prepared local recipe is `.local/nepo-integrated-build/Invoke-CoherentBuild.ps1`
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

No build or runtime acceptance is implied by the source gate. After a successful
build, record executable/runtime/manifest hashes and the exact candidate, then
obtain the fresh coordinator/owner screen handoff for a save-safe qualification.
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
The existing multiscreen investigation remains blocked on a guest scene/view
submission route; the post-presentation observer is not one.
