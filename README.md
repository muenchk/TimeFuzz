# TimeFuzz

## Compilation

### Windows

```
git clone https://github.com/muenchk/TimeFuz.git
cd TimeFuzz
cmake --preset buildRelease
cmake --build buildRelease
```

The following presets are available for Windows
build - Debug
buildRelease - Release
buildAll - Debug with Tests

### Linux

```
git clone https://github.com/muenchk/TimeFuz.git
cd TimeFuzz
cmake --preset linux
cmake --build buildLinux
```

The following presets are available for Linux
linux = kali - Release
kali-debug = Debug
Gnu - using g++-13 - Release
GnuNoUI - using g++-13 - Release - does not require OpenGL and does
								   not provide a UI

