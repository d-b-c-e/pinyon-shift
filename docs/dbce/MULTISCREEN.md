# FH1 linked-console triple screen and the "hydraulics" feed: research notes

Source study only, 2026-10-16 (mudo). Addresses are for the base build Pinyon Shift
targets: `default.xex` image SHA-256
`CD5D64D59240438D65E84CE95ECC462147F5CE78053B808ECFAB7C477FF63BB1`, loaded at
`0x82000000`. The image was expanded with a scratch XEX2 basic-compression expander
following `shiftglue-sdk/src/system/xex_module.cpp`. Code references are from
`.local/generated/default`. Upstream tracks this as FB-12.18 /
[issue #493](https://github.com/arcanite24/pinyon-shift/issues/493).

## The launch options involved

All are fields of the launch-options singleton (`0x832E29A0`, getter `sub_82479E88`),
filled by the per-token visitor `sub_824F8150` with typed matchers:
`sub_82C096E0` (flag), `sub_82C09468` (integer), `sub_82C09648` (float) and
`sub_82C097C0` (integer, the port).

| Field | Option (string address) | Type |
|---|---|---|
| +4946 | `MultiscreenServer` (`0x8200F6C0`) | flag |
| +4947 | `MultiscreenClient` (`0x8200F6AC`) | flag |
| +4948 | `MultiscreenChannel` (`0x8200F698`) | int |
| +4952 | `MultiscreenView` (`0x8200F688`) | int |
| +4956 | `MultiscreenDegreesBetweenScreens` (`0x8200F664`) | float |
| +4960 | `MultiscreenDegreesHidden` (`0x8200F648`) | float |
| +4964 | `MultiscreenSpan321` (`0x8200F634`) | flag (read by `sub_82850B10`) |
| +4965 | `UnsecureSockets` (`0x8200F624`) | flag |
| +4882 | `hydraulicsip` (`0x8200F82C`) | string/flag (see below) |
| +4916 | `hydraulicsport` (`0x8200F81C`) | int |

Retail does not parse the kernel command line (upstream `REVERSE_ENGINEERING.md`), so
setting any of these needs a guest hook on the singleton after the visitor runs, the
pattern Pinyon already uses for its developer options.

## Where the game consumes them

Game init `sub_824843E8` (one caller, in `pinyon_shift_recomp.110.cpp`) copies
Server, Client, Channel, View, DegreesBetweenScreens and DegreesHidden into a local
struct at `r1+320` (`+320` server byte, `+321` client byte, `+324` channel, `+328`
view, `+332` degrees between, `+336` degrees hidden). It checks `hydraulicsip`
(`+4882`) and `hydraulicsport > 0` (`+4916`), then calls `sub_82CF6A40(unsecure,
&struct)`. `unsecure` is 1 when `UnsecureSockets` is set. `sub_82CF6A40` is the next
thing to read: it is the multiscreen/network setup.

RTTI names in the image: `CMultiscreenServer@Network`, `CMultiscreenClient@Network`,
`CMultiscreenServerQueue@Network`, `CMultiscreenClientQueue@Network`,
`CMultiscreenServerComponent`, `CMultiscreenClientComponent`,
`CMultiscreenClientStatus@ForzaUI`. Scene `835_C_MULTISCREEN_SETUP` (`0x8203FD54`).
Camera data: `Driver\TripleScreenAdditionalZOffset` (`0x82074610`; FM3's
`CameraSettings.ini` carries the same key).

So the mode is **one server console plus clients**, linked over the network: one view
each, the side views offset by `MultiscreenDegreesBetweenScreens`, with
`MultiscreenDegreesHidden` presumably covering the bezel. `UnsecureSockets` points to
plain sockets rather than Xbox secure networking.

## Routes to three views on one PC

1. **Three instances, linked.** One server (centre) and two clients
   (`MultiscreenView` left and right), each fullscreen on its own racing screen, over
   loopback. This is the game's own path: its cameras, projection, culling and HUD
   all stay native. It needs the runtime's sockets (XNet/winsock) to carry the link
   between instances on one host, three state folders, and about 3x GPU and CPU. The
   RTX 5080 runs one 1x instance far below its frame budget. The risks are system-link
   discovery on loopback, and the client's dependence on the server's simulation
   stream.
2. **One instance, three views.** Reuse the client camera path (the yaw offset) inside
   one process, rendering the scene three times. Cheaper on CPU, but it needs the
   render loop to run the view three times, which is much deeper work in generated
   code.

Next offline steps: read `sub_82CF6A40` (what server and client set up, sockets or
XNet, the port and channel), find where `MultiscreenView` and the degrees reach the
camera (a reader of the struct's `+328`/`+332` after init), and check what the
socket layer in ShiftGlue implements for system link.

## "hydraulics": a built-in motion-platform feed

`hydraulicsip` and `hydraulicsport` are developer options for sending data to a
hydraulic motion platform at an IP and port (Forza's simulator cockpits). If that
stream carries vehicle state, it is a **ready-made telemetry source** for SimHub and
for the FFB model, with no car-state reverse engineering. Next: find the consumer of
`+4882`/`+4916` beyond the init gate, the socket it opens and the packet layout.

## The runtime's networking (ShiftGlue `1e8968f7`, `src/kernel/xam/xam_net.cpp`)

FH1 imports the full XNet stack (keys, QoS, `XNetConnect`, address conversion) and
Winsock. ShiftGlue implements real host Winsock: `NetDll_socket`, `bind`, `sendto`,
`recvfrom` and `WSAStartup`, plus `XNetStartup`, `XNetGetTitleXnAddr` and
`XNetInAddrToXnAddr`. It stubs the secure-session calls (`XNetConnect`,
`XNetCreateKey`, `XNetGetConnectStatus` and others). The game's own `UnsecureSockets`
option, passed to `sub_82CF6A40`, may bypass the secure key path, in which case route 1
(three linked instances over loopback) needs little runtime work. That is the next
thing to confirm: which XNet calls the multiscreen server and client actually make
with `unsecure = 1` (`sub_82CF6A40` calls `sub_82CED920` and `sub_82CED1E0` three times
each, then `sub_82CF4B60`, `sub_82CF48F0` and `sub_82CF4408`).

## October 23 source boundary at upstream 0.6.1

Inspected in isolated branch `nepo/fh1-triples-authority-20261023`, based on
the checked source candidate `f39a3d1502c9d2090cda0ee88895548c76258d08` and
its SDK `59bc11f546a6ea67eefcad974d0c14041b2ab20d`. The live GitHub API still
reports issue493 open, last updated October16; its sole comment retains the
same missing view/angle-consumer trace. No triple-view implementation is
established by that issue or the existing source notes.

Current source offers these narrower authorities:

- `PinyonShiftViewportAspect` and `PinyonShiftCameraFieldOfView` in
  `src/pinyon_shift_runtime_hooks.cpp` change scalar aspect and vertical FOV.
  The hooks in `config/rexglue/analysis/main-xex.toml` identify gameplay versus
  pause-map callers. They do not supply per-panel yaw, an asymmetric frustum,
  or a three-view submission loop.
- The SDK's `Fh1NativeExecutor::Camera` and `TakeCameraFrame` in
  `include/rex/graphics/vulkan/fh1_native_executor.h` provide previous/current
  clip reprojection, lens and depth after swap for temporal upscaling.
  `src/graphics/vulkan/fh1_camera.cpp` observes the guest's submitted matrices;
  it does not establish a side camera and corresponding visibility set.
- `NativeGuestOutputRenderer` in `include/rex/system/interfaces/graphics.h`
  receives only `kPresented`, must not modify the guest output, and has its
  return value ignored. The app's `guest_output_renderer.cpp` uses it only
  for render-test observation. It is not a pre-render camera callback.
- The existing confirmed viewport finding in `site/findings/rendering.toml`
  already records that changing GPU camera matrices alone was abandoned:
  some transforms escape them and culling disagrees. A correct side view
  cannot be inferred by splitting or reprojecting that finished frame.

The toolkit's shared `TripleLayout.cs` / `Dbce.TripleScreen.Core` can compute
measured panel geometry. Geometry is not the missing authority. The source
prerequisite is to trace `MultiscreenView` and the angle fields after
`sub_82CF6A40` into a proven guest camera/frustum builder and submission
boundary (including the base/v4 mapping), or demonstrate a complete renderer
replay boundary with per-view culling and HUD ownership.

Result: **BLOCKED at the source hook**, not waiting for hardware. No projection
code, unused math helper, guessed address, renderer change or new setting was
added. The source candidate and its wheel/FFB path remain intact; full game
build and runtime still need their separate handoff. Only documentation/site
links and generation are checked here; no math/render-data result is claimed.
The site builds all37 pages and its new section anchor resolves. The two changed
Markdown files have valid links; the whole-checkout link scan also reports two
existing SDK source links missing because this documentation worktree has no
materialized SDK submodule. That is an environment gap, not a clean full scan.
