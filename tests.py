import sys
if sys.version_info[0] < 3:
    raise ImportError("These tests require Python 3 to run.")

import os
import shutil
import subprocess
import tempfile
import unittest
import winreg


SCRIPT_TEMPLATE='''%(shebang_line)s%(coding_line)simport sys
print(sys.version)
print(sys.argv)
%(comment)s'''

BOM_UTF8 = b'\xEF\xBB\xBF'

LAUNCHER = os.path.join('Debug', 'py.exe')

IS_W = sys.executable.endswith("w.exe")

SHEBANGS = {
    'NONE': '',
    'ENV_PY': '#!/usr/bin/env python\n',
    'ENV_PY2': '#!/usr/bin/env python2\n',
    'ENV_PY3': '#!/usr/bin/env python3\n',
    'BIN_PY': '#!/usr/bin/python\n',
    'BIN_PY2': '#!/usr/bin/python2\n',
    'BIN_PY3': '#!/usr/bin/python3\n',
    'LBIN_PY': '#!/usr/local/bin/python\n',
    'LBIN_PY2': '#!/usr/local/bin/python2\n',
    'LBIN_PY3': '#!/usr/local/bin/python3\n',
    'PY': '#!python\n',
    'PY2': '#!python2\n',
    'PY3': '#!python3\n',
}

COMMENT_WITH_UNICODE = '# Libert\xe9, \xe9galit\xe9, fraternit\xe9\n'

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

class ScriptMaker:
    def setUp(self):
        self.work_dir = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.work_dir)

    def make_script(self, shebang_line='', coding_line='', encoding='ascii',
                    bom=b'', comment=''):
        script = (SCRIPT_TEMPLATE % locals())
        script = script.replace('\r', '').replace('\n',
                                                  '\r\n').encode(encoding)
        if bom and not script.startswith(bom):
            script = bom + script
        path = os.path.join(self.work_dir, 'showver.py')
        with open(path, 'wb') as f:
            f.write(script)
        self.last_script = script
        return path

    def matches(self, stdout, pyinfo):
        result = stdout.startswith(pyinfo.bversion)
        if not result:
            with open('last_failed.py', 'wb') as f:
                f.write(self.last_script)
            print(pyinfo.bversion)
            for s in self.last_streams:
                print(repr(s))
        return result

    def is_encoding_error(self, message):
        return b'but no encoding declared; see' in message

    def run_child(self, path):
        p = subprocess.Popen([LAUNCHER, path], stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE, shell=False)
        stdout, stderr = p.communicate()
        self.last_streams = stdout, stderr
        return stdout, stderr

    def get_python_for_shebang(self, shebang):
        if '3' in shebang:
            result = DEFAULT_PYTHON3
        else:
            result = DEFAULT_PYTHON2
        return result

    def get_coding_line(self, coding):
        return '# -*- coding: %s -*-\n' % coding

class BasicTest(ScriptMaker, unittest.TestCase):
    def test_help(self):
        "Test help invocation"
        p = subprocess.Popen([LAUNCHER, '-h'], stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE)
        stdout, stderr = p.communicate()
        self.assertTrue(stdout.startswith(b'Python Launcher for Windows'))
        self.assertIn(b'The following help text is from Python:\r\n\r\nusage: ', stdout)

    # Tests with ASCII Python sources
    def test_shebang_ascii(self):
        "Test shebangs in ASCII files"
        for shebang in SHEBANGS.values():
            path = self.make_script(shebang_line=shebang)
            stdout, stderr = self.run_child(path)
            python = self.get_python_for_shebang(shebang)
            self.assertTrue(self.matches(stdout, python))

    # Tests with UTF-8 Python sources with no BOM
    def test_shebang_utf8_nobom(self):
        "Test shebangs in UTF-8 files with no BOM"
        for shebang in SHEBANGS.values():
            # If there's no Unicode, all should be well
            path = self.make_script(shebang_line=shebang, encoding='utf-8')
            stdout, stderr = self.run_child(path)
            python = self.get_python_for_shebang(shebang)
            self.assertTrue(self.matches(stdout, python))
            # If there's a Unicode comment with no coding line to alert,
            # we should see those errors from the spawned Python
            path = self.make_script(shebang_line=shebang, encoding='utf-8',
                                    comment=COMMENT_WITH_UNICODE)
            stdout, stderr = self.run_child(path)
            # Python3 reads Unicode without BOM as UTF-8
            self.assertTrue(self.is_encoding_error(stderr) or '3' in shebang)
            path = self.make_script(shebang_line=shebang, encoding='utf-8',
                                    comment=COMMENT_WITH_UNICODE,
                                    coding_line=self.get_coding_line('utf-8'))
            stdout, stderr = self.run_child(path)
            self.assertTrue(self.matches(stdout, python))

    # Tests with UTF-8 Python sources with BOM
    def test_shebang_utf8_bom(self):
        "Test shebangs in UTF-8 files with BOM"
        for shebang in SHEBANGS.values():
            # If there's no Unicode, all should be well
            path = self.make_script(shebang_line=shebang, encoding='utf-8',
                                    bom=BOM_UTF8)
            stdout, stderr = self.run_child(path)
            python = self.get_python_for_shebang(shebang)
            self.assertTrue(self.matches(stdout, python))
            # If there's a Unicode comment, we should still be fine as
            # there's a BOM
            path = self.make_script(shebang_line=shebang, encoding='utf-8',
                                    comment=COMMENT_WITH_UNICODE,
                                    bom=BOM_UTF8)
            stdout, stderr = self.run_child(path)
            python = self.get_python_for_shebang(shebang)
            self.assertTrue(self.matches(stdout, python))

def read_data(path):
    if not os.path.exists(path):
        result = None
    else:
        with open(path, 'r') as f:
            result = f.read()
    return result

def write_data(path, value):
    with open(path, 'w') as f:
        f.write(value)

class ConfiguredScriptMaker(ScriptMaker):
    def setUp(self):
        ScriptMaker.setUp(self)

        self.local_ini = path = os.path.join(os.environ['APPDATA'],
                                                  'py.ini')
        self.local_config = read_data(path)
        self.global_ini = path = os.path.join(os.path.dirname(LAUNCHER),
                                              'py.ini')
        self.global_config = read_data(path)

    def tearDown(self):
        if self.local_config is not None:
            write_data(self.local_ini, self.local_config)
        if self.global_config is not None:
            write_data(self.global_ini, self.global_config)
        ScriptMaker.tearDown(self)

LOCAL_INI = '''[commands]
h3 = c:\Python32\python --help
v3 =c:\Python32\python --version

[defaults]
python=3
python3=3.2
'''

GLOBAL_INI = '''[commands]
h2=c:\Python26\python -h
h3 = c:\Python32\python -h
v2= c:\Python26\python -V
v3 =c:\Python32\python -V

[defaults]
python=2
python2=2.7
'''

class ConfigurationTest(ConfiguredScriptMaker, unittest.TestCase):
    def test_basic(self):
        "Test basic configuration"
        # We're specifying Python 3 in the local ini...
        write_data(self.local_ini, LOCAL_INI)
        shebang = SHEBANGS['PY']
        path = self.make_script(shebang_line=shebang)
        stdout, stderr = self.run_child(path)
        python = self.get_python_for_shebang(shebang)
        self.assertTrue(self.matches(stdout, DEFAULT_PYTHON3))
        # Now zap the local configuration
        write_data(self.local_ini, '')
        stdout, stderr = self.run_child(path)
        python = self.get_python_for_shebang(shebang)
        self.assertTrue(self.matches(stdout, DEFAULT_PYTHON2))

if __name__ == '__main__':
    unittest.main()

