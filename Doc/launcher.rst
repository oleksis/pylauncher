===========================
Python Launcher for Windows
===========================

The Python launcher for Windows is a utility which aids in the location and
execution of different Python versions.  It allows scripts (or the
command-line) to indicate a preference for a specific Python version, and
will locate and execute that version.

.. contents::

---------------
Getting started
---------------

From the command-line
---------------------

You should ensure the launcher is on your PATH - depending on how it was
installed it may already be there, but check just in case it is not.

From a command-prompt, execute the following command:

::

  py

You should find that the latest version of Python 2.x you have installed is
started - it can be exited as normal, and any additional command-line
arguments specified will be sent directly to Python.

If you have multiple versions of Python 2.x installed (e.g., 2.6 and 2.7) you
will have noticed that Python 2.7 was started - to launch Python 2.6, try the
command:

::

  py -2.6

If you have a Python 3.x installed, try the command:

::

  py -3

You should find the latest version of Python 3.x starts.

From a script
-------------

Let's create a test Python script - create a file called ``hello.py`` with the
following contents

::

    #! python
    import sys
    sys.stdout.write("hello from Python %s\n" % (sys.version,))

From the directory in which hello.py lives, execute the command:

::

   py hello.py

You should notice the version number of your latest Python 2.x installation
is printed.  Now try changing the first line to be:

::

    #! python3

Re-executing the command should now print the latest Python 3.x information.
As with the above command-line examples, you can specify a more explicit
version qualifier.  Assuming you have Python 2.6 installed, try changing the
first line to ``#! python2.6`` and you should find the 2.6 version
information printed.

From file associations
----------------------

The launcher should have been associated with Python files (i.e. ``.py``,
``.pyw``, ``.pyc``, ``.pyo`` files) when it was installed.  This means that
when you double-click on one of these files from Windows explorer the launcher
will be used, and therefore you can use the same facilities described above to
have the script specify the version which should be used.

The key benefit of this is that a single launcher can support multiple Python
versions at the same time depending on the contents of the first line.

-------------
Shebang Lines
-------------

If the first line of a script file starts with ``#!``, it is known as a
"shebang" line.  Linux and other Unix like operating systems have native
support for such lines and are commonly used on such systems to indicate how
a script should be executed.  This launcher allows the same facilities to be
using with Python scripts on Windows and the examples above demonstrate their
use.

To allow shebang lines in Python scripts to be portable between Unix and
Windows, this launcher supports a number of 'virtual' commands to specify
which interpreter to use.  The supported virtual commands are:

* ``/usr/bin/env python``
* ``/usr/bin/python``
* ``/usr/local/bin/python``
* ``python``

For example, if the first line of your script starts with

::

  #! /usr/bin/python

The default Python will be located and used.  As many Python scripts written
to work on Unix will already have this line, you should find these scripts can
be used by the launcher without modification.  If you are writing a new script
on Windows which you hope will be useful on Unix, you should use one of the
shebang lines starting with ``/usr``.

Arguments in shebang lines
--------------------------

The shebang lines can also specify additional options to be passed to the
Python interpreter.  For example, if you have a shebang line:

::

  #! /usr/bin/python -v

Then Python will be started with the ``-v`` option

-------------
Customization
-------------

Customization via INI files
---------------------------

    Two .ini files will be searched by the launcher - ``py.ini`` in the
    current user's "application data" directory (i.e. the directory returned
    by calling the Windows function SHGetFolderPath with CSIDL_LOCAL_APPDATA)
    and ``py.ini`` in the same directory as the launcher.  The same .ini
    files are used for both the 'console' version of the launcher (i.e.
    py.exe) and for the 'windows' version (i.e. pyw.exe)
    
    Customization specified in the "application directory" will have
    precedence over the one next to the executable, so a user, who may not
    have write access to the .ini file next to the launcher, can override
    commands in that global .ini file)

Customizing default Python versions
-----------------------------------

In some cases, a version qualifier can be included in a command to dictate
which version of Python will be used by the command. A version qualifier
starts with a major version number and can optionally be followed by a period
('.') and a minor version specifier. If the minor qualifier is specified, it
may optionally be followed by "-32" to indicate the 32-bit implementation of
that version be used.

For example, a shebang line of ``#!python`` has no version qualifier, while
``#!python3`` has a version qualifier which specifies only a major version.

If no version qualifiers are found in a command, the environment variable
``PY_PYTHON`` can be set to specify the default version qualifier - the default
value is "2". Note this value could specify just a major version (e.g. "2") or
a major.minor qualifier (e.g. "2.6"), or even major.minor-32.

If no minor version qualifiers are found, the environment variable
``PY_PYTHON{major}`` (where ``{major}`` is the current major version qualifier
as determined above) can be set to specify the full version. If no such option
is found, the launcher will enumerate the installed Python versions and use
the latest minor release found for the major version, which is likely,
although not guaranteed, to be the most recently installed version in that
family.

On 64-bit Windows with both 32-bit and 64-bit implementations of the same
(major.minor) Python version installed, the 64-bit version will always be
preferred. This will be true for both 32-bit and 64-bit implementations of the
launcher - a 32-bit launcher will prefer to execute a 64-bit Python installation
of the specified version if available. This is so the behavior of the launcher
can be predicted knowing only what versions are installed on the PC and
without regard to the order in which they were installed (i.e., without knowing
whether a 32 or 64-bit version of Python and corresponding launcher was
installed last). As noted above, an optional "-32" suffix can be used on a
version specifier to change this behaviour.

Examples:

* If no relevant options are set, the commands ``python`` and 
  ``python2`` will use the latest Python 2.x version installed and
  the command ``python3`` will use the latest Python 3.x installed.

* The commands ``python3.1`` and ``python2.7`` will not consult any
  options at all as the versions are fully specified.

* If ``PY_PYTHON=3``, the commands ``python`` and ``python3`` will both use 
  the latest installed Python 3 version.

* If ``PY_PYTHON=3.1-32``, the command ``python`` will use the 32-bit
  implementation of 3.1 whereas the command ``python3`` will use the latest
  installed Python (PY_PYTHON was not considered at all as a major
  version was specified.)

* If ``PY_PYTHON=3`` and ``PY_PYTHON3=3.1``, the commands 
  ``python`` and ``python3`` will both use specifically 3.1

In addition to environment variables, the same settings can be configured
in the .INI file used by the launcher.  The section in the INI file is
called ``[defaults]`` and the key name will be the same as the 
environment variables without the leading ``PY\_`` prefix (and note that
the key names in the INI file are case insensitive.)  The contents of
an environment variable will override things specified in the INI file.

For example:

* Setting ``PY_PYTHON=3.1`` is equivalent to the INI file containing:

::

  [defaults]
  python=3.1

* Setting ``PY_PYTHON=3`` and ``PY_PYTHON3=3.1`` is equivalent to the INI file
  containing:

::

  [defaults]
  python=3
  python3=3.1

-----------
Diagnostics
-----------

If an environment variable ``PYLAUNCH_DEBUG`` is set (to any value), the
launcher will print diagnostic information to stderr (i.e. to the console).
While this information manages to be simultaneously verbose *and* terse, it
should allow you to see what versions of Python were located, why a
particular version was chosen and the exact command-line used to execute the
target Python.
