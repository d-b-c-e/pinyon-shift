# Composed private FH1 build receipt

The frozen composed source `b7e36099e21e5352d264b4f8548864e55f183195` with clean
SDK `e939542634798fe620d254f56382f6b4e8cae9a3` passes its coherent offline build.
This document is recorded separately; the built worktree remains at `b7e36099`.
Native gears and the later native-force safety SDK are not included.

## Execution and cleanup

- Lease `b8a9b0fa133b44d9ab445f1e5e61d417`, token timestamp
  `2026-10-23T09:12:09.0523760Z`; receipt start `09:12:09.0893145Z`.
- Owned payload PID 44156, birth `2026-10-23T09:12:09.1512641Z`, exit 0.
- **EXIT / lease release `2026-10-23T09:56:00.7060768Z`**; receipt finished
  `09:56:00.7083259Z`. All 326 tracked identities are quiescent, process queries
  safe, zero remaining scoped children, literal lease absent. No custody remains.
- Existing runner pins stayed Entry `5E6BA4E1...` / Payload `69FF9F75...` as
  recorded in [the prepared composition](WHEELKIT-PLAIN-PLAY-COMPOSED.md).
- Release, SSE4.1, Clang 20.1.8, CMake 3.31.10, parallel 4, private adapter ON.
  Independent dependencies, read-only retained toolchain and private XEX inputs.
  Fresh generator, three-module bootstrap codegen (660 written, none reused,
  659.3 seconds), existing dependency-tracked generation, SDK/runtime/GPU/app
  compile and link pass. Warnings and the large-function diagnostic are retained;
  this is not a warning-free claim.
- Metadata schema 3 reports both source dirty flags false, private adapter true,
  shim `ee8014d4ca7f3b38604b0fcc2baef460aabf7021` and manifest
  `E65907B8C07C9818868A7E9CBC116A46550B2C5748EC5F6192EA2824AF728027`.

## Artifacts

Build root:
`E:/Source/games/.worktrees/nepo-fh1-plain-play-composed-20261023/out/build/win-amd64-release`.

| File | Bytes | SHA-256 |
| --- | ---: | --- |
| `pinyon_shift.exe` | 65,516,032 | `3196CFE4DE418BA84AF0137543099F3586E7B455E88F6F73701CD33357F5EC80` |
| `rexruntime.dll` | 12,999,168 | `31A41222B8EC6356EA3679BA96F99E03465D1CB38F4D41D1C348BDF735990C60` |
| `rexgpu-fh1.dll` | 12,255,232 | `0E2BCFB71CCF0EA20863F65101467FE6A1B0194A60AEB3AA42347A6C11D2DFC7` |
| `pinyon_shift_build.json` | 1,515 | `20912CAE8235CB941BC8CB325792E4EB5C4BA8BCB834933257D4EF4AAA2193E8` |

The executable hash equals metadata's `executable_sha256`. The complete 23-file
closure includes the current runtime/facade DLLs, controller database, metadata,
Visual C++ runtime, DLSS/FidelityFX version directories and their licenses.

Receipts under the built worktree's `.local/nepo-plain-play-build/run-01/`:

| File | SHA-256 |
| --- | --- |
| `result.json` | `40CA15660BAF082623A6FB6486635410E9B8D6E459C74D57D23688E8AEA2E76F` |
| `artifacts.json` | `F2EEB89F87436E88EE1245CC02EF62CE62B5C9386FF31767BCC513D51EF20986` |
| `coherent-build.stdout.log` (1,208,414 bytes) | `C608DC9ED292FDBC227F1651F1F312B9D413CD9C4151689B5D43FBA4385AE6B6` |
| `coherent-build.stderr.log` (3,410 bytes) | `D315C5C98EBE9A5587FE65D2B9C6526914C91D7A2F02EA755C224DF384A1B968` |

This receipt proves source/build closure, not rendered UI, actual module lookup,
applied profile admission, physical input, telemetry reception, game/device/force
behavior or owner acceptance. No install, game, screen, device, owner config/save
write or Wheelkit Builds admission occurred. Deployment and first-run
qualification require their own explicit handoff.
