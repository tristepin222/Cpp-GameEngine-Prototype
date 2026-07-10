@echo off
setlocal

REM ============================================================
REM  build_game_package.bat
REM  Packages a standalone game distributable.
REM  Called by the editor's Build Settings panel.
REM
REM  Usage:
REM    build_game_package.bat <project_path> <output_path>
REM
REM  Expects to be run from the same directory as editor.exe,
REM  which is sdk/bin/ (so game_runtime.exe, engine.dll, etc. are next to it).
REM ============================================================

set PROJECT_PATH=%~1
set OUTPUT_PATH=%~2
set SCRIPT_DIR=%~dp0

if "%PROJECT_PATH%"=="" (
    echo [ERROR] No project path provided.
    exit /b 1
)
if "%OUTPUT_PATH%"=="" (
    echo [ERROR] No output path provided.
    exit /b 1
)

echo [Build] Packaging game to: %OUTPUT_PATH%
echo [Build] Project path: %PROJECT_PATH%

REM Create output directory structure
if exist "%OUTPUT_PATH%" rmdir /s /q "%OUTPUT_PATH%"
mkdir "%OUTPUT_PATH%"

REM Copy headless runtime as game.exe
copy /Y "%SCRIPT_DIR%game_runtime.exe" "%OUTPUT_PATH%\game.exe"
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] game_runtime.exe not found next to editor. Run build_engine.bat first.
    exit /b 1
)

REM Copy engine.dll
copy /Y "%SCRIPT_DIR%engine.dll" "%OUTPUT_PATH%\"

REM Copy engine plugins
if exist "%SCRIPT_DIR%plugins" (
    xcopy /E /Y /I "%SCRIPT_DIR%plugins" "%OUTPUT_PATH%\plugins\"
)

REM Copy compiled built-in shaders
if exist "%SCRIPT_DIR%..\shaders" (
    xcopy /E /Y /I "%SCRIPT_DIR%..\shaders" "%OUTPUT_PATH%\shaders\"
) else if exist "%SCRIPT_DIR%build\shaders" (
    xcopy /E /Y /I "%SCRIPT_DIR%build\shaders" "%OUTPUT_PATH%\shaders\"
)

REM Copy project assets, scenes, project.settings
if exist "%PROJECT_PATH%\assets" (
    xcopy /E /Y /I "%PROJECT_PATH%\assets" "%OUTPUT_PATH%\assets\"
)
if exist "%PROJECT_PATH%\scenes" (
    xcopy /E /Y /I "%PROJECT_PATH%\scenes" "%OUTPUT_PATH%\scenes\"
)
if exist "%PROJECT_PATH%\project.settings" (
    copy /Y "%PROJECT_PATH%\project.settings" "%OUTPUT_PATH%\"
)

REM Copy compiled user script DLLs (if any)
if exist "%PROJECT_PATH%\scripts" (
    for /r "%PROJECT_PATH%\scripts" %%f in (*.dll) do (
        copy /Y "%%f" "%OUTPUT_PATH%\scripts\"
    )
)

echo.
echo [Build] Game packaged successfully at: %OUTPUT_PATH%
echo [Build] Run: %OUTPUT_PATH%\game.exe
echo.

endlocal
exit /b 0
