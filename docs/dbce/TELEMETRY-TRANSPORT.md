# FH1 telemetry transport source fix

October 23, request 354427. This source branch starts at documentation commit
`7acfe710355143a4da55f4ac31cd5e93e6c3d203`. The frozen plain-Play app
`b7e36099e21e5352d264b4f8548864e55f183195` and SDK
`e939542634798fe620d254f56382f6b4e8cae9a3` remain unchanged. This fix has
not had a full app build and is not in those binaries.

## Behavior

The existing sender, settings and FM7/Horizon protocol are retained. No bridge,
worker, service, dependency or new setting is introduced. Telemetry remains
off by default, targeting `127.0.0.1:5300` when enabled without an override.

`telemetry_out::Initialize` prepares an enabled endpoint from OnPostSetup.
Callbacks for enable, target and layout queue Configure through the existing
CallInUIThreadDeferred route. This matters because the SDK invokes flag-change
callbacks while holding its registry mutex: resolving directly in a callback
could still stall guest flag reads. Configuration reads flags before acquiring
the sender lock; resolution and socket preparation happen on the host thread.
The SDK implementation queues even calls from the UI thread.

Open requests FIONBIO on Windows and preserves existing fcntl flags while
adding O_NONBLOCK on POSIX. Setup failure closes the owned handle and frees the
resolved address. Send no longer resolves or opens a socket, uses try_to_lock,
and drops a frame when configuration owns the sender. Each sendto is one
nonblocking attempt; errors and would-block are dropped without retry. The
encoder and player-car producer are unchanged.

When an endpoint has successfully sent telemetry, disabling, changing target
or changing layout attempts one inactive sample using the old endpoint and
layout, then closes it. OnWindowCloseRequested calls Shutdown before the SDK's
accepted-close hard exit; OnShutdown unregisters callbacks and also shuts down.
Shutdown closes even if the final send fails, balances this sender's successful
WSAStartup, is idempotent, and prevents already-queued Configure calls reopening
the socket. A new Initialize starts a fresh lifetime.

Hostname resolution remains synchronous and can pause host configuration/UI.
This is an explicit small-change limit, not an async DNS implementation. A failed
target stays sticky until disable/re-enable or endpoint/layout change; frames
do not retry failures. Final inactive UDP delivery is best effort, and crash,
forced termination, network failure or a stopped receiver can lose it. This
does not prove that SimHub or a device parks; receiver timeout remains necessary
for failures that prevent delivery. The driving flag remains the existing
gameplay/activity heuristic, not an event/session or lap state.

## Inert verification

```powershell
python tests/telemetry/telemetry_out_test.py `
  --cxx 'C:/Program Files/LLVM/bin/clang++.exe' `
  --out .local/telemetry-transport-fixture
```

The fixture extracts the complete production sender namespace and links the
unchanged production encoder. Every socket, resolver, WSA and fcntl name is a
local fake; it includes no platform socket headers, calls no real network API,
and binds no port. Both Windows and POSIX source paths compile with C++17,
Wall/Wextra/Werror. Windows passes 46 assertions, POSIX 47. These totals include
per-socket nonblocking assertions, not 93 independent scenarios.

It covers disabled startup, endpoint preparation outside Send, actual 311/324
payloads and fields, a lock held on another thread, a dropped send, old-target
and old-layout inactive sends, disable and repeated/queued close, failed final
send cleanup, WSA/resolver/socket/nonblocking-setup failures, sticky failure,
and a fresh successful lifetime. Removing the production final-inactive call
causes the target-change assertion to fail. The script also checks deferred
configuration registration and both app close hooks in source; those anchors
are static checks, not an app lifecycle run.

The site builds 37 pages and git diff --check passes. No full product build,
game, SimHub, real UDP, device, owner install/config/save write, admission or
runtime/physical acceptance was performed. A later integration must build this
source and qualify real endpoint selection, live values and receiver parking
under its own authorized slot. Existing built-candidate evidence is retained.
