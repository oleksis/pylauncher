# x86 ...
python makemsi.py -x -o uilauncher Platform=x86 Version=2.0.0.1 '"Manufacturer=Oleksis Fraga"' uilauncher
python makemsi.py -x -o uilaunchwin Platform=x86 Version=2.0.0.1 '"Manufacturer=Oleksis Fraga"' ToWindows uilauncher

# x64 ...
python makemsi.py -x -o uilauncher Platform=x64 Version=2.0.0.1 '"Manufacturer=Oleksis Fraga"' uilauncher
python makemsi.py -x -o uilaunchwin Platform=x64 Version=2.0.0.1 '"Manufacturer=Oleksis Fraga"' ToWindows uilauncher
