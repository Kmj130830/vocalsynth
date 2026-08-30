@echo off
setlocal
if not exist build\Release\MyVocalSynth.exe (
  echo Release executable not found. Build first.
  exit /b 1
)
if not exist dist mkdir dist
copy /Y build\Release\MyVocalSynth.exe dist\MyVocalSynth.exe >nul
where windeployqt >nul 2>nul
if errorlevel 1 (
  echo windeployqt was not found in PATH.
  echo Add the Qt bin directory to PATH and rerun.
  exit /b 1
)
windeployqt --release --compiler-runtime dist\MyVocalSynth.exe
if exist resampler\moresampler.exe copy /Y resampler\moresampler.exe dist\resampler\moresampler.exe >nul
if exist resources xcopy /E /I /Y resources dist\resources >nul
if exist VoiceBanks xcopy /E /I /Y VoiceBanks dist\VoiceBanks >nul
echo Deployment complete.
