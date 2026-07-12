@echo off
call "D:\software\vs2022\installer\VC\Auxiliary\Build\vcvars64.bat"

rem Override VCPKG_ROOT to standalone vcpkg (vcvars64 sets VS built-in vcpkg)
set VCPKG_ROOT=C:\vcpkg

rem Override QT6_DIR for this machine
set QT6_DIR=D:\software\qt6\6.8.3\msvc2022_64

rem Use system cmake/ninja/pwsh instead of letting vcpkg download its own
set VCPKG_FORCE_SYSTEM_BINARIES=1

rem Prepend VS2022 cmake, ninja and PowerShell 7 to PATH
set CMAKE_DIR=D:\software\vs2022\installer\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin
set NINJA_DIR=D:\software\vs2022\installer\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja
set PWSH_DIR=C:\Program Files\PowerShell\7
set SEVENZIP_DIR=C:\Program Files\7-Zip
set PATH=%CMAKE_DIR%;%NINJA_DIR%;%PWSH_DIR%;%SEVENZIP_DIR%;%PATH%

cd /d "D:\Project_vibecoding\NovaChat"

rem Remove stale build cache
if exist build\debug rmdir /s /q build\debug

cmake --preset windows-debug ^
    -DCMAKE_MAKE_PROGRAM="%NINJA_DIR%\ninja.exe" ^
    > D:\Project_vibecoding\NovaChat\build_configure_output.txt 2>&1

echo Exit code: %ERRORLEVEL% >> D:\Project_vibecoding\NovaChat\build_configure_output.txt
