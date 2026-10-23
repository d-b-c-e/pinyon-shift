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
omits them. The separate LaunchBox source fix is branch
`muzu/fh1-deploy-upscalers-20261023`, tip `b97b33c1`; it has not been applied
to the owner install as part of this task.

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
