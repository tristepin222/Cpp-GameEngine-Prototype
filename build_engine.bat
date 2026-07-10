@echo off
setlocal

REM ============================================================
REM  build_engine.bat
REM  Builds the engine and installs it as an SDK to ./sdk/
REM ============================================================

set BUILD_DIR=build_engine
set SDK_DIR=sdk
set CONFIG=Release

set CMAKE_GENERATOR=Visual Studio 17 2022
set CMAKE_GENERATOR_PLATFORM=x64
set CMAKE_GENERATOR_TOOLSET=v143

if exist %BUILD_DIR% (
    echo [Engine Build] Removing stale CMake build directory...
    rmdir /s /q %BUILD_DIR%
)

echo [Engine Build] Configuring CMake (engine-only)...
cmake -S . -B %BUILD_DIR% ^
    -G "%CMAKE_GENERATOR%" ^
    -A %CMAKE_GENERATOR_PLATFORM% ^
    -T %CMAKE_GENERATOR_TOOLSET% ^
    -DCMAKE_BUILD_TYPE=%CONFIG%

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CMake configuration failed.
    exit /b 1
)

echo [Engine Build] Compiling engine and plugins...
cmake --build %BUILD_DIR% --config %CONFIG%

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Engine build failed.
    exit /b 1
)

echo [Engine Build] Assembling SDK at ./%SDK_DIR%/ ...

REM Create SDK directory structure
if exist %SDK_DIR% rmdir /s /q %SDK_DIR%
mkdir %SDK_DIR%\bin
mkdir %SDK_DIR%\lib
mkdir %SDK_DIR%\include
mkdir %SDK_DIR%\src
mkdir %SDK_DIR%\third_party\imgui\backends
mkdir %SDK_DIR%\third_party\imguizmo
mkdir %SDK_DIR%\cmake
mkdir %SDK_DIR%\shaders

REM Copy engine DLL and import lib
copy /Y %BUILD_DIR%\engine\%CONFIG%\engine.dll %SDK_DIR%\bin\
copy /Y %BUILD_DIR%\engine\%CONFIG%\engine.lib %SDK_DIR%\lib\

REM Copy editor and game_runtime executables
copy /Y %BUILD_DIR%\engine\%CONFIG%\editor.exe      %SDK_DIR%\bin\
copy /Y %BUILD_DIR%\engine\%CONFIG%\game_runtime.exe %SDK_DIR%\bin\

REM Copy pre-built static libs (imgui, imguizmo, glfw)
copy /Y %BUILD_DIR%\engine\%CONFIG%\imgui.lib    %SDK_DIR%\lib\
copy /Y %BUILD_DIR%\engine\%CONFIG%\imguizmo.lib %SDK_DIR%\lib\
copy /Y %BUILD_DIR%\_deps\glfw-build\src\%CONFIG%\glfw3.lib %SDK_DIR%\lib\

REM Copy public engine headers (include/)
xcopy /E /Y /I engine\include %SDK_DIR%\include

REM Copy internal headers needed by public API (src/**/*.hpp)
xcopy /E /Y /I /EXCLUDE:build_engine_exclude.txt engine\src %SDK_DIR%\src

REM Copy third-party headers
xcopy /E /Y /I third_party\imgui\*.h      %SDK_DIR%\third_party\imgui\
xcopy /E /Y /I third_party\imgui\backends %SDK_DIR%\third_party\imgui\backends\
xcopy /E /Y /I third_party\imguizmo\*.h   %SDK_DIR%\third_party\imguizmo\

REM Copy GLFW headers from FetchContent cache
xcopy /E /Y /I %BUILD_DIR%\_deps\glfw-src\include\GLFW %SDK_DIR%\include\GLFW\

REM Copy GLM headers from FetchContent cache
xcopy /E /Y /I %BUILD_DIR%\_deps\glm-src\glm %SDK_DIR%\include\glm\

REM Copy compiled shaders
xcopy /E /Y /I %BUILD_DIR%\shaders %SDK_DIR%\shaders\

REM Copy plugin DLLs to sdk/bin/plugins/ (cinemachine etc.)
if exist %BUILD_DIR%\plugins\cinemachine\%CONFIG%\cinemachine_plugin.dll (
    mkdir %SDK_DIR%\bin\plugins
    copy /Y %BUILD_DIR%\plugins\cinemachine\%CONFIG%\cinemachine_plugin.dll %SDK_DIR%\bin\plugins\
)

REM Copy EngineConfig.cmake (for standalone game CMake projects)
copy /Y engine\cmake\EngineConfig.cmake %SDK_DIR%\cmake\

REM Copy the game packaging script next to editor.exe
copy /Y build_game_package.bat %SDK_DIR%\bin\

echo.
echo [SUCCESS] Engine SDK assembled at: ./%SDK_DIR%/
echo.
echo   sdk/bin/editor.exe         <- Run this to open the editor
echo   sdk/bin/game_runtime.exe   <- Headless runtime (packaged as game.exe on Build)
echo.
echo   To open your project:
echo     cd sandbox_game
echo     ..\sdk\bin\editor.exe
echo.

endlocal

