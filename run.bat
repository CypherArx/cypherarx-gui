@echo off
rem Launch the CypherArx GUI (its DLLs sit next to the executable in build-release).
start "" "%~dp0build-release\cypherarx_gui.exe" %*
