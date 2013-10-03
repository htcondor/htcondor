echo "Updating the KRB5 Manifests in '%1'"
::echo %CD%
::dir
::type comerr32.dll.manifest
if EXIST comerr32.dll.manifest mt.exe /nologo /manifest comerr32.dll.manifest /outputresource:comerr32.dll;2
if EXIST gssapi32.dll.manifest mt.exe /nologo /manifest gssapi32.dll.manifest /outputresource:gssapi32.dll;2
if EXIST k5sprt32.dll.manifest mt.exe /nologo /manifest k5sprt32.dll.manifest /outputresource:k5sprt32.dll;2
if EXIST krb5_32.dll.manifest  mt.exe /nologo /manifest krb5_32.dll.manifest  /outputresource:krb5_32.dll;2
if EXIST xpprof32.dll.manifest mt.exe /nologo /manifest xpprof32.dll.manifest /outputresource:xpprof32.dll;2