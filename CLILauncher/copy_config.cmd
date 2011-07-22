@echo off
Echo copying .ini files ...
if not exist "%1py.ini" copy global.ini "%1py.ini"
if not exist "%appdata%\py.ini" copy local.ini "%appdata%\py.ini"
