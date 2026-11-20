@echo off
rem Launch the CypherArx GUI (its DLLs sit next to the executable in build).
start "" "%~dp0build\cypherarx_gui.exe" %*
