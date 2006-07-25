
##
## NMAKE file to build the gsoap stubs. This file is NOT machine generated,
## so humans can feel free to edit it as necessary.
##

!ifdef TEMP
TEMPDIR = $(TEMP)\_soap_gen_temp
!else
TEMPDIR = C:\TEMP\_soap_gen_temp
!endif


SRCDIR = $(MAKEDIR)\..\src

!ifdef SOAPCPP2
SOAPCPP = $(SOAPCPP2)
!else
SOAPCPP = soapcpp2.exe
!endif
SOAPCPPFLAGS = -I $(SRCDIR)\condor_daemon_core.V6

DAEMONS = collector dagman gridmanager master negotiator had credd \
		schedd shadow startd starter cgahp cgahp_worker quill \
		replication transferer

all : $(DAEMONS)

gridmanager : $(SRCDIR)\condor_$@\soap_$@Stub.C \
			$(SRCDIR)\condor_$@\gsoap_$@.h
	-2mkdir $(TEMPDIR)
	cd $(SRCDIR)\condor_$@
	$(SOAPCPP) $(SOAPCPPFLAGS) -p soap_$@ -d $(TEMPDIR) gsoap_$@.h
	copy /Y $(TEMPDIR)\soap_$@C.cpp      .\*.C
	copy /Y $(TEMPDIR)\soap_$@Server.cpp .\*.C
	copy /Y $(TEMPDIR)\condor$@.nsmap    .
	copy /Y $(TEMPDIR)\soap_$@H.h        .
	copy /Y $(TEMPDIR)\soap_$@Stub.h     .
	copy /Y $(TEMPDIR)\condor$@.wsdl     .
	rd /q /s $(TEMPDIR) > NUL 2>&1

dagman : $(SRCDIR)\condor_$@\soap_$@Stub.C \
			$(SRCDIR)\condor_$@\gsoap_$@.h
	-2mkdir $(TEMPDIR)
	cd $(SRCDIR)\condor_$@
	$(SOAPCPP) $(SOAPCPPFLAGS) -p soap_$@ -d $(TEMPDIR) gsoap_$@.h
	copy /Y $(TEMPDIR)\soap_$@C.cpp      .\*.C
	copy /Y $(TEMPDIR)\soap_$@Server.cpp .\*.C
	copy /Y $(TEMPDIR)\condor$@.nsmap    .
	copy /Y $(TEMPDIR)\soap_$@H.h        .
	copy /Y $(TEMPDIR)\soap_$@Stub.h     .
	copy /Y $(TEMPDIR)\condor$@.wsdl     .
	rd /q /s $(TEMPDIR) > NUL 2>&1

starter : $(SRCDIR)\condor_$@.V6.1\soap_$@Stub.C \
			$(SRCDIR)\condor_$@.V6.1\gsoap_$@.h
	-2mkdir $(TEMPDIR)
	cd $(SRCDIR)\condor_$@.V6.1
	$(SOAPCPP) $(SOAPCPPFLAGS) -p soap_$@ -d $(TEMPDIR) gsoap_$@.h
	copy /Y $(TEMPDIR)\soap_$@C.cpp      .\*.C
	copy /Y $(TEMPDIR)\soap_$@Server.cpp .\*.C
	copy /Y $(TEMPDIR)\condor$@.nsmap    .
	copy /Y $(TEMPDIR)\soap_$@H.h        .
	copy /Y $(TEMPDIR)\soap_$@Stub.h     .
	copy /Y $(TEMPDIR)\condor$@.wsdl     .
	rd /q /s $(TEMPDIR) > NUL 2>&1

shadow : $(SRCDIR)\condor_$@.V6.1\soap_$@Stub.C \
			$(SRCDIR)\condor_$@.V6.1\gsoap_$@.h
	-2mkdir $(TEMPDIR)
	cd $(SRCDIR)\condor_$@.V6.1
	$(SOAPCPP) $(SOAPCPPFLAGS) -p soap_$@ -d $(TEMPDIR) gsoap_$@.h
	copy /Y $(TEMPDIR)\soap_$@C.cpp      .\*.C
	copy /Y $(TEMPDIR)\soap_$@Server.cpp .\*.C
	copy /Y $(TEMPDIR)\condor$@.nsmap    .
	copy /Y $(TEMPDIR)\soap_$@H.h        .
	copy /Y $(TEMPDIR)\soap_$@Stub.h     .
	copy /Y $(TEMPDIR)\condor$@.wsdl     .
	rd /q /s $(TEMPDIR) > NUL 2>&1

startd : $(SRCDIR)\condor_$@.V6\soap_$@Stub.C \
			$(SRCDIR)\condor_$@.V6\gsoap_$@.h
	-2mkdir $(TEMPDIR)
	cd $(SRCDIR)\condor_$@.V6
	$(SOAPCPP) $(SOAPCPPFLAGS) -p soap_$@ -d $(TEMPDIR) gsoap_$@.h
	copy /Y $(TEMPDIR)\soap_$@C.cpp      .\*.C
	copy /Y $(TEMPDIR)\soap_$@Server.cpp .\*.C
	copy /Y $(TEMPDIR)\condor$@.nsmap    .
	copy /Y $(TEMPDIR)\soap_$@H.h        .
	copy /Y $(TEMPDIR)\soap_$@Stub.h     .
	copy /Y $(TEMPDIR)\condor$@.wsdl     .
	rd /q /s $(TEMPDIR) > NUL 2>&1

collector : $(SRCDIR)\condor_$@.V6\soap_$@Stub.C \
			$(SRCDIR)\condor_$@.V6\gsoap_$@.h
	-2mkdir $(TEMPDIR)
	cd $(SRCDIR)\condor_$@.V6
	$(SOAPCPP) $(SOAPCPPFLAGS) -p soap_$@ -d $(TEMPDIR) gsoap_$@.h
	copy /Y $(TEMPDIR)\soap_$@C.cpp      .\*.C
	copy /Y $(TEMPDIR)\soap_$@Server.cpp .\*.C
	copy /Y $(TEMPDIR)\condor$@.nsmap    .
	copy /Y $(TEMPDIR)\soap_$@H.h        .
	copy /Y $(TEMPDIR)\soap_$@Stub.h     .
	copy /Y $(TEMPDIR)\condor$@.wsdl     .
	rd /q /s $(TEMPDIR) > NUL 2>&1

schedd : $(SRCDIR)\condor_$@.V6\soap_$@Stub.C \
			$(SRCDIR)\condor_$@.V6\gsoap_$@.h
	-2mkdir $(TEMPDIR)
	cd $(SRCDIR)\condor_$@.V6
	$(SOAPCPP) $(SOAPCPPFLAGS) -p soap_$@ -d $(TEMPDIR) gsoap_$@.h
	copy /Y $(TEMPDIR)\soap_$@C.cpp      .\*.C
	copy /Y $(TEMPDIR)\soap_$@Server.cpp .\*.C
	copy /Y $(TEMPDIR)\condor$@.nsmap    .
	copy /Y $(TEMPDIR)\soap_$@H.h        .
	copy /Y $(TEMPDIR)\soap_$@Stub.h     .
	copy /Y $(TEMPDIR)\condor$@.wsdl     .
	rd /q /s $(TEMPDIR) > NUL 2>&1

negotiator : $(SRCDIR)\condor_$@.V6\soap_$@Stub.C \
			$(SRCDIR)\condor_$@.V6\gsoap_$@.h
	-2mkdir $(TEMPDIR)
	cd $(SRCDIR)\condor_$@.V6
	$(SOAPCPP) $(SOAPCPPFLAGS) -p soap_$@ -d $(TEMPDIR) gsoap_$@.h
	copy /Y $(TEMPDIR)\soap_$@C.cpp      .\*.C
	copy /Y $(TEMPDIR)\soap_$@Server.cpp .\*.C
	copy /Y $(TEMPDIR)\condor$@.nsmap    .
	copy /Y $(TEMPDIR)\soap_$@H.h        .
	copy /Y $(TEMPDIR)\soap_$@Stub.h     .
	copy /Y $(TEMPDIR)\condor$@.wsdl     .
	rd /q /s $(TEMPDIR) > NUL 2>&1

master : $(SRCDIR)\condor_$@.V6\soap_$@Stub.C \
			$(SRCDIR)\condor_$@.V6\gsoap_$@.h
	-2mkdir $(TEMPDIR)
	cd $(SRCDIR)\condor_$@.V6
	$(SOAPCPP) $(SOAPCPPFLAGS) -p soap_$@ -d $(TEMPDIR) gsoap_$@.h
	copy /Y $(TEMPDIR)\soap_$@C.cpp      .\*.C
	copy /Y $(TEMPDIR)\soap_$@Server.cpp .\*.C
	copy /Y $(TEMPDIR)\condor$@.nsmap    .
	copy /Y $(TEMPDIR)\soap_$@H.h        .
	copy /Y $(TEMPDIR)\soap_$@Stub.h     .
	copy /Y $(TEMPDIR)\condor$@.wsdl     .
	rd /q /s $(TEMPDIR) > NUL 2>&1

cgahp : $(SRCDIR)\condor_c-gahp\soap_$@Stub.C \
			$(SRCDIR)\condor_c-gahp\gsoap_$@.h
	-2mkdir $(TEMPDIR)
	cd $(SRCDIR)\condor_c-gahp
	$(SOAPCPP) $(SOAPCPPFLAGS) -p soap_$@ -d $(TEMPDIR) gsoap_$@.h
	copy /Y $(TEMPDIR)\soap_$@C.cpp      .\*.C
	copy /Y $(TEMPDIR)\soap_$@Server.cpp .\*.C
	copy /Y $(TEMPDIR)\condor$@.nsmap    .
	copy /Y $(TEMPDIR)\soap_$@H.h        .
	copy /Y $(TEMPDIR)\soap_$@Stub.h     .
	copy /Y $(TEMPDIR)\condor$@.wsdl     .
	rd /q /s $(TEMPDIR) > NUL 2>&1

cgahp_worker : $(SRCDIR)\condor_c-gahp\soap_$@Stub.C \
			$(SRCDIR)\condor_c-gahp\gsoap_$@.h
	-2mkdir $(TEMPDIR)
	cd $(SRCDIR)\condor_c-gahp
	$(SOAPCPP) $(SOAPCPPFLAGS) -p soap_$@ -d $(TEMPDIR) gsoap_$@.h
	copy /Y $(TEMPDIR)\soap_$@C.cpp      .\*.C
	copy /Y $(TEMPDIR)\soap_$@Server.cpp .\*.C
	copy /Y $(TEMPDIR)\condor$@.nsmap    .
	copy /Y $(TEMPDIR)\soap_$@H.h        .
	copy /Y $(TEMPDIR)\soap_$@Stub.h     .
	copy /Y $(TEMPDIR)\condor$@.wsdl     .
	rd /q /s $(TEMPDIR) > NUL 2>&1

quill : $(SRCDIR)\condor_$@\soap_$@Stub.C \
			$(SRCDIR)\condor_$@\gsoap_$@.h
	-2mkdir $(TEMPDIR)
	cd $(SRCDIR)\condor_$@
	$(SOAPCPP) $(SOAPCPPFLAGS) -p soap_$@ -d $(TEMPDIR) gsoap_$@.h
	copy /Y $(TEMPDIR)\soap_$@C.cpp      .\*.C
	copy /Y $(TEMPDIR)\soap_$@Server.cpp .\*.C
	copy /Y $(TEMPDIR)\condor$@.nsmap    .
	copy /Y $(TEMPDIR)\soap_$@H.h        .
	copy /Y $(TEMPDIR)\soap_$@Stub.h     .
	copy /Y $(TEMPDIR)\condor$@.wsdl     .
	rd /q /s $(TEMPDIR) > NUL 2>&1

credd : $(SRCDIR)\condor_$@\soap_$@Stub.C \
			$(SRCDIR)\condor_$@\gsoap_$@.h
	-2mkdir $(TEMPDIR)
	cd $(SRCDIR)\condor_$@
	$(SOAPCPP) $(SOAPCPPFLAGS) -p soap_$@ -d $(TEMPDIR) gsoap_$@.h
	copy /Y $(TEMPDIR)\soap_$@C.cpp      .\*.C
	copy /Y $(TEMPDIR)\soap_$@Server.cpp .\*.C
	copy /Y $(TEMPDIR)\condor$@.nsmap    .
	copy /Y $(TEMPDIR)\soap_$@H.h        .
	copy /Y $(TEMPDIR)\soap_$@Stub.h     .
	copy /Y $(TEMPDIR)\condor$@.wsdl     .
	rd /q /s $(TEMPDIR) > NUL 2>&1

had : $(SRCDIR)\condor_$@\soap_$@Stub.C \
			$(SRCDIR)\condor_$@\gsoap_$@.h
	-2mkdir $(TEMPDIR)
	cd $(SRCDIR)\condor_$@
	$(SOAPCPP) $(SOAPCPPFLAGS) -p soap_$@ -d $(TEMPDIR) gsoap_$@.h
	copy /Y $(TEMPDIR)\soap_$@C.cpp      .\*.C
	copy /Y $(TEMPDIR)\soap_$@Server.cpp .\*.C
	copy /Y $(TEMPDIR)\condor$@.nsmap    .
	copy /Y $(TEMPDIR)\soap_$@H.h        .
	copy /Y $(TEMPDIR)\soap_$@Stub.h     .
	copy /Y $(TEMPDIR)\condor$@.wsdl     .
	rd /q /s $(TEMPDIR) > NUL 2>&1

replication : $(SRCDIR)\condor_had\soap_$@Stub.C \
			$(SRCDIR)\condor_had\gsoap_$@.h
	-2mkdir $(TEMPDIR)
	cd $(SRCDIR)\condor_had
	$(SOAPCPP) $(SOAPCPPFLAGS) -p soap_$@ -d $(TEMPDIR) gsoap_$@.h
	copy /Y $(TEMPDIR)\soap_$@C.cpp      .\*.C
	copy /Y $(TEMPDIR)\soap_$@Server.cpp .\*.C
	copy /Y $(TEMPDIR)\condor$@.nsmap    .
	copy /Y $(TEMPDIR)\soap_$@H.h        .
	copy /Y $(TEMPDIR)\soap_$@Stub.h     .
	copy /Y $(TEMPDIR)\condor$@.wsdl     .
	rd /q /s $(TEMPDIR) > NUL 2>&1

transferer : $(SRCDIR)\condor_had\soap_$@Stub.C \
			$(SRCDIR)\condor_had\gsoap_$@.h
	-2mkdir $(TEMPDIR)
	cd $(SRCDIR)\condor_had
	$(SOAPCPP) $(SOAPCPPFLAGS) -p soap_$@ -d $(TEMPDIR) gsoap_$@.h
	copy /Y $(TEMPDIR)\soap_$@C.cpp      .\*.C
	copy /Y $(TEMPDIR)\soap_$@Server.cpp .\*.C
	copy /Y $(TEMPDIR)\condor$@.nsmap    .
	copy /Y $(TEMPDIR)\soap_$@H.h        .
	copy /Y $(TEMPDIR)\soap_$@Stub.h     .
	copy /Y $(TEMPDIR)\condor$@.wsdl     .
	rd /q /s $(TEMPDIR) > NUL 2>&1

