@echo off
set QT6_DIR=D:\software\qt6\6.8.3\msvc2022_64
set PATH=%QT6_DIR%\bin;%PATH%

cd /d D:\Project_vibecoding\NovaChat
build\debug\client\NovaChat.exe
