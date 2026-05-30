[![CMake](https://github.com/ZombiTefu/majimix/actions/workflows/cmake.yml/badge.svg)](https://github.com/ZombiTefu/majimix/actions/workflows/cmake.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
# Majimix

## Overview
Majimix is a high-level API designed to play simultaneously audio sources (WAVE files, Xiph Ogg audio files, KSS files) as sounds or background musics.


## Example
```cpp
#include <iostream>
#include <majimix/majimix.hpp>

int main()
{
    // initialize majimix
    majimix::pa::initialize();

    // create a majimix instance
    auto majimix_ptr = majimix::pa::create_instance();

    // set the majimix output format : rate 44,1 KHz stereo 16 bits
    // and use 10 channels : you can play 10 sounds simultaneously
    if (majimix_ptr->set_format(44100, true, 16, 10))
    {
        majimix_ptr->set_master_volume(50);
        // add source and get the source handle (it can be done later)
        int source_handle_1 = majimix_ptr->add_source("bgm.ogg");
        // ... add other sources ...

        // start majimix instance
        if (majimix_ptr->start_mixer())
        {
            // play bgm.ogg (loop)
            int play_handle_1 = majimix_ptr->play_source(source_handle_1, true, false);
            std::cout << play_handle_1 << "\n";

            // add a new source
            int source_handle_2 = majimix_ptr->add_source("sound.wav");
            std::cout << source_handle_2 << "\n";

            // press any key ('q' to quit)
            char u = 0;
            while(u != 'q') 
            {
                // play sound.wav (once) while bgm.ogg continu
                int play_handle_2 = majimix_ptr->play_source(source_handle_2);
                std::cout << "Press 'q' to quit\n";
                std::cin >> u; 
            }

            // stop majimix
            majimix_ptr->stop_mixer();
        }
    }

    // dispose
    majimix::pa::terminate();
    return 0;
}
 ```

 ## Dependencies

 Majimix uses the following libraries:<br>
   - PortAudio   - http://www.portaudio.com
   - Xiph libogg - https://xiph.org/ogg
   - libKss      - https://github.com/digital-sound-antiques/libkss

 ## Build

Majimix now defaults to a bundled build: CMake downloads and builds libkss,
libogg, libvorbis and PortAudio automatically.

Bundled dependencies follow the selected build configuration. The Release and
MinSizeRel presets therefore build both Majimix and the bundled dependencies in
the same configuration.

An experimental Linux to Windows cross-compilation flow using MinGW-w64 is also
available. It is intended for generating Windows DLL artifacts from a Linux host.

### General prerequisites

- CMake 3.21 or newer
- A C++17 compiler
- Internet access for the first configure step, because dependencies are downloaded automatically
- Git, currently required for libkss because it is fetched with its submodules

### Linux prerequisites

Required:
- CMake 3.21 or newer
- Ninja, because the Linux preset uses the Ninja generator
- GCC or Clang with C++17 support
- ALSA development headers for PortAudio
- Git

Examples:

Debian or Ubuntu:
```sh
sudo apt-get update
sudo apt-get install -y cmake ninja-build g++ git libasound2-dev
```

Fedora:
```sh
sudo dnf install cmake ninja-build gcc-c++ git alsa-lib-devel
```

Arch Linux:
```sh
sudo pacman -S cmake ninja gcc git alsa-lib
```

### Windows prerequisites

Required for every Windows build:
- CMake 3.21 or newer
- Git
- Internet access for the first configure step

For the MSVC preset:
- Visual Studio 2022 or Build Tools 2022 with the Desktop development with C++ workload

For the MinGW preset:
- MinGW-w64 in PATH
- The MinGW Makefiles generator requirements available in PATH

### Linux bundled build

Configure:
```sh
cmake --preset linux-release
```

Build:
```sh
cmake --build --preset linux-release
```

Install into a local test prefix:
```sh
cmake --install out/build/linux-release --prefix out/install/linux-release
```

Install into the default system prefix:
```sh
sudo cmake --install out/build/linux-release
```

Size-optimized MinSizeRel build:
```sh
cmake --preset linux-minsizerel
cmake --build --preset linux-minsizerel
cmake --install out/build/linux-minsizerel --prefix out/install/linux-minsizerel
```

### Windows bundled build with MSVC

Configure:
```powershell
cmake --preset windows-msvc-release
```

Build:
```powershell
cmake --build --preset windows-msvc-release
```

Install into a local test prefix:
```powershell
cmake --install out/build/windows-msvc-release --config Release --prefix out/install/windows-msvc-release
```

Size-optimized MinSizeRel build:
```powershell
cmake --preset windows-msvc-minsizerel
cmake --build --preset windows-msvc-minsizerel
cmake --install out/build/windows-msvc-minsizerel --config MinSizeRel --prefix out/install/windows-msvc-minsizerel
```

### Windows bundled build with MinGW

Configure:
```powershell
cmake --preset windows-mingw-release
```

Build:
```powershell
cmake --build --preset windows-mingw-release
```

Install into a local test prefix:
```powershell
cmake --install out/build/windows-mingw-release --prefix out/install/windows-mingw-release
```

Size-optimized MinSizeRel build:
```powershell
cmake --preset windows-mingw-minsizerel
cmake --build --preset windows-mingw-minsizerel
cmake --install out/build/windows-mingw-minsizerel --prefix out/install/windows-mingw-minsizerel
```

### Experimental Linux to Windows cross-compilation with MinGW-w64

Required on the Linux host:
- Ninja
- MinGW-w64 cross-compilers in PATH
- `x86_64-w64-mingw32-gcc`, `x86_64-w64-mingw32-g++`, and `x86_64-w64-mingw32-windres`

Example on Debian or Ubuntu:
```sh
sudo apt-get update
sudo apt-get install -y cmake ninja-build gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64 binutils-mingw-w64-x86-64
```

Release build:
```sh
cmake --preset linux-cross-windows-mingw-release
cmake --build --preset linux-cross-windows-mingw-release
cmake --install out/build/linux-cross-windows-mingw-release --prefix out/install/linux-cross-windows-mingw-release
```

Size-optimized MinSizeRel build:
```sh
cmake --preset linux-cross-windows-mingw-minsizerel
cmake --build --preset linux-cross-windows-mingw-minsizerel
cmake --install out/build/linux-cross-windows-mingw-minsizerel --prefix out/install/linux-cross-windows-mingw-minsizerel
```

This flow is experimental, but it has been locally validated for configure,
build, and install with bundled dependencies.

### Optional system dependencies mode

If you prefer preinstalled system packages for Ogg, Vorbis and PortAudio,
you can disable bundled dependencies.

Linux example:
```sh
cmake -S . -B out/build/system -G Ninja -DMAJIMIX_USE_BUNDLED_DEPS=OFF
cmake --build out/build/system
cmake --install out/build/system --prefix out/install/system
```

libkss remains bundled in every mode.

### Installed files

The install step provides:
- the shared library
- the public header in include/majimix
- the CMake package files exporting the Majimix::pa target
- the pkg-config file on Unix platforms
