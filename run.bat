@echo off
rem Launch the CypherArx GUI (Dear ImGui; statically linked, no extra runtime PATH needed).
start "" "%~dp0build\cypherarx_gui.exe" %*
