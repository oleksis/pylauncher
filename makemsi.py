#!python
import argparse
import os
import subprocess
import sys

def invoke(command):
    _command = ' '.join(command)
    print(_command)
    p = subprocess.Popen(_command,
                         stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE,
                         shell=True)
    stdout, stderr = p.communicate()
    if p.returncode:
        print('%s failed:' % command[0])
        if stderr:
            print(stderr.decode('utf-8'))
        if stdout:
            print(stdout.decode('utf-8'))
        sys.exit(p.returncode)

def main(args=None):
    parser = argparse.ArgumentParser()
    parser.add_argument('-o', dest='output')
    parser.add_argument('-x', dest='extensions', action='store_true',
                        help='Use UI, Util extensions')
    parser.add_argument('options', nargs='*', metavar='OPTION',
                        help='An option in the form NAME or NAME=VALUE')
    parser.add_argument('wxsname', metavar='WXSNAME',
                        help='The base name of the WiX source file')
    cmdline = parser.parse_args(args)
    wxsname = cmdline.wxsname
    if cmdline.output:
        msiname = cmdline.output
    else:
        msiname = wxsname
    wxsfn = '%s.wxs' % wxsname
    objfn = '%s.wixobj' % wxsname
    opts = cmdline.options
    plats = [opt for opt in opts if opt.startswith('Platform=')]

    if not plats:
        opts.append('Platform=x86')

    if 'Platform=x64' in plats:
        msifn = '%s.amd64.msi' % msiname
        pdbfn = '%s.amd64.wixpdb' % msiname
    else:
        msifn = '%s.msi' % msiname      
        pdbfn = '%s.wixpdb' % msiname

    if opts:
        opts = [ '-d %s' % opt for opt in opts]
    # We use WiX v4
    # invoke(['candle'] + opts + [wxsfn])
    # light = ['light']
    # if cmdline.extensions:
    #     light.extend(['-ext', 'WixUIExtension', '-ext', 'WixUtilExtension',
    #                   '-cultures:en-us'])
    # light.extend(['-o', msifn, objfn])
    # invoke(light)
    # os.remove(objfn)
    invoke(['wix', 'build', '-o', msifn] + opts + [wxsfn])
    os.remove(pdbfn)
    pwd = os.environ.get('SIGNPWD', '').strip()
    if pwd:
        invoke(['sign', '/d', 'Python Launcher Installer', msifn])
    return 0

if __name__ == '__main__':
    sys.exit(main())
