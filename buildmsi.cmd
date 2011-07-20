@echo off
REM x86 ...
\python27\python makemsi.py -o launchsys Platform=x86 Version=1.0.0.0 "Manufacturer=Vinay Sajip" ToSystem launcher
\python27\python makemsi.py -o launchwin Platform=x86 Version=1.0.0.0 "Manufacturer=Vinay Sajip" ToWindows launcher
\python27\python makemsi.py Platform=x86 Version=1.0.0.0 "Manufacturer=Vinay Sajip" launcher

REM x64 ...
\python27\python makemsi.py -o launchsys Platform=x64 Version=1.0.0.0 "Manufacturer=Vinay Sajip" ToSystem launcher
\python27\python makemsi.py -o launchwin Platform=x64 Version=1.0.0.0 "Manufacturer=Vinay Sajip" ToWindows launcher
\python27\python makemsi.py Platform=x64 Version=1.0.0.0 "Manufacturer=Vinay Sajip" launcher