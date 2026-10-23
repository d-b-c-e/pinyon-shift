# Composed plain-Play roots and build-bound Activation

October 23 branch/worktree `nepo/fh1-plain-play-composed-20261023`, directly based
on frozen coherent source `abd3962d5c2ae101180fbb8d6124344c8da371ae`. It contains
deployed-root fix `99947700` and build-bound Activation `00feecfe` as cherry-picks
`5ee084f2` and `657a0067`. SDK gitlink/release pin stays
`e939542634798fe620d254f56382f6b4e8cae9a3`, with a clean detached SDK worktree.
Native direct gears `4da6a32b` / SDK `d1fcb6d5` are not included. Original source
branches, owner main and built `abd3962d/e939542` artifacts remain unchanged.

Plain Play now selects an existing complete sibling game/state layout when both
root variables are absent, then enables the private adapter only when the same
INI's Activation hashes match the running executable and loaded runtime. Failure
retains ordinary input and the selected roots. Existing developer environment
precedence and profile/device/path admission are retained. See the original
[root](DEPLOYED-ROOTS.md) and [Activation](WHEELKIT-ACTIVATION.md) contracts.
Zono's source review of `00feecfe` (350580) found no required correction. Exotic
non-ASCII case folds in hand-written section/key names can be stricter than
.NET's reader; writer-emitted ASCII keys and normal case variants match.

## Focused checks

- `tests/input/wheelkit_activation_test.py`: **46 checks**, compiling actual
  parsing/hash/module-path/activation logic plus the production early-root
  initializer. The original 41 cases remain; five combined cases use a scratch
  deployed layout to check game/state selection, report placement, no alternate
  state next to bin, ordinary input after runtime replacement and unchanged
  roots on refusal. Module/environment lookup is faked; no provider is created.
- `tests/deployed_roots_test.py`: **17 checks** on scratch paths, retaining the
  original precedence/incomplete/type checks.
- The site builds **37 pages**; source/link targets and unchanged SDK pins pass.

Logs are under `.local/plain-play-fixture/{combined,roots}.log`. These are source
fixtures, not an owner-install launch, complete app/SDK link, real Windows module
lookup, physical wheel or force result. All tests write only owned scratch data.

## Prepared build runner, not started

`.local/nepo-plain-play-build/` reuses the previous coherent build's existing
runner and payload. Only its purpose label changes. PowerShell AST parsing and
all three original shared-helper SHA pins pass. Script hashes:

| File | SHA-256 |
| --- | --- |
| `Invoke-CoherentBuild.ps1` | `5E6BA4E140C9A6B4C6023276A51075246EF81B4EF2C2E928CC042C2CB07BECC5` |
| `Build-Payload.ps1` | `69FF9F7584F026934922667B971ED06F81EF8EDFEEB0BD75872A44AEB7CE4813` |
| `helper-pins.json` | `DC4292899D6C166810EA77330EE6A66EF29A606C22098D203F7E205962A16A9B` |

After a fresh coordinator build handoff, invoke from this worktree:

```powershell
pwsh -NoProfile -File .local/nepo-plain-play-build/Invoke-CoherentBuild.ps1 `
  -ExpectedCommit <exact composed commit> -Attempt 1
```

The runner refuses changed/dirty source, changed helper pins, a pre-existing
attempt or predecessor lease. It acquires the existing shared build lease,
starts the payload through the existing process-census helper, and releases only
after verified quiescence. The payload creates independent pinned dependency
checkouts using immutable local Git object caches, reuses the read-only toolchain,
makes private copies of three XEX inputs, and runs the existing Release/SSE4.1/
parallel-4/private-adapter build route. It checks the resulting app/SDK/shim pins.
It neither installs nor launches the game. No runner, payload, lease or heavy
build was invoked during preparation. A post-commit preflight record pins the
exact source and runner bytes in the local build folder.

## Remaining gates

The current executable does not contain either fix. A clean coherent build must
produce new hashes, followed by an explicit redeploy and build admission through
the selected Wheelkit writer/catalog successor. File-presence-only opt-in remains
unmerged. First input-only qualification stays separate from the native force
route and its focus/close safety patch. No owner install/config/save, game,
device, display or runtime evidence is changed by this composition.
