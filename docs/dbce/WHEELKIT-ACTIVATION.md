# Build-bound private Wheelkit activation for plain Play

The original slice below is now included with the deployed-root fix in the
[composed plain-Play source candidate](WHEELKIT-PLAIN-PLAY-COMPOSED.md).
The independent original branch and frozen binaries remain unchanged.

October 23 isolated source branch `nepo/fh1-activation-20261023`, directly based
on coherent source `abd3962d5c2ae101180fbb8d6124344c8da371ae`, with unchanged SDK
gitlink/release pin `e939542634798fe620d254f56382f6b4e8cae9a3`. This is a separate
unbuilt successor: neither root fallback `99947700` nor native gears `4da6a32b`
is composed here. Built artifacts, owner main, installations and profiles remain
unchanged. Ordinary builds still compile the private adapter OFF.

## Activation rule

When `PINYON_SHIFT_WHEELKIT` is present, the existing developer route wins:
exact value `1` uses the explicit profile and existing report directory; every
other value disables the adapter and does not attempt automatic activation.
The explicit developer route does not require an Activation record.

When that variable is absent, `PrepareNativeWheelkitInput` looks up the running
executable and the **already loaded** `rexruntime.dll` module. It never loads a
new DLL to satisfy the check. Module paths must fit the bounded Windows buffer.
It reads `dbce-controls.ini` beside the running executable, at most 64 KiB total,
then checks strict UTF-8 and optional leading BOM using the existing shim
reader's UTF-8 check. The profile bytes used for admission are the same bytes
later passed to the existing Controls reader.

The parser follows Wheelkit `IniDocument` with `SettingsFormat.Ini`, verified
at writer `2be1452` (`Settings/ModSettings.cs`, SectionOf/KeyOf/Find/Get):

- A trimmed header must start with `[` and end with `]`; its inner name is
  trimmed and compared case-insensitively to `Activation`.
- Only the **first matching section** counts. Reading stops at the next header;
  a later repeated Activation cannot provide a missing or corrected hash.
- `Executable` and `Runtime` use their first matching key in that section.
  Keys are trimmed and case-insensitive; lines beginning with `;` or `#` are
  comments. Values are the trimmed text after the first `=`.
- There is **no inline-comment stripping** in Ini format. A hash followed by
  ` ; note` or ` # note` is malformed, and `[Activation] ; note` is not a header.
- Both values must be exactly 64 ASCII hexadecimal characters. Comparison
  with the computed hashes ignores ASCII letter case.

The preliminary cross-section/comment grammar in message 349796 was incorrect;
Miho/Muzu corrected it against production source in 349964/349970. Fixture vectors
are shared with Muzu's test-only writer successor `66277a9`. The app does not
require a writer or catalog change for the correction.

The executable and loaded runtime files are hashed with the SDK's existing
streaming SHA-256 implementation, using a fixed 64 KiB buffer. No extra library
or unbounded binary read is added. Existing `e939542` runtime exports include
the streaming add/getHash symbols; this is a source/link-boundary check, not a
new app link result.

Only after both hashes match does the app create `<resolved state root>/wheelkit`
and pass that report folder plus the exact profile bytes to existing adapter
admission. A missing/malformed/oversized/unreadable profile, unavailable module,
hash mismatch or report-folder failure logs one reason and returns ordinary
input before constructing a provider. Existing device/path/bootstrap admission
can still refuse a hash-matched profile. No native-gear or force behavior changes.

This record binds an applied profile to executable/runtime bytes; it does not
admit a build. Wheelkit's writer still has an empty admitted Builds list and
therefore writes nothing for current installs. A redeploy changes the hashes
and leaves a stale record inert. File-presence-only LaunchBox opt-in `6117944`
must stay unmerged because its explicit environment bypasses this build check.

## Focused source fixture

```powershell
python tests/input/wheelkit_activation_test.py `
  --sdk E:/Source/games/.worktrees/nepo-fh1-v061-source-review-20261023/thirdparty/shiftglue-sdk `
  --cxx 'C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/Llvm/x64/bin/clang++.exe' `
  --out .local/activation-fixture
```

**41 checks pass.** The fixture compiles the production parser, bounded reader,
streaming hasher, module-path helper and extracted native activation route.
Module/environment lookup is faked, profiles and module bytes are scratch files,
and the route stops before provider construction. It covers all nine corrected
writer grammar vectors, BOM/Unicode trimming/UTF-8, total 65536/65537-byte edges,
known SHA-256, executable/runtime replacement, the loaded runtime outside `bin`,
missing/truncated lookup, no report creation on mismatch, resolved state reports,
and explicit environment precedence. Log: `.local/activation-fixture/check.log`.
The documentation site builds 37 pages. No full app/SDK compile or link, Windows
module lookup execution, DirectInput operation, game or hardware behavior is proven.

## Delivery gates

Compose the selected source slices into a new reviewed candidate before its
coherent build. In particular, deployed plain Play needs root fallback `99947700`
so resolved state points at the existing sibling state rather than the old
default next to `bin`. Native direct gears remain a separate optional slice.
New binaries require fresh build hashes and explicit Wheelkit admission, then
the writer/catalog successor, calibrated profile, owner-noticed input run and
normal cleanup evidence. This task made no install/config/save write, game or
device/display operation, rig lease or heavyweight build.
