# BLDR
A Lua script based build system for C/C++.

# Usage

```
Usage:
bldr install add    : Install new bldr files
     .       remove : Uninstall bldr files
     .       .        -all : Delete *all* files
     .       clean  : Clean broken bldr files
     .       list   : List bldr files
     env     clear  : Clear environemnts
     make           : Build projects
                      -clean   : Clean build
                      -no-warn : Disable warnings
                      -no-opt  : Disable optimizations
```

# Setup

1) Run: `setup.bat` (Builds dependencies)
2) Run: `build.bat` (Builds and bootstraps bldr)
3) Put `bin/bldr.exe` on system PATH