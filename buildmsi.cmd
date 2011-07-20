@echo off
REM x86 ...
\python32\python makemsi.py -o launchsys Platform=x86 Version=1.0.0.0 "Manufacturer=Vinay Sajip" ToSystem launcher
\python32\python makemsi.py -o launchwin Platform=x86 Version=1.0.0.0 "Manufacturer=Vinay Sajip" ToWindows launcher
\python32\python makemsi.py Platform=x86 Version=1.0.0.0 "Manufacturer=Vinay Sajip" launcher

REM x64 ...
\python32\python makemsi.py -o launchsys Platform=x64 Version=1.0.0.0 "Manufacturer=Vinay Sajip" ToSystem launcher
\python32\python makemsi.py -o launchwin Platform=x64 Version=1.0.0.0 "Manufacturer=Vinay Sajip" ToWindows launcher
\python32\python makemsi.py Platform=x64 Version=1.0.0.0 "Manufacturer=Vinay Sajip" launcher