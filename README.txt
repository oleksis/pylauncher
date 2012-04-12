================================================
Installing and Using Python Launcher for Windows
================================================

Binary Builds
-------------

You can get 32- and 64-bit binary installers from the Downloads section.

Source Builds
-------------

You should be able to clone this repository, then build the executables using
Visual Studio or Visual Studio Express (2008 Editions - 2010 Editiona should
also work, but haven't been tested).

Once the executables have been built, you can run buildmsi.cmd to build the
installers (you'll need the WiX toolkit installed and on your path in order to
do this).

Installation and Uninstallation
-------------------------------

You can install the launcher using

msiexec /i launcher.msi

for a 32-bit system. If all goes well, the launcher will be installed, and you
should see a message box indicating that installation was successful.

Installing the launcher will change the file associations for Python files 
(ones with extensions of .py, .pyw, .pyc and .pyo). These will be handled by
the installed launcher. If you previously had associations for these files,
they should be restored when the launcher is uninstalled.

You can uninstall using

msiexec /x launcher.msi

On successful uninstallations, any file associations for Python files should
be restored to what they were before you installed the launcher. If there were
no previous file associations, the launcher will display a dialog showing all
installed Pythons and offer you the option of associating Python files with one
of them. You can choose to associate with a Python of your choice, or avoid
associating these files altogether.

Variants of the launcher
------------------------

There are four variants of the launcher MSI which you can download:

launcher.msi        32-bit launcher, installs to \Program Files\Python Launcher
launcher.amd64.msi  64-bit launcher, installs to \Program Files\Python Launcher
launchwin.msi       32-bit launcher, installs to \Windows
launchwin.amd64.msi 64-bit launcher, installs to \Windows

One advantage of the versions which install to the Windows folder is
that you can invoke the launcher itself more easily, using just "py" or "pyw",
as the Windows folder is on the PATH for both 32-bit and 64-bit executables.

User Permissions
----------------

There are a couple of points worth noting about how the installers work:

1. If you don't have administrator rights on the machine you're installing on,
   you probably won't be able to complete the installation, because you don't
   have permissions to write to the "Program Files" folder.

2. By default, all installations run using MSIEXEC will attempt to perform an
   installation for the current user: this means registry changes will be
   made under HKEY_CURRENT_USER rather than HKEY_LOCAL_MACHINE. To install
   for all users, use a command line of the form

   msiexec /i launcher.msi ALLUSERS=1

   This will attempt an all-users installation, which will fail if you don't
   have administrator access. As an alternative, you can do

   msiexec /i launcher.msi ALLUSERS=2

   which will do an all-users installation (if you have administrator access)
   or a per-user installation (if you don't).

Usage
-----

See the PEP for how the launcher is supposed to work. In practice, once the
launcher is installed, you can run any Python script on your system even if
Python is not on the path, simply invoking the script by name (if you have
.py and .pyw in your PATHEXT environment variable, you don't even need to
specify the extension):

C:\Users\Vinay> script arg1 arg2 arg3

If you have more than one Python installed, the first line of the script can
indicate a specific Python to run it with:

#!python    -> the default Python (usually the most recent Python 2.x)
#!python2   -> the most recent Python 2.x
#!python3   -> the most recent Python 3.x
#!python3.2 -> Python 3.2 (an error will be raised if that's not installed)

Basic Documentation
-------------------

Basic documentation in ReStructuredText format can be found at

https://bitbucket.org/vinay.sajip/pylauncher/raw/tip/Doc/launcher.rst

Software Status
---------------

The software is currently in beta status - while it is reasonably stable, you
may experience problems on installation or uninstallation. If for any reason
you cannot uninstall the launcher using msiexec, you can use the msizap utility
from Microsoft for this (it's part of the Windows SDK - free to download).

If you need to use msizap, these are the Windows Installer product codes for
the launcher MSIs:

{298B5D62-1287-427F-B8D9-B44D605F8F6B} (32-bit)
{1CB6C42B-5887-47CF-AF21-988256F0455B} (64-bit)

If you find any problems, please file a ticket in the Issues section.
