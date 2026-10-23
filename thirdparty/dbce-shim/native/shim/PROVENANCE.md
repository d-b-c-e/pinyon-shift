# Provenance: shared input shim

Moved from `d-b-c-e/outrun2-decomp`, commit `24dc0f803e6bf111798ff7b74d6c99428cf3400e` (branch `codex/outrun-wheel-parity`,
the OutRun integration line; parent `9c31d666949d836cb6a2a23b98c3e39b2e045b38`). The move commit's message abbreviates
it wrongly as `24dc0f8a`; it is `24dc0f80`. That branch was local when this was copied: push it before anyone relies on
the id remotely. The copy is exact as git stores it: each file's blob id below equals the source repository's,
and it is checked after staging. Line endings follow both repositories' `core.autocrlf` normalization. The directory
layout mirrors the source `windows/` folders, so the decomp can vendor this tree back as a plain copy.

## Authorship and terms

- Every file here is DBCE-authored. `git log` on the source folders shows commits by `d-b-c-e` only; the upstream decomp
  author (`r4dius`, remote `upstream`) has 2 commits in that repository and none under `windows/`.
- No file carries a third-party license header. `shim/sha256.hpp` is written from FIPS 180-4: only the standard's
  constants are shared.
- These files are applied under this toolkit's existing project terms (the repository's MIT `LICENSE`, (c) 2026
  d-b-c-e), as part of the authorized shared-library work. No separate owner statement licenses these files
  individually.
- No decomp, game or runtime source, header or asset is included. The DLL's include closure stays inside these folders
  plus the toolkit's own `native/controls` and `native/session`.
- `native/controls` and `native/session` are the files the decomp had vendored from this toolkit: they are
  byte-identical today, so they are not copied again.

## Source variants in use (consumers unchanged by this move)

| variant | source | DLL SHA-256 | consumers | notes |
|---|---|---|---|---|
| direct provider | `codex/shim-device-path` `f49cfc559f065b778967a7c94068a75df538ec69` | `9B8A036F664CF8F45CCCF82B98E5E4376F93215B60EAC50B6DB965BF498015D8` | DRIVE (rc.12 and later), the Woden private pilot | The consumers pin this hash, ABI 2 and 31 actions. |
| copied-provider bridge | `codex/art-shim-bridge` `574bb8827098779257bd08a89038c787403d15b0` | `7DDC962132819EDAE76EF84CE6509F1936B8C38177A36D498B8ABBCFDCFFBCAF` | Art (installed) | Shares Art's existing native reader. Its additions from base `a9e16010e4d8909b71d1090a8266551ce74acb14`: `bridge_provider.{hpp,cpp}`, `shim_dll/dbce_shim_bridge.h`, bridge exports in `dbce_shim.cpp`, and core/provider changes (`d46096c`, `1e88feb`, `574bb88`; tests `test_shim_bridge.cpp`). **Merged here** since 2026-10-13: see "Copied-input bridge merge" below. Art keeps its pinned DLL until a reviewed adoption. |
| integration line (this copy) | `24dc0f803e6bf111798ff7b74d6c99428cf3400e` | built by `build_test.ps1` (see its receipt) | none yet | A superset of the direct provider inside the DLL closure: an optional `front` provider in `provider_host.hpp` (default off; used by OutRun's WheelFfb bridge), read accessors in `frame_recorder.hpp`, and the absent-device rescan in `core.cpp`/`shim.hpp`. `dinput_provider.*` and `shim_dll/*` match `f49cfc55`. |

No consumer is repinned by this move. Adoption, a repin or the bridge merge each need their own review.

## Copied-input bridge merge (2026-10-13, sana 77241/77425)

The bridge delta is `outrun2-decomp` `a9e16010..574bb88` on `codex/art-shim-bridge`. Its commits are `d46096c`, the
evidence-only `2a11562`, `1e88feb` and `574bb88`. `a9e16010` is also the base of the line this copy came from. Each
file was merged three ways: base `a9e16010`, this toolkit at `fb710df`, and the bridge at `574bb88`.

- **New, byte-identical to the bridge** (same git blob):

  | blob | file |
  |---|---|
  | `e99fc33505b5e3a50378a3cd7351901b77a413ed` | `shim/bridge_provider.hpp` |
  | `172d99db1ddcc45564ae82ef7e0d9552229638d9` | `shim/bridge_provider.cpp` |
  | `901c135917fd8f79b6f0e107c57d5edb0741f471` | `shim_dll/dbce_shim_bridge.h` |
  | `a238dc228e9a33db28c491edbb7326afc8113442` | `tests/test_shim_bridge.cpp` |
  | `cee0ae82d467ab6b0f5cb7efa0efa19f738ea758` | `docs/COPIED-INPUT-BRIDGE.md` (its paths and build notes are the OutRun branch's) |

- **Merged without conflict:**
  - `shim_dll/dbce_shim.cpp` and `shim/dinput_provider.hpp` now equal the bridge's (blobs `65dea610…`, `98f764aa…`).
    The direct provider's metadata change (`f49cfc5` = bridge `1e88feb`) was already here.
  - `shim/core.cpp` gains the optional `Provider::source_read` provenance.
  - `shim/shim.hpp` gains `SourceRead`, `ProviderOwner`, `ProviderShutdownReport` and the sample's source generation and
    sequence.
- **Toolkit side kept.** These conflicts were the bridge's own copy of the device-path commit next to toolkit-only
  additions:
  - `shim.hpp`: `missing_scan_after_us_`;
  - `tests/test_shim.cpp`: `test_missing_rescan`;
  - `tests/test_shim_abi.cpp`: `test_schema2_refused`;
  - `shim_dll/README.md`: the toolkit's paths and its asynchronous-open section, plus a bridge section added here.
- **Merged by hand: `host/provider_host.hpp`.**
  - The worker and release state hold `ProviderOwner`, as in the bridge.
  - The DirectInput constructor keeps the toolkit's signature, its check and ownership order (recorder, then the
    provider takes `input`, then the optional `front` callback over that concrete `DInputProvider`), and
    `controls_reads`.
  - A second constructor takes an owned provider, with no `front`. The bridge's delegating constructor is not used: it
    would create the DirectInput provider before the recorder check.
- **Not taken:** the bridge's `windows/CMakeLists.txt` change. `build_test.ps1` gets the same sources and the
  `test_shim_bridge` target instead.

The direct path's ABI, frame, open arguments, actions and exports are unchanged. The seven bridge exports are additive.
No consumer is repinned.

### Latched bridge control (`f38b855`, sana 78097)

The next commit on `codex/art-shim-bridge`, merged three ways from base `574bb88`.

- **Same blobs as `f38b855`:**
  - `docs/CONTROL-JOURNAL.md`, `docs/COPIED-INPUT-BRIDGE.md`, `host/control_journal.hpp` (the optional
    `inputSource`), `shim_dll/dbce_shim_bridge.h` and `tests/test_shim_bridge.cpp`;
  - the journal fixture and its strict reader, new here: `tests/control_journal/main.cpp` (`b3ed8de7…`),
    `read_journal.py` (`6dfba24d…`), `run.py` (`98704b63…`) and `README.md` (`d0f6013f…`).
- **Adapted:**
  - `tests/control_journal/CMakeLists.txt`: the controls headers come from the toolkit's `../controls`, not the
    decomp's `vendor/controls`.
  - `host/journal_identity.hpp`: the toolkit's `controls_reads` stays where it is, and `input_source` follows it.
  - `shim_dll/dbce_shim.cpp`: the latched journal call names the DLL ABI's controls schema (1) before the bridge's
    `"external-physical-input@1"`.
- **Result:**
  - `dbce_shim_bridge_open` admits `DBCE_SHIM_LATCHED` with the base ABI's fence and mute checks, and refuses
    `DBCE_SHIM_CAPTURE`.
  - Copied physical samples keep their source generation, sequence and timestamps. Only accepted batches are
    injected.

### Pure profile action inspection (`5b96ba8`, sana 78097)

`codex/art-shim-profile-info` `5b96ba8`, the source of Art's installed `723E8A38`, merged three ways from base
`f38b855`.

- **New:** `docs/INPUT-SHIM-PROFILE-INSPECTION.md`. It was the `5b96ba8` blob (`1a34f792…`) when merged, and then gained
  a toolkit paragraph (Rita 78344, sana 78360): the export reads controls schema 1, it passes an empty `Capability`
  (so it validates bindings, not adapter support), and `set_profile` runs once per Core.
- **Same blob:** `shim_dll/dbce_shim.h`. It adds the optional `dbce_shim_profile_action_info` and leaves
  `dbce_shim_profile_info` unchanged.
- **Merged without conflict:**
  - `shim/shim.hpp` and `shim/parse.cpp`: `ProfilePlan` and `plan_profile`, Core's binding admission as a pure function;
  - `shim/core.cpp`: `set_profile` uses it. The toolkit's `set_profile` matched the base there.
  - `shim_dll/dbce_shim.cpp`: the export. It reads at the default controls schema 1.
  - `tests/test_shim.cpp`.
- **Union of both test lists:** `tests/test_shim_abi.cpp` runs `test_profile_inspection` beside the toolkit's
  `test_schema2_refused`.

## x86 ordinary-only build (2026-10-14, sana 97961/99082)

These changes are toolkit-local and were made after the move. For the files they touch, the blob table below records
the moved bytes, not the current ones. They are for 32-bit games, following the HCT assessment in the hub's
`artifacts/x86-shim-probe-20261014`. The x64 DLL's behavior is unchanged.

- **New:** `shim/json_text.hpp` holds `json_string`, moved unchanged from `host/control_journal.hpp`. The journal keeps
  the name `journal_detail::json_string` through a `using` declaration. The DLL's ordinary device and profile JSON now
  uses it without including the journal.
- **New:** `shim/ordinary_only.hpp` defines `DBCE_SHIM_ORDINARY_ONLY`, which is 1 only for `_M_IX86`.
- **Changed:** `shim_dll/dbce_shim.cpp` compiles the journal, recorder and identity includes, the capture and latched
  parts of `open_impl`, `game_channels`, `game_id`, the pending capture rows and `bridge_clock` on x64 only. On x86,
  `dbce_shim_open2`, `dbce_shim_bridge_open` and `dbce_shim_record` refuse, and the `open_args` layout assert pins
  the 80-byte x86 layout.
- **Changed:** `tests/test_shim_abi.cpp` and `tests/test_provider_host.cpp` keep their latched, capture and journal
  cases x64-only. `test_shim_abi` adds `test_x86_refusals` and checks all 20 exports on both architectures.
- **Changed:** `build_test.ps1` gains `-Arch x86`, and an export, machine and import check for both architectures.
- **Unchanged:** `shim/observer_transport.hpp`, with its x64 assert. The x86 build never includes it.

## Declared extras, extension 2 (2026-10-14, sana 100911)

These are toolkit-local and additive. The design and reviews are in the hub's `artifacts/extra-actions-design-20261014`.
- `controls/dbce_controls.hpp`: `fits` and `normalize` gain `Shape` overloads. The action-name versions now delegate
  to them, with identical results.
- `shim/shim.hpp`, `shim/parse.cpp` and `shim/core.cpp`:
  - `Status::ContextInactive` is appended, as 14;
  - the declaration and `[Extras]` types, `parse_declaration` and `plan_extras`;
  - Core's per-action resolve is split into `resolve_binding`, which the 31 and the extras share;
  - `set_profile` and `tick` gain extras overloads; the current overloads are the no-extras case.
- `host/provider_host.hpp`: an optional trailing declaration on the DirectInput constructor, and `tick(contexts)`.
- `shim_dll/dbce_shim_extras.h` (new) and `dbce_shim.cpp`: six exports. `dbce_shim_tick` refuses an extras handle.
- `tests/test_extras.cpp` is new; `tests/test_shim_abi.cpp` gains the extension cases and checks the export names.

## Not moved

- OutRun's adapter and game-side shim code (`windows/source/shim_input.cpp`, `outrun_adapter.*`, `shim_steer.hpp`), and
  the test that needs it (`test_shim_steer.cpp`).
- `shim/wheelffb_bridge.*`, which only OutRun uses today.
- The P21 and control-host fixtures.

## Files (git blob id, source path under `windows/`)

| blob | file |
|---|---|
| `0df2a76e5bed64d070abfcc3f7a28123667ae139` | `docs/CONTROL-JOURNAL.md` |
| `b30c079104a086f0aaf6326591dcf698d5a899c5` | `docs/INPUT-SHIM.md` |
| `cbf22b36675719aae0b4b0f25c7baf6861e45bb9` | `host/control_journal.hpp` |
| `e282931da5e389f1b4ff70ca05cd63cd92e36139` | `host/control_snapshot.hpp` |
| `09856c8e8209cf68f11bc28ae9f6b95c1bf352ff` | `host/host_lifecycle.hpp` |
| `ce0d32040a0181d79ce3088800ea9282444dee05` | `host/journal_identity.hpp` |
| `38e281829141e470627384e422bf8d4b7d04fcb1` | `host/provider_host.hpp` |
| `d6336d5373949618b417768e2dcec0bf9e4bc0a3` | `host/recorder/capture.hpp` |
| `9d6b20ce5fe2d265fcf81f306827fbfc38992d8c` | `host/recorder/frame_channels.hpp` |
| `e0a046dfe77c54d882bfba89cfe2487462562cfe` | `host/recorder/frame_recorder.hpp` |
| `2d3fdab65698399c3bef1597bf98287ff42ed72b` | `host/recorder/worker.hpp` |
| `a78afe704d331569b4f62dc5ff3543de2db16bb9` | `shim/control_pump.hpp` |
| `881496aed55911f874a99f22c2451f0c4d165f82` | `shim/core.cpp` |
| `312584e5f0940516dbc530b60c866aac6d24dc74` | `shim/dinput_provider.cpp` |
| `3e039e4074fefd85ce24818e59b0420ea8e03738` | `shim/dinput_provider.hpp` |
| `c33de82687e7e8833c912afeff0ff16252ce8b57` | `shim/observer_domain.hpp` |
| `94cc00c2838042436153b954bb7be641dfd20b92` | `shim/observer_transport.hpp` |
| `caefe9468ea25eff8d4bda3d69c160e3cb243cc0` | `shim/parse.cpp` |
| `a01d8fb502895e15c138a9dfcdae10fdc2dc1a3d` | `shim/sha256.hpp` |
| `277e90c1dad867086f6f4bf743a2159e70733942` | `shim/shim.hpp` |
| `e2f2302b207260128945deaaa02c92d682166556` | `shim_dll/README.md` |
| `8df5f102acb4539d5e61395d9afaf369fbe03ba2` | `shim_dll/dbce_shim.cpp` |
| `47e264627289370ed486789e423fc76b7d6fcbdd` | `shim_dll/dbce_shim.h` |
| `ab3a662046c4ea3f86205ce15db84c06d4615443` | `shim_dll/dbce_shim_internal.hpp` |
| `d764a3dda797dee03ec90fa8ad19d0e633097c1d` | `tests/shim_fake_com.hpp` |
| `6447f2913ba5c085fda0c64dace0beeb0ca31a1b` | `tests/shim_fake_provider.hpp` |
| `12a030a2e3766943174e17eabd556ed9c97d4ce0` | `tests/test_dinput_provider.cpp` |
| `2197412a5476be86fdfd12d0071b03e161cdb7ba` | `tests/test_shim.cpp` |
| `4cbf4fe5469f905f881fc452aa4da6fbb45060d4` | `tests/test_shim_abi.cpp` |
