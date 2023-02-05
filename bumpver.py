import re
from pathlib import Path


def main():
    VER = '2.0.0.1'
    py_files = ["buildmsi.py"]
    rc_sources = [
        "CLILauncher/CLILauncher.rc",
        "CLIWrapper/CLIWrapper.rc",
        "GUILauncher/GUILauncher.rc",
        "GUIWrapper/GUIWrapper.rc"
    ]

    MAJOR, MINOR, PATCH, INC0 = VER.split(".")

    for rc in rc_sources:
        file = Path(rc)
        content = re.sub(
            r"\d,\d,\d,\d", f"{MAJOR},{MINOR},{PATCH},{INC0}", file.read_text(encoding="utf-8"))
        content = re.sub(r"\d,\s\d,\s\d,\s\d",
                         f"{MAJOR}, {MINOR}, {PATCH}, {INC0}", content)
        file.write_text(content, encoding="utf-8")

    for py in py_files:
        file = Path(py)
        content = re.sub(
            r"\d\.\d\.\d\.\d", f"{MAJOR}.{MINOR}.{PATCH}.{INC0}", file.read_text(encoding="utf-8"))
        file.write_text(content, encoding="utf-8")


if __name__ == "__main__":
    main()
