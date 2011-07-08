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

Usage
-----

You can install the launcher using

msiexec /i launcher.msi

for a 32-bit system. If all goes well, the launcher will be installed, and you
should see a message box indicating that installation was successful.

Installing the launcher will change the file associations for Python files 
(ones with extensions of .py, .pyw, .pyc and .pyo). These will be handled by
the installed launcher. If you previously had associations for these files,
they should be restored when the launcher is uninstalled.

You can uninstall using

msiexec /i launcher.msi

On successful uninstallations, any file associations for Python files should
be restored to what they were before you installed the launcher. If there were
no previous file associations, the launcher will display a dialog showing all
installed Pythons and offer you the option of associating Python files with one
of them. You can choose to associate with a Python of your choice, or avoid
associating these files altogether.

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