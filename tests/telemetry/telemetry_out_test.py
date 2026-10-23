"""Compile the production sender/encoder with inert socket APIs, never network APIs."""
import argparse
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--cxx", required=True)
parser.add_argument("--out", required=True, type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
source = (root / "src/telemetry/telemetry_out.cpp").read_text()
source = source[source.index("namespace pinyon_shift::telemetry {"):]
fixture = (root / "tests/telemetry/telemetry_out_test.cpp").read_text()
app = (root / "src/pinyon_shift_app.cpp").read_text()
setup = app.split("void PinyonShiftApp::OnPostSetup() {", 1)[1].split("#if", 1)[0]
assert "CallInUIThreadDeferred([] { pinyon_shift::telemetry::Configure(); });" in setup
for key in ("telemetry_udp", "telemetry_udp_target", "telemetry_udp_format"):
    assert '"' + key + '"' in setup
assert "RegisterChangeCallback(key, configure_telemetry)" in setup
assert "pinyon_shift::telemetry::Initialize();" in setup
close = app.split("bool PinyonShiftApp::OnWindowCloseRequested() {", 1)[1]
assert close.index("telemetry::Shutdown();") < close.index("StopWheelkitInput();")
assert close.index("telemetry::Shutdown();") < close.index("NotifyShutdown();")
shutdown = app.split("void PinyonShiftApp::OnShutdown() {", 1)[1]
assert "UnregisterChangeCallbacks(key);" in shutdown
assert shutdown.index("telemetry::Shutdown();") < shutdown.index("NotifyShutdown();")
out = args.out.resolve()
out.mkdir(parents=True, exist_ok=True)
# Each mutation: (label, [(production text, replacement)], the check that must reject it).
mutations = [
    ("negative", [("if (g_sent) WriteLocked({});", "if (g_sent) (void)0; // Omit final inactive packet.")],
     "target change parks old endpoint"),
    ("blocking", [("if (enabled && target != g_target) RequestLocked(target);",
                   "if (enabled && target != g_target) { RequestLocked(target); Endpoint blocking;"
                   " (void)OpenEndpoint(target, blocking); if (blocking.socket != kNoSocket) CloseSocket(blocking.socket); }")],
     "configuration returns while the lookup is pending"),
    ("stale", [("g_resolver->done.load() != g_generation) return;", "g_resolver->done.load() == 0) return;"),
               ("  if (g_resolver->done != g_generation || g_resolver->error) return;\n",
                "  if (g_resolver->error) return;\n")],
     "a finished result for an older request is never adopted"),
    ("adopt-logging", [("  g_resolver->result.socket = kNoSocket;\n}",
                        "  g_resolver->result.socket = kNoSocket;\n"
                        "  diagnostics::RecordEvent(\"telemetry.udp.open\", {{\"target\", g_target}});\n}")],
     "a blocked diagnostic never holds up a frame"),
    ("winsock-once", [("  bool winsock = false;", "  bool winsock = StartWinsock();"),
                      ("    if (!winsock) winsock = StartWinsock();\n", "")],
     "a failed Winsock start is retried on the next request"),
]
for platform in ("windows", "posix"):
    for mutation in [None] + (mutations if platform == "windows" else []):
        body = source
        if mutation:
            for old, new in mutation[1]:
                assert old in body, old
                body = body.replace(old, new)
        label = platform + ("-" + mutation[0] if mutation else "")
        generated = out / (label + ".cpp")
        executable = out / (label + ".exe")
        platform_define = "#define _WIN32 1" if platform == "windows" else "#undef _WIN32"
        generated.write_text(fixture.replace("// PLATFORM", platform_define)
                             .replace("// PRODUCTION_SENDER", body))
        subprocess.run([args.cxx, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                        "-I" + str(root / "src"), str(generated),
                        str(root / "src/telemetry/forza_dash.cpp"), "-o", str(executable)],
                       check=True, timeout=60)
        # The pending-lookup checks wait up to 2 s each; a hung mutation is cut off here instead.
        result = subprocess.run([str(executable)], capture_output=True, text=True, timeout=60)
        if mutation:
            assert result.returncode != 0 and mutation[2] in result.stderr, result
            print(label + ": mutation rejected (" + mutation[2] + ")")
        else:
            assert result.returncode == 0, result.stdout + result.stderr
            print(label + ": " + result.stdout.strip())
print("app deferred-config and both close lifecycle anchors pass; no real sockets used")
