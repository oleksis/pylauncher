@echo off
REM x86 ...
\python27\python makemsi.py Platform=x86 Version=1.0.0.0 "Manufacturer=Vinay Sajip" ToSystem launcher
xcopy /y launcher.msi launchsys.msi
\python27\python makemsi.py Platform=x86 Version=1.0.0.0 "Manufacturer=Vinay Sajip" launcher

REM x64 ...
\python27\python makemsi.py Platform=x64 Version=1.0.0.0 "Manufacturer=Vinay Sajip" ToSystem launcher
xcopy /y launcher.amd64.msi launchsys.amd64.msi
\python27\python makemsi.py Platform=x64 Version=1.0.0.0 "Manufacturer=Vinay Sajip" launcher