# MyVocalSynth

Open-source Windows desktop vocal editor built with C++17, Qt 6 Widgets and CMake. The project is designed around UTAU/OpenUtau-style voicebanks (`oto.ini`, `character.txt`) and an extensible phonemizer/resampler architecture.

## Requirements
- Windows 10/11
- Visual Studio 2022 with C++ Desktop workload
- Qt 6 with Core, Gui, Widgets, Multimedia
- CMake 3.21+
- A compatible UTAU resampler (moresampler is the intended default)

## Build
Run `build.bat` from a VS 2022 developer command prompt. Configure Qt through `CMAKE_PREFIX_PATH` when required, for example:

`cmake -S . -B build -A x64 -DCMAKE_PREFIX_PATH=C:\Qt\6.x.x\msvc2022_64`

The current renderer architecture validates the phonemization stage and keeps the external resampler invocation isolated so each resampler's command-line contract can be handled without contaminating the editor model.
