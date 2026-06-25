@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
cd /d "%SCRIPT_DIR%"

echo.
echo ==== Jianjian ESP32 Flash Helper ====
echo.
echo Step 1: Current serial devices
pio device list
echo.
set /p PORT=Enter the current ESP32 COM port (example: COM8 or COM12): 
if "%PORT%"=="" (
  echo No COM port entered. Exiting.
  exit /b 1
)

echo.
echo Step 2: Trying upload on %PORT%
pio run -t upload --upload-port %PORT%
if %errorlevel%==0 (
  echo.
  echo Flash complete on %PORT%.
  exit /b 0
)

echo.
echo Upload failed on %PORT%.
echo The board may have switched into download mode and changed COM port.
echo.
echo Step 3: Scan ports again and retry with the new port
pio device list
echo.
set /p PORT2=Enter the new COM port after reset (example: COM8 or COM9): 
if "%PORT2%"=="" (
  echo No fallback COM port entered. Exiting.
  exit /b 1
)

echo.
echo Step 4: Retrying upload on %PORT2%
pio run -t upload --upload-port %PORT2%
if %errorlevel%==0 (
  echo.
  echo Flash complete on %PORT2%.
  exit /b 0
)

echo.
echo Flash failed again. Check cable, power, and whether the board is in download mode.
exit /b 1
