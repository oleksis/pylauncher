#!python3

# this test requires:
#  - python2.x 32bit
#  - python2.x 64bit
#  - python3.x 32bit
#  - python3.x 64bit
# to be installed

import sys
if sys.version_info[0] < 3:
    raise ImportError("These tests require Python 3 to run.")

import ctypes
import logging
import os
import os.path
import shutil
import subprocess
import tempfile
import unittest
import winreg

logger = logging.getLogger()

SCRIPT_TEMPLATE='''%(shebang_line)s%(coding_line)simport sys
print(sys.version)
print(sys.argv)
%(comment)s'''

BOM_UTF8 = b'\xEF\xBB\xBF'

LAUNCHER = os.path.join('Debug', 'py.exe')

IS_W = sys.executable.endswith("w.exe")

SUPPORT_VENV_IN_SHEBANG = False

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
    '/usr/bin/env ',
    '/usr/bin/env  ', # test extra whitespace before command
    '/usr/bin/',
    '/usr/local/bin/',
    '',
]

class VirtualPath: # think a C struct...
    def __init__(self, version, bits, executable):
        self.version = version
        self.bits = bits
        self.executable = executable

def is_64_bit_os():
    i = ctypes.c_int()
    kernel32 = ctypes.windll.kernel32
    process = kernel32.GetCurrentProcess()
    kernel32.IsWow64Process(process, ctypes.byref(i))
    return i.value

def locate_pythons_for_key(root, flags, infos):
    executable = 'pythonw.exe' if IS_W else 'python.exe'
    python_path = r'SOFTWARE\Python\PythonCore'
    try:
        core_root = winreg.OpenKeyEx(root, python_path, 0, flags)
    except WindowsError:
        return
    try:
        i = 0
        while True:
            try:
                verspec = winreg.EnumKey(core_root, i)
            except WindowsError:
                break
            try:
                ip_path = python_path + '\\' + verspec + '\\' + 'InstallPath'
                key_installed_path = winreg.OpenKeyEx(root, ip_path, 0, flags)
                try:
                    install_path, typ = winreg.QueryValueEx(key_installed_path,
                                                            None)
                finally:
                    winreg.CloseKey(key_installed_path)
                if typ==winreg.REG_SZ:
                    for check in ['', 'pcbuild', 'pcbuild/amd64']:
                        maybe = os.path.join(install_path, check, executable)
                        if os.path.isfile(maybe):
                            if ' ' in maybe:
                                maybe = '"' + maybe + '"'
                            infos.append(VirtualPath(verspec, 32, maybe))
                            #debug("found version %s at '%s'" % (verspec, maybe))
                            break
            except WindowsError:
                pass
            i += 1
    finally:
        winreg.CloseKey(core_root)

# Locate all installed Python versions, reverse-sorted by their version
# number - the sorting allows a simplistic linear scan to find the higest
# matching version number.
def locate_all_pythons():
    infos = []
    
    if not is_64_bit_os():
        locate_pythons_for_key(winreg.HKEY_CURRENT_USER, winreg.KEY_READ,
                               infos)
        locate_pythons_for_key(winreg.HKEY_LOCAL_MACHINE, winreg.KEY_READ,
                               infos)
    else:
        locate_pythons_for_key(winreg.HKEY_CURRENT_USER,
                               winreg.KEY_READ | winreg.KEY_WOW64_64KEY,
                               infos)
        locate_pythons_for_key(winreg.HKEY_LOCAL_MACHINE,
                               winreg.KEY_READ | winreg.KEY_WOW64_64KEY,
                               infos)
        locate_pythons_for_key(winreg.HKEY_CURRENT_USER,
                               winreg.KEY_READ | winreg.KEY_WOW64_32KEY,
                               infos)
        locate_pythons_for_key(winreg.HKEY_LOCAL_MACHINE,
                               winreg.KEY_READ | winreg.KEY_WOW64_32KEY,
                               infos)

    return sorted(infos, reverse=True, key=lambda info: (info.version, -info.bits))


# These are defined later in the main clause.
#ALL_PYTHONS = DEFAULT_PYTHON2 = DEFAULT_PYTHON3 = None


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
        spec = os.environ.get('PY_DEFAULT_PYTHON'+spec, spec)
    if spec:
        ret = locate_python_ver(spec)
    else:
        # No python spec - see if the environment has a default.
        spec = os.environ.get('PY_DEFAULT_PYTHON')
        if spec:
            ret = locate_python_ver(spec)
        else:
            # hrmph - still no spec - prefer python 2 if installed.
            ret = locate_python_ver('2')
            if ret is None:
                ret = locate_python_ver('3')
    # may still be none, but we are out of search options.
    logger.debug('located python: %s -> %s', spec, (ret.version, ret.bits, ret.executable))
    return ret


def update_for_installed_pythons(*pythons):
    for python in pythons:
        python.bversion = python.version.encode('ascii')
        python.dir = 'Python%s' % python.version.replace('.', '')
        python.bdir = python.dir.encode('ascii')
        python.output_version = b'Python ' + python.bversion
        
        # Add additional shebangs for the versions we know are present
        major = python.version[0]
        upd_templates = {
            'ENV_PY%s_MIN': '#!/usr/bin/env python%s\n',
            'ENV_PY%s_MIN_BITS': '#!/usr/bin/env python%s-32\n',
            'BIN_PY%s_MIN': '#!/usr/bin/python%s\n',
            'BIN_PY%s_MIN_BITS': '#!/usr/bin/python%s-32\n',
            'LBIN_PY%s_MIN': '#!/usr/local/bin/python%s\n',
            'LBIN_PY%s_MIN_BITS': '#!/usr/local/bin/python%s-32\n',
            'PY%s_MIN': '#!/usr/local/bin/python%s\n',
            'PY%s_MIN_BITS': '#!/usr/local/bin/python%s-32\n',
        }
        for k, v in upd_templates.items():
            key = k % major
            value = v % python.version
            assert key not in SHEBANGS  # sanity check
            SHEBANGS[key] = value

class ScriptMaker:
    def setUp(self):
        self.work_dir = tempfile.mkdtemp()
        logger.debug('Starting test %s', self._testMethodName)

    def tearDown(self):
        logger.debug('Ending test %s', self._testMethodName)
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
        logger.debug('Made script: %r' % script)
        return path

    def save_script(self):
            with open('last_failed.py', 'wb') as f:
                f.write(self.last_script)

    def matches(self, stdout, pyinfo):
        result = stdout.startswith(pyinfo.bversion)
        if not result:
            self.save_script()
            logger.debug('Match failed - expected: %s', pyinfo.bversion)
            for i, s in enumerate(self.last_streams):
                stream = 'stderr' if i else 'stdout'
                logger.debug('%s: %r', stream, s)
        return result

    def is_encoding_error(self, message):
        return b'but no encoding declared; see' in message

    def run_child(self, path, env=None):
        if isinstance(path, (list, tuple)):
            cmdline = [LAUNCHER] + list(path)
        else:
            cmdline = [LAUNCHER, path]
        p = subprocess.Popen(cmdline, stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE, shell=False,
                             env=env)
        stdout, stderr = p.communicate()
        self.last_streams = stdout, stderr
        return stdout, stderr

    def get_python_for_shebang(self, shebang):
        if 'python3' in shebang:
            result = DEFAULT_PYTHON3
        elif not shebang:  # changed to return Python 3 in this case
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

    def test_version_specifier(self):
        """Test that files named like a version specifier do not get 
        misinterpreted as a version specifier when it does not have a shebang."""
        for nohyphen in ['t3', 'x2.6', '_3.1-32']:
            with open(nohyphen, 'w') as f:
                f.write('import sys\nprint(sys.version)\nprint(sys.argv)')
            try:
                script = self.make_script(shebang_line='')
                p =  subprocess.Popen([LAUNCHER, nohyphen, script], 
                        stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                stdout, stderr = p.communicate()
                self.last_streams = stdout, stderr
                self.assertTrue(self.matches(stdout, DEFAULT_PYTHON3))
            finally:
                os.remove(nohyphen)
 
    # Tests with ASCII Python sources
    def test_shebang_ascii(self):
        "Test shebangs in ASCII files"
        for shebang in SHEBANGS.values():
            path = self.make_script(shebang_line=shebang)
            stdout, stderr = self.run_child(path)
            python = self.get_python_for_shebang(shebang)
            matches = self.matches(stdout, python)
            self.assertTrue(matches, 'Failed for shebang: %s' % shebang)

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
            self.assertTrue(self.is_encoding_error(stderr) or '3' in shebang or not shebang)
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

    def test_venv(self):
        "Test correct operation in a virtualenv"
        if not os.path.isdir('venv34'):
            raise unittest.SkipTest('a venv is needed for this test')
        # Check interactive operation
        env = os.environ.copy()
        stdout, stderr = self.run_child(['-c', 'import sys; print(sys.version)'], env=env)
        # Not true any more: We favour 3 over 2 if not explicitly specified
        # self.assertTrue(stdout.startswith(b'2.7'))
        env['VIRTUAL_ENV'] = os.path.abspath('venv34')
        stdout, stderr = self.run_child(['-c', 'import sys; print(sys.version)'], env=env)
        self.assertTrue(stdout.startswith(b'3.4'))        
        if SUPPORT_VENV_IN_SHEBANG:
            for key in ('PY', 'ENV_PY'):
                shebang = SHEBANGS[key]
                path = self.make_script(shebang_line=shebang, encoding='utf-8')
                stdout, stderr = self.run_child(path, env=env)
                self.assertEqual(stdout.startswith(b'3.4'), key == 'ENV_PY')
                self.assertEqual(stdout.startswith(b'2.7'), key != 'ENV_PY')

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

        self.local_ini = path = os.path.join(os.environ['LOCALAPPDATA'],
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

LOCAL_INI_IN = '''[commands]
h3  = {p3.executable} --help
v3  = {p3.executable} --version
v2a = {p2.executable} -v

[defaults]
python=3
python3={p3.version}
'''

GLOBAL_INI_IN = '''[commands]
h2  = {p2.executable} -h
h3  = {p3.executable} -h
v2  = {p2.executable} -V
v3  = {p3.executable} -V
v3a = {p3.executable} -v
shell = cmd /c

[defaults]
python=2
python2={p2.version}
'''

class ConfigurationTest(ConfiguredScriptMaker, unittest.TestCase):
    def test_basic(self):
        "Test basic configuration"
        # We're specifying Python 3 in the local ini...
        write_data(self.local_ini, LOCAL_INI)
        write_data(self.global_ini, GLOBAL_INI)
        shebang = SHEBANGS['PY']    # just 'python' ...
        path = self.make_script(shebang_line=shebang)
        stdout, stderr = self.run_child(path)
        self.assertTrue(self.matches(stdout, DEFAULT_PYTHON3))
        # Now zap the local configuration ... should get Python 2
        write_data(self.local_ini, '')
        stdout, stderr = self.run_child(path)
        self.assertTrue(self.matches(stdout, DEFAULT_PYTHON2))

    def test_customised(self):
        "Test customised commands"
        write_data(self.local_ini, LOCAL_INI)
        write_data(self.global_ini, GLOBAL_INI)

        # Python 3 with help
        shebang = '#!h3\n'
        path = self.make_script(shebang_line=shebang)
        stdout, stderr = self.run_child(path)
        self.assertTrue(stdout.startswith(b'usage: '))
        # Assumes standard Python installation directory
        self.assertIn(DEFAULT_PYTHON3.bdir, stdout)

        # Python 2 with help
        shebang = '#!h2\n'
        path = self.make_script(shebang_line=shebang)
        stdout, stderr = self.run_child(path)
        self.assertTrue(stdout.startswith(b'usage: '))
        # Assumes standard Python installation directory
        self.assertIn(DEFAULT_PYTHON2.bdir, stdout)

        # Python 3 version
        for prefix in VIRT_PATHS:
            shebang = '#!%sv3\n' % prefix
            path = self.make_script(shebang_line=shebang)
            stdout, stderr = self.run_child(path)
            self.assertTrue(stdout.startswith(DEFAULT_PYTHON3.output_version) or
                            stderr.startswith(DEFAULT_PYTHON3.output_version))

        # Python 2 version
        for prefix in VIRT_PATHS:
            shebang = '#!%sv2\n' % prefix
            path = self.make_script(shebang_line=shebang)
            stdout, stderr = self.run_child(path)
            self.assertTrue(stderr.startswith(DEFAULT_PYTHON2.output_version))

        VERBOSE_START = b'# installing zipimport hook'
        VERBOSE_START_3 = b'import _frozen_importlib # frozen'

        # Python 3 with -v
        shebang = '#!v3a\n'
        path = self.make_script(shebang_line=shebang)
        stdout, stderr = self.run_child(path)
        self.assertTrue(-1 != stderr.find(VERBOSE_START))
        # Assumes standard Python installation directory
        self.assertIn(DEFAULT_PYTHON3.bdir, stderr)

        # Python 2 with -v
        shebang = '#!v2a\n'
        path = self.make_script(shebang_line=shebang)
        stdout, stderr = self.run_child(path)
        self.assertTrue(stderr.startswith(VERBOSE_START))
        self.assertIn(DEFAULT_PYTHON2.bdir, stderr)

        # Python 2 with -V via cmd.exe /C
        shebang = '#!shell %s -V\n' % DEFAULT_PYTHON2.executable
        path = self.make_script(shebang_line=shebang)
        stdout, stderr = self.run_child(path)
        self.assertTrue(stderr.startswith(DEFAULT_PYTHON2.output_version))
        shebang = '#!shell %s -v\n' % DEFAULT_PYTHON2.executable
        path = self.make_script(shebang_line=shebang)
        stdout, stderr = self.run_child(path)
        self.assertTrue(stdout.startswith(DEFAULT_PYTHON2.bversion))
        self.assertTrue(stderr.startswith(VERBOSE_START))

    def test_environment(self):
        "Test configuration via the environment"
        "Test basic configuration"
        # We're specifying Python 3 in the local ini...
        write_data(self.local_ini, LOCAL_INI)
        write_data(self.global_ini, GLOBAL_INI)
        shebang = SHEBANGS['PY']    # just 'python' ...
        path = self.make_script(shebang_line=shebang)
        stdout, stderr = self.run_child(path)
        self.assertTrue(self.matches(stdout, DEFAULT_PYTHON3))
        # Now, override in the environment
        env = os.environ.copy()
        env['PY_PYTHON'] = '2'
        stdout, stderr = self.run_child(path, env=env)
        self.assertTrue(self.matches(stdout, DEFAULT_PYTHON2))
        # And again without the environment change
        stdout, stderr = self.run_child(path)
        self.assertTrue(self.matches(stdout, DEFAULT_PYTHON3))


if __name__ == '__main__':
    logging.basicConfig(filename='tests.log', filemode='w', level=logging.DEBUG,
                        format='%(lineno)4d %(message)s')
    global DEFAULT_PYTHON2, DEFAULT_PYTHON3, ALL_PYTHONS, LOCAL_INI, GLOBAL_INI
    ALL_PYTHONS = locate_all_pythons()
    DEFAULT_PYTHON2 = locate_python('2')
    assert DEFAULT_PYTHON2, "You don't appear to have Python 2 installed"

    DEFAULT_PYTHON3 = locate_python('3')
    assert DEFAULT_PYTHON3, "You don't appear to have Python 3 installed"

    update_for_installed_pythons(DEFAULT_PYTHON2, DEFAULT_PYTHON3)

    LOCAL_INI = LOCAL_INI_IN.format(p2=DEFAULT_PYTHON2, p3=DEFAULT_PYTHON3)
    GLOBAL_INI = GLOBAL_INI_IN.format(p2=DEFAULT_PYTHON2, p3=DEFAULT_PYTHON3)

    unittest.main()
