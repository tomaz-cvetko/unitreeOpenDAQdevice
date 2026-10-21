@echo off
rem Starts the RobotDog openDAQ server. Keep this window open while measuring; press Enter in it to stop.
rem The full path matters: Windows can be set up to not run programs from the current folder.
cd /d "%~dp0bin"
"%~dp0bin\server_example.exe"
if errorlevel 1 pause
