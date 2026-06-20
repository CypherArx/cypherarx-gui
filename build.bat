@echo off
rem Configure + build the CypherArx GUI (MinGW + Qt + vcpkg).
rem Edit QT / MINGW below if your Qt install differs.
setlocal
set "QT=C:\Qt\6.11.1\mingw_64"
set "MINGW=C:\Qt\Tools\mingw1310_64"
set "PATH=%MINGW%\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;%QT%\bin;%PATH%"
cd /d "%~dp0"
cmake --preset default || exit /b 1
cmake --build --preset default || exit /b 1
echo.
echo Build OK. Run with: run.bat
endlocal
