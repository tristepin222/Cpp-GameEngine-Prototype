@echo off
setlocal

REM Set path to Vulkan glslc
set GLSLC=C:/VulkanSDK/1.3.296.0/Bin/glslc.exe

REM Set source and output directories
set SRC_DIR=assets/shaders
set OUT_DIR=build/shaders

REM Create output directory if it doesn't exist
if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

REM Compile all .vert shaders
for /R "%SRC_DIR%" %%f in (*.vert) do (
    echo Compiling %%f...
    "%GLSLC%" "%%f" -o "%OUT_DIR%\%%~nf.vert.spv"
)

REM Compile all .frag shaders
for /R "%SRC_DIR%" %%f in (*.frag) do (
    echo Compiling %%f...
    "%GLSLC%" "%%f" -o "%OUT_DIR%\%%~nf.frag.spv"
)

echo All shaders compiled.
pause
