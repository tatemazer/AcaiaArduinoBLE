@echo off
setlocal enabledelayedexpansion

echo ============================================
echo   ShotStopper - Smart Bring-Up Script
echo ============================================

rem --- Auto-detect bootloader file ---
for %%F in (shotStopper_bootloader_*.bin) do (
    set BOOTLOADER_FILE=%%F
)
if not defined BOOTLOADER_FILE (
    echo ERROR: No bootloader file found matching shotStopper_bootloader_*.bin
    pause
    exit /b 1
)

echo Found bootloader file: %BOOTLOADER_FILE%
echo.

rem --- List all app files ---
set COUNT=0
for %%F in (shotStopper_app_*.bin) do (
    set /a COUNT+=1
    set APP_FILE_!COUNT!=%%F

    rem --- Determine label based on filename ending ---
    set LABEL_!COUNT!=

    echo %%F | findstr /i "_mom0_reed0.bin" >nul && set LABEL_!COUNT!=Micra/Mini
    echo %%F | findstr /i "_mom1_reed1.bin" >nul && set LABEL_!COUNT!=GS3
    echo %%F | findstr /i "_mom1_reed0.bin" >nul && set LABEL_!COUNT!=Silvia
    echo %%F | findstr /i "_mom0_reed1.bin" >nul && set LABEL_!COUNT!=Stone
)

if %COUNT%==0 (
    echo ERROR: No app files found matching shotStopper_app_*.bin
    pause
    exit /b 1
)

echo Available application files:
for /l %%I in (1,1,%COUNT%) do (
    set FILE=!APP_FILE_%%I!
    set LABEL=!LABEL_%%I!

    if defined LABEL (
        echo   %%I^) !FILE!   [!LABEL!]
    ) else (
        echo   %%I^) !FILE!
    )
)

echo.
set /p USER_CHOICE=Enter the number of the app file to flash: 

if %USER_CHOICE% GTR %COUNT% (
    echo Invalid selection.
    pause
    exit /b 1
)

set APP_FILE=!APP_FILE_%USER_CHOICE%!

echo.
echo Selected app file: %APP_FILE%
echo.

echo Make sure the board is in DOWNLOAD MODE:
echo   1) Hold BOOT
echo   2) Plug in USB
echo   3) Release BOOT
echo.
pause

echo.
echo Reading first 0x6000 bytes (bootloader region)...
python -m esptool read-flash 0x0 0x6000 device_bootloader.bin
if %errorlevel% neq 0 (
    echo ERROR: Could not read flash from device.
    pause
    exit /b %errorlevel%
)

echo.
echo Extracting first 0x6000 bytes from known-good bootloader...
copy /b "%BOOTLOADER_FILE%" bootloader_trim.bin >nul
powershell -command "(Get-Content 'bootloader_trim.bin' -Encoding Byte)[0..24575] | Set-Content 'bootloader_trim.bin' -Encoding Byte"

echo.
echo Computing hashes...

certutil -hashfile device_bootloader.bin SHA256 > device_hash.txt
certutil -hashfile bootloader_trim.bin SHA256 > good_hash.txt

set "device_hash="
for /f "skip=1 tokens=1" %%A in (device_hash.txt) do (
    if not defined device_hash set device_hash=%%A
)

set "good_hash="
for /f "skip=1 tokens=1" %%A in (good_hash.txt) do (
    if not defined good_hash set good_hash=%%A
)

echo Device hash: !device_hash!
echo Good hash:   !good_hash!
echo.

if "!device_hash!"=="!good_hash!" (
    echo Bootloader matches known-good version.
    echo Will flash APP ONLY.
    set FLASH_BOOTLOADER=0
) else (
    echo Bootloader does NOT match known-good version.
    echo Will ERASE FLASH and flash BOOTLOADER + APP.
    set FLASH_BOOTLOADER=1
)

echo.

if %FLASH_BOOTLOADER%==1 (
    echo Erasing flash...
    python -m esptool erase-flash
    if %errorlevel% neq 0 (
        echo ERROR: erase-flash failed.
        pause
        exit /b %errorlevel%
    )
)

echo.
if %FLASH_BOOTLOADER%==1 (
    echo Flashing bootloader + app...
    python -m esptool write-flash 0x0 "%BOOTLOADER_FILE%" 0x10000 "%APP_FILE%"
) else (
    echo Flashing app only...
    python -m esptool write-flash 0x10000 "%APP_FILE%"
)

if %errorlevel% neq 0 (
    echo ERROR: write-flash failed.
    pause
    exit /b %errorlevel%
)

echo.
echo Cleaning up temporary files...
del device_bootloader.bin >nul 2>&1
del device_hash.txt >nul 2>&1
del good_hash.txt >nul 2>&1
del bootloader_trim.bin >nul 2>&1

echo.
echo ============================================
echo   Flashing complete!
echo   Temporary files removed.
echo   Now UNPLUG and REPLUG the board
echo   (do NOT hold BOOT)
echo ============================================
echo.
pause