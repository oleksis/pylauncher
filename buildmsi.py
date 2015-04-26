import getpass
import os
import sys

VER = '1.0.1.6'
VERSION = 'Version=%s' % VER
MANUFACTURER = 'Manufacturer=Vinay Sajip'
X86 = 'Platform=x86'
X64 = 'Platform=x64'
TOWIN = 'ToWindows'

def main():
    signpwd = getpass.getpass('Password for signing:')
    import builddoc
    builddoc.main()
    os.environ['SIGNPWD'] = signpwd
    import makemsi
    makemsi.main(['-o', 'launchwin-%s' % VER, X86, VERSION, MANUFACTURER, TOWIN, 'launcher'])
    makemsi.main(['-o', 'launcher-%s' % VER, X86, VERSION, MANUFACTURER, 'launcher'])
    makemsi.main(['-o', 'launchwin-%s' % VER, X64, VERSION, MANUFACTURER, TOWIN, 'launcher'])
    makemsi.main(['-o', 'launcher-%s' % VER, X64, VERSION, MANUFACTURER, 'launcher'])

if __name__ == '__main__':
    sys.exit(main())