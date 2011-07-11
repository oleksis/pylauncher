#!python
import argparse
import os
import subprocess
import sys

def invoke(command):
    print(' '.join(command))
    p = subprocess.Popen(command,
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
    parser.add_argument('options', nargs='*', metavar='OPTION',
                        help='An option in the form NAME or NAME=VALUE')
    parser.add_argument('wxsname', metavar='WXSNAME',
                        help='The base name of the WiX source file')
    cmdline = parser.parse_args(args)
    wxsname = cmdline.wxsname
    wxsfn = '%s.wxs' % wxsname
    objfn = '%s.wixobj' % wxsname
    opts = cmdline.options
    plats = [opt for opt in opts if opt.startswith('Platform=')]
    if not plats:
        opts.append('Platform=x86')
    if 'Platform=x64' in plats:
        msifn = '%s.amd64.msi' % wxsname
        pdbfn = '%s.amd64.wixpdb' % wxsname
    else:
        msifn = '%s.msi' % wxsname
        pdbfn = '%s.wixpdb' % wxsname
    if opts:
        opts = [ '-d%s' % opt for opt in opts]
    invoke(['candle'] + opts + [wxsfn])
    invoke(['light', '-o', msifn, objfn])
    os.remove(objfn)
    os.remove(pdbfn)
    return 0

if __name__ == '__main__':
    sys.exit(main())
