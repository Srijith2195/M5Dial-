@echo off
title M5Dial PC Control Center
cd /d "D:\OneDrive\Desktop\M5Dial-UserDemo"
echo ======================================================================
echo             M5Dial PC Control Center - Companion Server
echo ======================================================================
echo Project Directory: %CD%
echo Web Dashboard    : http://localhost:8765
echo M5Dial Direct IP : http://192.168.137.1:8765
echo M5Dial Office IP : http://192.168.0.25:8765
echo.
echo [1/2] Ensuring Windows 2.4 GHz PC Hotspot (M5Dial_PC) is active...
powershell -ExecutionPolicy Bypass -File "D:\OneDrive\Desktop\M5Dial-UserDemo\pc_companion\enable_hotspot.ps1"
echo.
echo [2/2] Starting Companion Server on port 8765...
echo [IMPORTANT] Keep this window OPEN while using PC Control on M5Dial!
echo [TIP] Press Ctrl+C in this window when you want to stop the server.
echo ======================================================================
echo.
where python >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    python app.py
) else (
    py -3 app.py
)
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Server exited with error code %ERRORLEVEL%.
    pause
)
