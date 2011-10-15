@echo off
\python32\python builddoc.py
set /P SIGNPWD=Password for signing:
REM x86 ...
REM \python32\python makemsi.py -o launchsys Platform=x86 Version=1.0.0.2 "Manufacturer=Vinay Sajip" ToSystem launcher
\python32\python makemsi.py -o launchwin Platform=x86 Version=1.0.0.2 "Manufacturer=Vinay Sajip" ToWindows launcher
\python32\python makemsi.py Platform=x86 Version=1.0.0.2 "Manufacturer=Vinay Sajip" launcher

REM x64 ...
REM \python32\python makemsi.py -o launchsys Platform=x64 Version=1.0.0.2 "Manufacturer=Vinay Sajip" ToSystem launcher
\python32\python makemsi.py -o launchwin Platform=x64 Version=1.0.0.2 "Manufacturer=Vinay Sajip" ToWindows launcher
\python32\python makemsi.py Platform=x64 Version=1.0.0.2 "Manufacturer=Vinay Sajip" launcher
set SIGNPWD=
