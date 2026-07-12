@echo off
call "D:\software\vs2022\installer\VC\Auxiliary\Build\vcvars64.bat"

set VCPKG_ROOT=C:\vcpkg
set QT6_DIR=D:\software\qt6\6.8.3\msvc2022_64
set VCPKG_FORCE_SYSTEM_BINARIES=1

set CMAKE_DIR=D:\software\vs2022\installer\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin
set NINJA_DIR=D:\software\vs2022\installer\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja
set PWSH_DIR=C:\Program Files\PowerShell\7
set SEVENZIP_DIR=C:\Program Files\7-Zip
set PATH=%CMAKE_DIR%;%NINJA_DIR%;%PWSH_DIR%;%SEVENZIP_DIR%;%PATH%

cd /d "D:\Project_vibecoding\NovaChat"
cmake --build --preset windows-debug > D:\Project_vibecoding\NovaChat\build_output.txt 2>&1
echo Exit code: %ERRORLEVEL% >> D:\Project_vibecoding\NovaChat\build_output.txt
