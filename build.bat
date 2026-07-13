@echo off
setlocal enabledelayedexpansion

:: ============================================================
::  build.bat
::  Builds the engine + editor and assembles the SDK.
::  The game is built inside the editor via File -> Build Settings.
:: ============================================================

:: Help message
if "%1"=="--help" (
    echo Usage: build.bat [options]
    echo.
    echo Options:
    echo   --clean    Remove the existing build and SDK directories and rebuild.
    echo   --run      Build and immediately open the editor with sandbox_game.
    echo   --help     Show this help message.
    exit /b 0
)

:: Clean check
set CLEAN_BUILD=0
if "%1"=="--clean" set CLEAN_BUILD=1
if "%2"=="--clean" set CLEAN_BUILD=1

if !CLEAN_BUILD! equ 1 (
    echo [INFO] Cleaning build and SDK directories...
    if exist build_engine rmdir /s /q build_engine
    if exist build rmdir /s /q build
    if exist sdk rmdir /s /q sdk
    if exist sandbox_game\build rmdir /s /q sandbox_game\build
    if exist sandbox_game\bin rmdir /s /q sandbox_game\bin
)

echo [INFO] Building Engine SDK (editor + runtime + plugins)...
call build_engine.bat
if %errorlevel% neq 0 (
    echo [ERROR] Engine SDK build failed.
    exit /b %errorlevel%
)

:: Compile sandbox_game user scripts dynamically using the SDK CMake config
if exist sandbox_game\scripts\CMakeLists.txt (
    echo [INFO] Building sandbox_game user scripts...
    if not exist sandbox_game\scripts\build mkdir sandbox_game\scripts\build
    cmake -S sandbox_game\scripts -B sandbox_game\scripts\build -G "Visual Studio 17 2022" -A x64 -T v143 -DCMAKE_BUILD_TYPE=Release
    cmake --build sandbox_game\scripts\build --config Release
    if !errorlevel! neq 0 (
        echo [ERROR] User scripts build failed.
        exit /b !errorlevel!
    )
)

echo [SUCCESS] Engine built successfully.
echo.
echo   To open the sandbox project in the editor:
echo     cd sandbox_game
echo     ..\sdk\bin\editor.exe
echo.

:: Run check — open editor with sandbox_game as the project
set RUN_EDITOR=0
if "%1"=="--run" set RUN_EDITOR=1
if "%2"=="--run" set RUN_EDITOR=1

if !RUN_EDITOR! equ 1 (
    echo [INFO] Launching editor with sandbox_game project...
    if exist sdk\bin\editor.exe (
        start "" "sdk\bin\editor.exe" "sandbox_game"
    ) else (
        echo [ERROR] editor.exe not found in sdk\bin\. Build may have failed.
    )
)

endlocal
