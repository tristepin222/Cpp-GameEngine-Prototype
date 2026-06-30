@echo off
setlocal enabledelayedexpansion

:: Help message
if "%1"=="--help" (
    echo Usage: build.bat [options]
    echo.
    echo Options:
    echo   --clean    Remove the existing build directory and run a fresh configuration.
    echo   --run      Launch the compiled sandbox game executable after building.
    echo   --help     Show this help message.
    exit /b 0
)

:: Clean check
set CLEAN_BUILD=0
if "%1"=="--clean" set CLEAN_BUILD=1
if "%2"=="--clean" set CLEAN_BUILD=1

if !CLEAN_BUILD! equ 1 (
    echo [INFO] Cleaning build directory...
    if exist build (
        rmdir /s /q build
    )
)

:: Configure CMake Workspace
echo [INFO] Configuring CMake workspace...
cmake -B build -DCMAKE_BUILD_TYPE=Release
if %errorlevel% neq 0 (
    echo [ERROR] CMake configuration failed.
    exit /b %errorlevel%
)

:: Compile Workspace Targets
echo [INFO] Compiling workspace targets (Release)...
cmake --build build --config Release
if %errorlevel% neq 0 (
    echo [ERROR] Build compilation failed.
    exit /b %errorlevel%
)

echo [SUCCESS] Workspace built successfully.

:: Run check
set RUN_GAME=0
if "%1"=="--run" set RUN_GAME=1
if "%2"=="--run" set RUN_GAME=1

if !RUN_GAME! equ 1 (
    echo [INFO] Launching Sandbox Game...
    cd build\sandbox_game\Release
    game.exe
)
