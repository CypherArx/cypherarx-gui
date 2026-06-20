@echo off
rem Launch CypherArx GUI with the MinGW + Qt runtime on PATH.
rem Edit QT / MINGW below if your Qt install differs.
setlocal
set "QT=C:\Qt\6.11.1\mingw_64"
set "MINGW=C:\Qt\Tools\mingw1310_64"
set "PATH=%MINGW%\bin;%QT%\bin;%PATH%"
start "" "%~dp0build\cypherarx_gui.exe" %*
endlocal
