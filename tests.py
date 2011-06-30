import os
import shutil
import subprocess
import sys
import tempfile
import unittest
import winreg

SCRIPT_TEMPLATE='''%(shebang_line)s%(coding_line)simport sys
print(sys.version)
print(sys.argv)
'''

BOM_UTF8 = b'\xEF\xBB\xBF'
BOM_UTF16LE = b'\xFF\xFE'
BOM_UTF16BE = b'\xFE\xFF'
LAUNCHER = os.path.join('Release', 'py.exe')

IS_W = sys.executable.endswith("w.exe")

VIRT_PATHS = [
    "/usr/bin/env ",
    "/usr/bin/",
]

class VirtualPath: # think a C struct...
    def __init__(self, version, bits, executable):
        self.version = version
        self.bits = bits
        self.executable = executable

# Locate all installed Python versions, reverse-sorted by their version
# number - the sorting allows a simplistic linear scan to find the higest
# matching version number.
def locate_all_pythons():
    infos = []
    executable = "pythonw.exe" if IS_W else "python.exe"
    python_path = r"SOFTWARE\Python\PythonCore"
    for root in (winreg.HKEY_LOCAL_MACHINE, winreg.HKEY_CURRENT_USER):
        core_root = winreg.OpenKey(root, python_path)
        try:
            i = 0
            while True:
                try:
                    verspec = winreg.EnumKey(core_root, i)
                except WindowsError:
                    break
                try:
                    ip_path = python_path + "\\" + verspec + "\\" + "InstallPath"
                    key_installed_path = winreg.OpenKey(root, ip_path)
                    try:
                        install_path, typ = winreg.QueryValueEx(key_installed_path,
                                                                None)
                    finally:
                        winreg.CloseKey(key_installed_path)
                    if typ==winreg.REG_SZ:
                        for check in ["", "pcbuild", "pcbuild/amd64"]:
                            maybe = os.path.join(install_path, check, executable)
                            if os.path.isfile(maybe):
                                if " " in maybe:
                                    maybe = '"' + maybe + '"'
                                infos.append(VirtualPath(verspec, 32, maybe))
                                #debug("found version %s at '%s'" % (verspec, maybe))
                                break
                except WindowsError:
                    pass
                i += 1
        finally:
            winreg.CloseKey(core_root)
    return sorted(infos, reverse=True, key=lambda info: info.version)


ALL_PYTHONS = locate_all_pythons()

# locate a specific python version - some version must be specified (although
# it may be just a major version)
def locate_python_ver(spec):
    assert spec
    for info in ALL_PYTHONS:
        if info.version.startswith(spec):
            return info
    return None


def locate_python(spec):
    if len(spec)==1:
        # just a major version was specified - see if the environment
        # has a default for that version.
        spec = os.environ.get("PY_DEFAULT_PYTHON"+spec, spec)
    if spec:
        return locate_python_ver(spec)
    # No python spec - see if the environment has a default.
    spec = os.environ.get("PY_DEFAULT_PYTHON")
    if spec:
        return locate_python_ver(spec)
    # hrmph - still no spec - prefer python 2 if installed.
    ret = locate_python_ver("2")
    if ret is None:
        ret = locate_python_ver("3")
    # may still be none, but we are out of search options.
    return ret

DEFAULT_PYTHON2 = locate_python("2")
if DEFAULT_PYTHON2:
    DEFAULT_PYTHON2.bversion = DEFAULT_PYTHON2.version.encode('ascii')
    
DEFAULT_PYTHON3 = locate_python("3")
if DEFAULT_PYTHON3:
    DEFAULT_PYTHON3.bversion = DEFAULT_PYTHON3.version.encode('ascii')

class ScriptLaunchTest(unittest.TestCase):
    def setUp(self):
        self.work_dir = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.work_dir)

    def make_script(self, shebang_line='', coding_line='', encoding='ascii',
                    bom=b''):
        script = (SCRIPT_TEMPLATE % locals()).encode(encoding)
        script = script.replace(b'\r', b'').replace(b'\n', b'\r\n')
        path = os.path.join(self.work_dir, 'showver.py')
        with open(path, 'wb') as f:
            f.write(bom + script)
        return path

    def matches(self, stdout, pyinfo):
        return stdout.startswith(pyinfo.bversion)

    def run_child(self, path):
        p = subprocess.Popen([LAUNCHER, path], stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE)
        stdout, stderr = p.communicate()
        return stdout, stderr

    def test_help(self):
        p = subprocess.Popen([LAUNCHER, '-h'], stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE)
        stdout, stderr = p.communicate()
        self.assertTrue(stdout.startswith(b'Python Launcher for Windows'))
        self.assertIn(b'The following help text is from Python:\r\n\r\nusage: ', stdout)

    def test_no_shebang_ascii(self):
        path = self.make_script()
        stdout, stderr = self.run_child(path)
        self.assertTrue(self.matches(stdout, DEFAULT_PYTHON2))

    def test_py_shebang_ascii(self):
        path = self.make_script(shebang_line='#!/usr/bin/env python\n')
        stdout, stderr = self.run_child(path)
        self.assertTrue(self.matches(stdout, DEFAULT_PYTHON2))

    def test_py2_shebang_ascii(self):
        path = self.make_script(shebang_line='#!/usr/bin/env python2\n')
        stdout, stderr = self.run_child(path)
        self.assertTrue(self.matches(stdout, DEFAULT_PYTHON2))

    def test_py3_shebang_ascii(self):
        path = self.make_script(shebang_line='#!/usr/bin/env python3\n')
        stdout, stderr = self.run_child(path)
        self.assertTrue(self.matches(stdout, DEFAULT_PYTHON3))

    def test_no_shebang_utf8_nobom(self):
        path = self.make_script(encoding='utf-8')
        stdout, stderr = self.run_child(path)
        self.assertTrue(self.matches(stdout, DEFAULT_PYTHON2))

    def test_py_shebang_utf8_nobom(self):
        path = self.make_script(shebang_line='#!/usr/bin/env python\n',
                                encoding='utf-8')
        stdout, stderr = self.run_child(path)
        self.assertTrue(self.matches(stdout, DEFAULT_PYTHON2))

    def test_py2_shebang_utf8_nobom(self):
        path = self.make_script(shebang_line='#!/usr/bin/env python2\n',
                                encoding='utf-8')
        stdout, stderr = self.run_child(path)
        self.assertTrue(self.matches(stdout, DEFAULT_PYTHON2))

    def test_py3_shebang_utf8_nobom(self):
        path = self.make_script(shebang_line='#!/usr/bin/env python3\n',
                                encoding='utf-8')
        stdout, stderr = self.run_child(path)
        self.assertTrue(self.matches(stdout, DEFAULT_PYTHON3))

    def test_no_shebang_utf8_bom(self):
        path = self.make_script(encoding='utf-8', bom=BOM_UTF8)
        stdout, stderr = self.run_child(path)
        self.assertTrue(self.matches(stdout, DEFAULT_PYTHON2))

    def test_py_shebang_utf8_bom(self):
        path = self.make_script(shebang_line='#!/usr/bin/env python\n',
                                encoding='utf-8', bom=BOM_UTF8)
        stdout, stderr = self.run_child(path)
        self.assertTrue(self.matches(stdout, DEFAULT_PYTHON2))

    def test_py2_shebang_utf8_bom(self):
        path = self.make_script(shebang_line='#!/usr/bin/env python2\n',
                                encoding='utf-8', bom=BOM_UTF8)
        stdout, stderr = self.run_child(path)
        self.assertTrue(self.matches(stdout, DEFAULT_PYTHON2))

    def test_py3_shebang_utf8_bom(self):
        path = self.make_script(shebang_line='#!/usr/bin/env python3\n',
                                encoding='utf-8', bom=BOM_UTF8)
        stdout, stderr = self.run_child(path)
        self.assertTrue(self.matches(stdout, DEFAULT_PYTHON3))

    def test_no_shebang_utf8_bom_coding(self):
        path = self.make_script(encoding='utf-8', bom=BOM_UTF8,
                                coding_line='# -*- coding: utf-8 -*-\n')
        stdout, stderr = self.run_child(path)
        self.assertTrue(self.matches(stdout, DEFAULT_PYTHON2))

    def test_py_shebang_utf8_bom_coding(self):
        path = self.make_script(shebang_line='#!/usr/bin/env python\n',
                                coding_line='# -*- coding: utf-8 -*-\n',
                                encoding='utf-8', bom=BOM_UTF8)
        stdout, stderr = self.run_child(path)
        self.assertTrue(self.matches(stdout, DEFAULT_PYTHON2))

    def test_py2_shebang_utf8_bom_coding(self):
        path = self.make_script(shebang_line='#!/usr/bin/env python2\n',
                                coding_line='# -*- coding: utf-8 -*-\n',
                                encoding='utf-8', bom=BOM_UTF8)
        stdout, stderr = self.run_child(path)
        self.assertTrue(self.matches(stdout, DEFAULT_PYTHON2))

    def test_py3_shebang_utf8_bom_coding(self):
        path = self.make_script(shebang_line='#!/usr/bin/env python3\n',
                                coding_line='# -*- coding: utf-8 -*-\n',
                                encoding='utf-8', bom=BOM_UTF8)
        stdout, stderr = self.run_child(path)
        self.assertTrue(self.matches(stdout, DEFAULT_PYTHON3))

        
if __name__ == '__main__':
    unittest.main()

