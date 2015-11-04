@echo off
:loop
if "%1"=="" exit
for %%f in (%1) do echo %%f
shift
goto loop
