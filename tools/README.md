# Local RE tooling

Drop third-party reverse engineering executables here. This folder is
gitignored so nothing in it will be committed.

Expected layout:

```
tools/
├── extract-xiso.exe    # https://github.com/XboxDev/extract-xiso/releases
└── xextool.exe         # xorloser's xextool (community distribution)
```

`WindowsScripts/Setup.bat` looks in this folder as a fallback if the
tools aren't on `PATH`.
