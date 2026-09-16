# An extensible desktop taskbar replacement

Tasked is a C++ and Qt Quick desktop shell intended to replace the operating system taskbar with an elegant, extensible bar for Windows, macOS, and Linux.

The primary target is taskbar replacement, not a floating toolbar. On Windows, Tasked currently hides and reserves the Explorer taskbar while the dock is running, then restores it when Tasked exits.

## Project direction

- One self-contained Tasked executable
- Optional extensions loaded from an `extensions` directory
- Built-in QML and visual resources embedded into the executable
- Platform-specific shell integration behind small native adapters
- User-configurable startup with the operating system
- Cross-platform visual design with platform-specific taskbar behavior

## Current state

The repository currently contains a static Qt Quick host with a Windows AppBar reservation, taskbar takeover/restore guard, a configurable Combined/Split bottom dock, native executable icons, running-window/tray enumeration, and the first Styling settings surface. Explorer taskbar replacement is being validated in the Windows-first build.

## Build

Requirements:

- MSVC 2022 x64
- CMake and Ninja
- Static Qt 6.8.3 installed at `D:/Qt/6.8.3-static-msvc2022_64`

From the project directory:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=D:/Qt/6.8.3-static-msvc2022_64 -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded
cmake --build build --parallel
```

The initial executable is `bin/Tasked.exe`.

## Source status

Tasked is currently closed source. The licensing position may change in the future.
