@echo off
setlocal
if not exist build mkdir build
cmake -S . -B build -A x64
if errorlevel 1 exit /b 1
cmake --build build --config Release --parallel
if errorlevel 1 exit /b 1
ctest --test-dir build -C Release --output-on-failure
if errorlevel 1 exit /b 1
if exist build\Release\MyVocalSynth.exe (
  mkdir dist 2>nul
  copy /Y build\Release\MyVocalSynth.exe dist\ >nul
  where windeployqt >nul 2>nul && windeployqt --release --compiler-runtime dist\MyVocalSynth.exe
)
echo Build complete.
