"""Exercise the imported SCons script without an ESP32 toolchain.

Run: python -m unittest discover -s components/usbuvc_version/tests -v
"""
import contextlib
import io
from pathlib import Path
import runpy
import tempfile
import unittest

COMPONENT = Path(__file__).resolve().parents[1]


class BuildEnvironment(dict):
    def __init__(self, build_dir, defines):
        super().__init__(CPPDEFINES=defines, BUILD_FLAGS=[], _LIBFLAGS="")
        self.build_dir = str(build_dir)
        self.sources = []
        self.targets = []

    def Append(self, **kwargs):
        for key, value in kwargs.items():
            self.setdefault(key, []).extend(value)

    def Prepend(self, **kwargs):
        for key, value in kwargs.items():
            self[key] = value + self.get(key, [])

    def subst(self, text):
        return text.replace("$BUILD_DIR", self.build_dir)

    def Object(self, target, source):
        if not Path(source).is_file():
            raise FileNotFoundError(source)
        self.sources.append(Path(source))
        self.targets.append(Path(self.subst(target)))
        return [target]

    def StaticLibrary(self, target, objects):
        return target


class BuildTests(unittest.TestCase):
    def run_script(self, build_dir, defines):
        env = BuildEnvironment(build_dir, defines)
        with contextlib.redirect_stdout(io.StringIO()):
            runpy.run_path(str(COMPONENT / "usbuvc_version_build.py"),
                           init_globals={"env": env, "Import": lambda _: None})
        return env

    def test_all_default_model_families(self):
        for family in range(7):
            with self.subTest(family=family), tempfile.TemporaryDirectory() as temp:
                env = self.run_script(temp, [("USBUVC_VERSION_FAMILY", family)])
                model = Path(temp) / "usbuvc_version_model_embed.c"
                self.assertIn("_binary_usbuvc_version_detect_espdl_start", model.read_text())
                self.assertIn(model, env.sources)
                self.assertGreater(len(env.sources), 100)
                if family in (5, 6):
                    self.assertIn(str(COMPONENT / "_vendor/esp-dl/vision/classification"), env["CPPPATH"])
                self.assertIn(str(COMPONENT / "_vendor/esp-dl"), str(env["LIBPATH"]))
                self.assertTrue(all(t.is_relative_to(temp) for t in env.targets))
                self.assertEqual(len(env.targets), len(set(env.targets)))
                self.assertTrue(any("isa/tie728" in str(s) for s in env.sources))
                for lib_path in env["LIBPATH"]:
                    self.assertTrue((Path(lib_path) / "libfbs_model.a").is_file())

    def test_explicit_model_overrides_family_default(self):
        with tempfile.TemporaryDirectory() as temp:
            model = Path(temp) / "custom.espdl"
            model.write_bytes(b"\x01\x02\x03")
            self.run_script(temp, [("USBUVC_VERSION_USER_MODEL_PATH", str(model))])
            embedded = (Path(temp) / "usbuvc_version_model_embed.c").read_text()
            self.assertIn("custom.espdl", embedded)
            self.assertIn("_binary_usbuvc_version_detect_espdl_size = 3;", embedded)

    def test_runtime_model_skips_embedding(self):
        with tempfile.TemporaryDirectory() as temp:
            self.run_script(temp, [("USBUVC_VERSION_FAMILY", 7),
                                  "USBUVC_VERSION_MODEL_FROM_FILE"])
            self.assertFalse((Path(temp) / "usbuvc_version_model_embed.c").exists())


if __name__ == "__main__":
    unittest.main()
