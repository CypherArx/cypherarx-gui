@echo off
rem Configure + build the CypherArx GUI (lamina-ui, MSVC + vcpkg).
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
set "VCPKG_ROOT=C:\dev\vcpkg"
set "PATH=C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;%PATH%"
cd /d "%~dp0"
cmake --preset release || exit /b 1
cmake --build --preset release || exit /b 1
echo.
echo Build OK. Run with: run.bat
endlocal
