echo %1 %2 %3
echo PATH=%PATH%
echo BASE_DIR=%BASE_DIR%
dir /b/s *.zip
dir /b/s *.msi
dir /b/s *.tar.gz
dir /b/od *
for %%I in (tar.exe mv.exe cp.exe) do echo %%I at %%~f$PATH:I

setlocal
set PUBLIC=%~f1
if "~%2"=="~cleanup" cacls "%~dp1*" /T /C /E /G SYSTEM:F 
if "~%2"=="~cleanup" shift
if NOT "~%2"=="~move" goto :EOF
move msconfig "%PUBLIC%"
move release_dir "%PUBLIC%"
move *.msi "%PUBLIC%"
move *.zip "%PUBLIC%"
