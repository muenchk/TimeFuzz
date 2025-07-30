# TimeFuzz
Framework for directed black-box fuzzing of interactive programs. Uses a grammar-based approach to generate valid program inputs in the form of time-series, where each entry represents a subsequent user-event for the target program.

Support for use-defined test oracle, dynamic command-line arguments and fuzzing via API by using LUA scripts.

Fuzzes target programs by observing whether the target program consumes user-events from the program inputs and measures reaction times.

## Requirements

* [CMake](https://cmake.org/)
	* Add this to your `PATH`
* [Vcpkg](https://github.com/microsoft/vcpkg)
	* Add the environment variable `VCPKG_ROOT` with the value as the path to the folder containing vcpkg
* [Visual Studio Community 2022](https://visualstudio.microsoft.com/)
	* Desktop development with C++
* [Ninja](https://ninja-build.org/)
	* Add the environment variable NinjaPath
* [Other] May require the installation of system packages as required by the vcpkg dependencies

## Compilation

### Windows

```
git clone https://github.com/muenchk/TimeFuz.git
cd TimeFuzz
cmake --preset release
cmake --build build-release
```

The following presets are available for Windows
release, all-release (with tests)
debug, all-debug (with tests)

### Linux

```
git clone https://github.com/muenchk/TimeFuz.git
cd TimeFuzz
cmake --preset kali-release
cmake --build build-kali-release
```

### Linux without UI

```
git clone https://github.com/muenchk/TimeFuz.git
cd TimeFuzz
cmake --preset kali-release-noui
cmake --build build-kali-release-noui
```


The following presets are available for Linux
For clang-18:
linux-release, kali-release, kali-release-all (with tests), kali-release-noui
linux-debug, kali-debug, kali-debug-all (with tests), linux-debug-noui
For gcc-13:
linux-gnu-release, linux-gnu-release-noui, linux-gnu-debug-noui

The noui presets can be build without OpenGL. The other presets require OpenGL support and packages

