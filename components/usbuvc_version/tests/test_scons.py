"""Run the real SCons object builder twice, including an incremental rebuild."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

COMPONENT = Path(__file__).resolve().parents[1]


class SConsBuildTests(unittest.TestCase):
    def test_object_output_never_uses_source_suffix(self):
        scons = shutil.which("scons")
        if scons is None:
            self.skipTest("Install SCons 4.11.1 to run the real builder regression")
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            component = root / "component"
            source = component / "_vendor/esp-dl/dl/tensor/src/probe.cpp"
            source.parent.mkdir(parents=True)
            original = b"int probe() { return 42; }\n"
            source.write_bytes(original)
            shutil.copyfile(COMPONENT / "usbuvc_version_build.py", component / "build.py")
            (root / "SConstruct").write_text('''
env = Environment(BUILD_DIR=Dir('out').abspath, CPPDEFINES=['USBUVC_VERSION_MODEL_FROM_FILE'], _LIBFLAGS='')
SConscript('component/build.py', exports={'env': env})
Default(env['LIBS'])
''')
            for _ in range(2):
                result = subprocess.run([scons, "-Q"], cwd=root, text=True, errors="replace", capture_output=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertEqual(source.read_bytes(), original)
                # The output must not masquerade as C++ source, including
                # on SCons versions which honor a supplied target suffix.
                self.assertFalse(list((root / 'out').rglob('*.cpp')))
                objects = list((root / 'out').rglob('*.o'))
                self.assertEqual(len(objects), 1)
                self.assertEqual(objects[0].read_bytes()[:4], b'\x7fELF')
