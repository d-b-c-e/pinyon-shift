"""Compile production Activation parsing/hash/route with scratch modules and fake lookup."""
import argparse
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--sdk", required=True, type=Path)
parser.add_argument("--cxx", required=True)
parser.add_argument("--out", required=True, type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
source = (root / "src/input/wheelkit_native_input.cpp").read_text()
begin = source.index("std::filesystem::path ModulePath(")
module_path = source[begin:source.index("\n}", begin) + 2]
begin = source.index("  WheelkitAppInputOptions options;", source.index("PrepareNativeWheelkitInput("))
route = source[begin:source.index("  return std::make_shared<WheelkitAppInput>", begin)]
fixture = (root / "tests/input/wheelkit_activation_test.cpp").read_text()
fixture = fixture.replace("// PRODUCTION_MODULE_PATH", module_path)
fixture = fixture.replace("// PRODUCTION_ROUTE", route)
out = args.out.resolve()
out.mkdir(parents=True, exist_ok=True)
generated = out / "wheelkit_activation_generated.cpp"
generated.write_text(fixture)
shim = root / "thirdparty/dbce-shim/native/shim/shim"
sdk = args.sdk.resolve()
executable = out / "wheelkit_activation_test.exe"
subprocess.run([args.cxx, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                "-I" + str(root / "src"), "-I" + str(shim),
                "-I" + str(root / "thirdparty/dbce-shim/native/controls"), "-I" + str(sdk),
                str(generated), str(shim / "parse.cpp"),
                str(sdk / "thirdparty/crypto/sha256.cpp"), "-o", str(executable)], check=True)
with tempfile.TemporaryDirectory(prefix="pinyon-activation-") as temporary:
    subprocess.run([str(executable), temporary], check=True)
