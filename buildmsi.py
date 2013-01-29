import getpass
import os
import sys

VERSION = 'Version=1.0.1.0'
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
    makemsi.main(['-o', 'launchwin', X86, VERSION, MANUFACTURER, TOWIN, 'launcher'])
    makemsi.main([X86, VERSION, MANUFACTURER, 'launcher'])
    makemsi.main(['-o', 'launchwin', X64, VERSION, MANUFACTURER, TOWIN, 'launcher'])
    makemsi.main([X64, VERSION, MANUFACTURER, 'launcher'])

if __name__ == '__main__':
    sys.exit(main())