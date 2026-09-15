@echo off
REM Always builds with the 32-bit MinGW g++ and the matching
REM TableSDK x86 library - no LIBRARY_PATH/PATH dependency.
"C:\MinGW\bin\g++.exe" %* -I"%LOCALAPPDATA%\TableSDK\include" -L"%LOCALAPPDATA%\TableSDK\lib\mingw\x86" -ltable
