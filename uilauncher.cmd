@echo off
REM x86 ...
\python32\python makemsi.py -x -o uilauncher Platform=x86 Version=1.0.0.0 "Manufacturer=Vinay Sajip" uilauncher
\python32\python makemsi.py -x -o uilaunchwin Platform=x86 Version=1.0.0.0 "Manufacturer=Vinay Sajip" ToWindows uilauncher

REM x64 ...
\python32\python makemsi.py -x -o uilauncher Platform=x64 Version=1.0.0.0 "Manufacturer=Vinay Sajip" uilauncher
\python32\python makemsi.py -x -o uilaunchwin Platform=x64 Version=1.0.0.0 "Manufacturer=Vinay Sajip" ToWindows uilauncher
