"""Keep the private shim distribution exact and closed over quoted includes."""
import hashlib
import json
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
VENDOR = ROOT / "thirdparty/dbce-shim"


class DistributionTest(unittest.TestCase):
    def test_exact_source_and_include_closure(self):
        manifest = json.loads((VENDOR / "PROVENANCE.json").read_text())
        self.assertEqual((VENDOR / "VERSION").read_text().strip(), manifest["commit"])
        self.assertEqual(manifest["commit"], "ee8014d4ca7f3b38604b0fcc2baef460aabf7021")
        paths = {item["path"] for item in manifest["files"]}
        self.assertTrue(set(manifest["roots"] + manifest["fixtureRoots"]) <= paths)
        self.assertTrue({"LICENSE", "native/shim/PROVENANCE.md",
                         "native/shim/shim/candidate.hpp"} <= paths)
        for item in manifest["files"]:
            path = VENDOR / item["path"]
            blob = path.read_bytes()
            with self.subTest(path=item["path"]):
                self.assertEqual(hashlib.sha256(blob).hexdigest(), item["sha256"])
                self.assertEqual(hashlib.sha1(f"blob {len(blob)}\0".encode() + blob).hexdigest(),
                                 item["gitBlob"])
                for include in re.findall(rb'^\s*#\s*include\s*"([^"]+)"', blob, re.M):
                    name = include.decode()
                    candidates = [path.parent / name] + [VENDOR / p / name for p in
                                                        manifest["includeDirectories"]]
                    self.assertTrue(any(p.is_file() and
                                        p.resolve().relative_to(VENDOR.resolve()).as_posix() in paths
                                        for p in candidates), name)

    def test_private_build_and_package_use_tracked_source(self):
        cmake = (ROOT / "cmake/PinyonShiftWheelkit.cmake").read_text()
        self.assertIn('set(PINYON_SHIFT_WHEELKIT_SOURCE "${PROJECT_SOURCE_DIR}/thirdparty/dbce-shim")', cmake)
        self.assertIn('adapter" OFF)', cmake)
        self.assertIn("'thirdparty/dbce-shim'", (ROOT / "tools/package-launcher.ps1").read_text())


if __name__ == "__main__":
    unittest.main()
