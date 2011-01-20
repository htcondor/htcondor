echo %1 %2 %3
echo PATH=%PATH%
dir /b/s *.zip
dir /b/s *.msi
dir /b/s *.tar.gz
which tar
for %%I in (tar.exe) do echo %%~f$PATH:I
