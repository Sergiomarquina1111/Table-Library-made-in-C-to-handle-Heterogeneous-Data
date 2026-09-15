@echo off
REM Always builds with the 64-bit MinGW g++ and the matching
REM TableSDK x64 library - no LIBRARY_PATH/PATH dependency.
"C:\mingw64\bin\g++.exe" %* -I"%LOCALAPPDATA%\TableSDK\include" -L"%LOCALAPPDATA%\TableSDK\lib\mingw\x64" -ltable
