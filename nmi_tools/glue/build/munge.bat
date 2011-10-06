echo %1 %2 %3
echo PATH=%PATH%
echo BASE_DIR=%BASE_DIR%
dir /b/s *.zip
dir /b/s *.msi
dir /b/s *.tar.gz
dir /b/od *
which tar
for %%I in (tar.exe) do echo %%~f$PATH:I

if "~%2"=="~cleanup" cacls "%~dp1*" /T /C /E /G SYSTEM:F 
