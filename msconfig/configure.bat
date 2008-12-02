@echo off
REM Copy the static Windows configration header file into place
copy /y ..\src\condor_includes\config.WINDOWS.h ..\src\condor_includes\config.h
