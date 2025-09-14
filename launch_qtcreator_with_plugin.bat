@echo off
REM Launch Qt Creator with MCP Plugin loaded
REM This is the Windows equivalent of launch_qtcreator_with_plugin.sh

REM Set the plugin path to the user's Qt Creator plugins directory
set PLUGIN_PATH=%LOCALAPPDATA%\QtProject\qtcreator\plugins

REM Try to find Qt Creator in common installation locations
set QTCREATOR_PATH=

REM Check common Qt Creator installation paths
if exist "C:\Qt\Qt Creator\6.9.2\msvc2022_64\bin\qtcreator.exe" (
    set QTCREATOR_PATH=C:\Qt\Qt Creator\6.9.2\msvc2022_64\bin\qtcreator.exe
) else if exist "C:\Program Files\Qt\Qt Creator\6.9.2\msvc2022_64\bin\qtcreator.exe" (
    set QTCREATOR_PATH=C:\Program Files\Qt\Qt Creator\6.9.2\msvc2022_64\bin\qtcreator.exe
) else if exist "C:\Qt\Qt Creator\6.9.2\mingw_64\bin\qtcreator.exe" (
    set QTCREATOR_PATH=C:\Qt\Qt Creator\6.9.2\mingw_64\bin\qtcreator.exe
) else if exist "C:\Program Files\Qt\Qt Creator\6.9.2\mingw_64\bin\qtcreator.exe" (
    set QTCREATOR_PATH=C:\Program Files\Qt\Qt Creator\6.9.2\mingw_64\bin\qtcreator.exe
) else (
    echo ERROR: Qt Creator not found in common installation locations
    echo Please ensure Qt Creator is installed and update this script with the correct path
    echo Common locations:
    echo   C:\Qt\Qt Creator\6.9.2\msvc2022_64\bin\qtcreator.exe
    echo   C:\Program Files\Qt\Qt Creator\6.9.2\msvc2022_64\bin\qtcreator.exe
    echo   C:\Qt\Qt Creator\6.9.2\mingw_64\bin\qtcreator.exe
    echo   C:\Program Files\Qt\Qt Creator\6.9.2\mingw_64\bin\qtcreator.exe
    pause
    exit /b 1
)

echo Starting Qt Creator with MCP Plugin...
echo Qt Creator path: %QTCREATOR_PATH%
echo Plugin path: %PLUGIN_PATH%

REM Launch Qt Creator with the plugin path
start "" "%QTCREATOR_PATH%" -pluginpath "%PLUGIN_PATH%"

echo Qt Creator launched successfully!
echo The MCP Plugin should be loaded and available on port 3001 (or next available port)

