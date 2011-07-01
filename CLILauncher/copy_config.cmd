@echo off
Echo copying .ini files ...
copy global.ini %1py.ini
copy local.ini %appdata%\py.ini
