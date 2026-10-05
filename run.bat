@echo off
rem Builds the project if needed and starts the sample launcher.
rem Usage: run.bat [debug|release]

setlocal

set "preset=%~1"
if "%preset%"=="" set "preset=debug"

if /i "%preset%"=="debug" (
    set "build_dir=build"
) else if /i "%preset%"=="release" (
    set "build_dir=build-release"
) else (
    echo Unknown preset '%preset%', expected debug or release 1>&2
    exit /b 1
)

cd /d "%~dp0"

rem We need a compiler, CMake and Ninja. If they are not on the PATH, use the
rem ones bundled with CLion, or else the ones from Visual Studio.
call :has_tools
if errorlevel 1 call :load_clion
if errorlevel 1 call :load_msvc
if errorlevel 1 (
    echo No C++ toolchain found. Install CLion, or Visual Studio with the "Desktop development with C++" workload. 1>&2
    exit /b 1
)

if not exist "%build_dir%\CMakeCache.txt" (
    cmake --preset %preset% || exit /b 1
)
cmake --build --preset %preset% || exit /b 1

"%build_dir%\bin\launcher.exe"
exit /b %errorlevel%


:has_tools
where cmake >nul 2>nul || exit /b 1
where ninja >nul 2>nul || exit /b 1
where cl >nul 2>nul && exit /b 0
where g++ >nul 2>nul && exit /b 0
exit /b 1

:load_clion
set "clion_dir="
for /d %%d in ("%ProgramFiles%\JetBrains\CLion*" "%LOCALAPPDATA%\Programs\CLion*" "%LOCALAPPDATA%\JetBrains\Toolbox\apps\CLion\ch-0\*") do (
    if exist "%%~d\bin\mingw\bin\g++.exe" set "clion_dir=%%~d"
)
if not defined clion_dir exit /b 1
echo Using the toolchain bundled with CLion: %clion_dir%
set "PATH=%clion_dir%\bin\mingw\bin;%clion_dir%\bin\cmake\win\x64\bin;%clion_dir%\bin\ninja\win\x64;%PATH%"
set "CC=gcc"
set "CXX=g++"
goto has_tools

:load_msvc
set "vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%vswhere%" exit /b 1
set "vs_dir="
for /f "usebackq tokens=*" %%i in (`"%vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "vs_dir=%%i"
if not defined vs_dir exit /b 1
echo Using Visual Studio: %vs_dir%
call "%vs_dir%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
goto has_tools
