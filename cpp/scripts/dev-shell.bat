@echo off
REM Enter an MSVC + CMake + Ninja-enabled shell for this project.
REM Used as a one-shot: dev-shell.bat <command...>

call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" >NUL
if errorlevel 1 (
    echo [dev-shell] Failed to invoke vcvars64.bat
    exit /b 1
)

set "PATH=C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;C:\ninja-win;%PATH%"

if "%~1"=="" (
    cmd /k
) else (
    %*
)
