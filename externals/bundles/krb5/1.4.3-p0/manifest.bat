echo "Updating the KRB5 Manifests"
mt.exe /manifest comerr32.dll.manifest /outputresource:comerr32.dll;2
mt.exe /manifest gssapi32.dll.manifest /outputresource:gssapi32.dll;2
mt.exe /manifest k5sprt32.dll.manifest /outputresource:k5sprt32.dll;2
mt.exe /manifest krb5_32.dll.manifest /outputresource:krb5_32.dll;2 
mt.exe /manifest xpprof32.dll.manifest /outputresource:xpprof32.dll;2