
##
## NMAKE file to build the gsoap stubs. This file is NOT machine generated,
## so humans can feel free to edit it as necessary.
##

TEMPDIR = $(MAKEDIR)\_soap_gen_temp

SRCDIR = $(MAKEDIR)\..\src

!ifdef SOAPCPP2
SOAPCPP = $(SOAPCPP2)
!else
SOAPCPP = soapcpp2.exe
!endif
SOAPCPPFLAGS = -I $(SRCDIR)\condor_daemon_core.V6

DAEMONS = collector schedd 

all : $(DAEMONS)

gridmanager : $(SRCDIR)\condor_$@\soap_$@Stub.cpp \
			$(SRCDIR)\condor_$@\gsoap_$@.h
	-2mkdir $(TEMPDIR)
	cd $(SRCDIR)\condor_$@
	$(SOAPCPP) $(SOAPCPPFLAGS) -p soap_$@ -d $(TEMPDIR) gsoap_$@.h
	copy /Y $(TEMPDIR)\soap_$@C.cpp      .
	copy /Y $(TEMPDIR)\soap_$@Server.cpp .
	copy /Y $(TEMPDIR)\condor$@.nsmap    .
	copy /Y $(TEMPDIR)\soap_$@H.h        .
	copy /Y $(TEMPDIR)\soap_$@Stub.h     .
	copy /Y $(TEMPDIR)\condor$@.wsdl     .
	rd /q /s $(TEMPDIR) > NUL 2>&1

dagman : $(SRCDIR)\condor_$@\soap_$@Stub.cpp \
			$(SRCDIR)\condor_$@\gsoap_$@.h
	-2mkdir $(TEMPDIR)
	cd $(SRCDIR)\condor_$@
	$(SOAPCPP) $(SOAPCPPFLAGS) -p soap_$@ -d $(TEMPDIR) gsoap_$@.h
	copy /Y $(TEMPDIR)\soap_$@C.cpp      .
	copy /Y $(TEMPDIR)\soap_$@Server.cpp .
	copy /Y $(TEMPDIR)\condor$@.nsmap    .
	copy /Y $(TEMPDIR)\soap_$@H.h        .
	copy /Y $(TEMPDIR)\soap_$@Stub.h     .
	copy /Y $(TEMPDIR)\condor$@.wsdl     .
	rd /q /s $(TEMPDIR) > NUL 2>&1

starter : $(SRCDIR)\condor_$@.V6.1\soap_$@Stub.cpp \
			$(SRCDIR)\condor_$@.V6.1\gsoap_$@.h
	-2mkdir $(TEMPDIR)
	cd $(SRCDIR)\condor_$@.V6.1
	$(SOAPCPP) $(SOAPCPPFLAGS) -p soap_$@ -d $(TEMPDIR) gsoap_$@.h
	copy /Y $(TEMPDIR)\soap_$@C.cpp      .
	copy /Y $(TEMPDIR)\soap_$@Server.cpp .
	copy /Y $(TEMPDIR)\condor$@.nsmap    .
	copy /Y $(TEMPDIR)\soap_$@H.h        .
	copy /Y $(TEMPDIR)\soap_$@Stub.h     .
	copy /Y $(TEMPDIR)\condor$@.wsdl     .
	rd /q /s $(TEMPDIR) > NUL 2>&1

shadow : $(SRCDIR)\condor_$@.V6.1\soap_$@Stub.cpp \
			$(SRCDIR)\condor_$@.V6.1\gsoap_$@.h
	-2mkdir $(TEMPDIR)
	cd $(SRCDIR)\condor_$@.V6.1
	$(SOAPCPP) $(SOAPCPPFLAGS) -p soap_$@ -d $(TEMPDIR) gsoap_$@.h
	copy /Y $(TEMPDIR)\soap_$@C.cpp      .
	copy /Y $(TEMPDIR)\soap_$@Server.cpp .
	copy /Y $(TEMPDIR)\condor$@.nsmap    .
	copy /Y $(TEMPDIR)\soap_$@H.h        .
	copy /Y $(TEMPDIR)\soap_$@Stub.h     .
	copy /Y $(TEMPDIR)\condor$@.wsdl     .
	rd /q /s $(TEMPDIR) > NUL 2>&1

startd : $(SRCDIR)\condor_$@.V6\soap_$@Stub.cpp \
			$(SRCDIR)\condor_$@.V6\gsoap_$@.h
	-2mkdir $(TEMPDIR)
	cd $(SRCDIR)\condor_$@.V6
	$(SOAPCPP) $(SOAPCPPFLAGS) -p soap_$@ -d $(TEMPDIR) gsoap_$@.h
	copy /Y $(TEMPDIR)\soap_$@C.cpp      .
	copy /Y $(TEMPDIR)\soap_$@Server.cpp .
	copy /Y $(TEMPDIR)\condor$@.nsmap    .
	copy /Y $(TEMPDIR)\soap_$@H.h        .
	copy /Y $(TEMPDIR)\soap_$@Stub.h     .
	copy /Y $(TEMPDIR)\condor$@.wsdl     .
	rd /q /s $(TEMPDIR) > NUL 2>&1

collector : $(SRCDIR)\condor_$@.V6\soap_$@Stub.cpp \
			$(SRCDIR)\condor_$@.V6\gsoap_$@.h
	-2mkdir $(TEMPDIR)
	cd $(SRCDIR)\condor_$@.V6
	$(SOAPCPP) $(SOAPCPPFLAGS) -p soap_$@ -d $(TEMPDIR) gsoap_$@.h
	copy /Y $(TEMPDIR)\soap_$@C.cpp      .
	copy /Y $(TEMPDIR)\soap_$@Server.cpp .
	copy /Y $(TEMPDIR)\condor$@.nsmap    .
	copy /Y $(TEMPDIR)\soap_$@H.h        .
	copy /Y $(TEMPDIR)\soap_$@Stub.h     .
	copy /Y $(TEMPDIR)\condor$@.wsdl     .
	rd /q /s $(TEMPDIR) > NUL 2>&1

schedd : $(SRCDIR)\condor_$@.V6\soap_$@Stub.cpp \
			$(SRCDIR)\condor_$@.V6\gsoap_$@.h
	-2mkdir $(TEMPDIR)
	cd $(SRCDIR)\condor_$@.V6
	$(SOAPCPP) $(SOAPCPPFLAGS) -p soap_$@ -d $(TEMPDIR) gsoap_$@.h
	copy /Y $(TEMPDIR)\soap_$@C.cpp      .
	copy /Y $(TEMPDIR)\soap_$@Server.cpp .
	copy /Y $(TEMPDIR)\condor$@.nsmap    .
	copy /Y $(TEMPDIR)\soap_$@H.h        .
	copy /Y $(TEMPDIR)\soap_$@Stub.h     .
	copy /Y $(TEMPDIR)\condor$@.wsdl     .
	rd /q /s $(TEMPDIR) > NUL 2>&1

negotiator : $(SRCDIR)\condor_$@.V6\soap_$@Stub.cpp \
			$(SRCDIR)\condor_$@.V6\gsoap_$@.h
	-2mkdir $(TEMPDIR)
	cd $(SRCDIR)\condor_$@.V6
	$(SOAPCPP) $(SOAPCPPFLAGS) -p soap_$@ -d $(TEMPDIR) gsoap_$@.h
	copy /Y $(TEMPDIR)\soap_$@C.cpp      .
	copy /Y $(TEMPDIR)\soap_$@Server.cpp .
	copy /Y $(TEMPDIR)\condor$@.nsmap    .
	copy /Y $(TEMPDIR)\soap_$@H.h        .
	copy /Y $(TEMPDIR)\soap_$@Stub.h     .
	copy /Y $(TEMPDIR)\condor$@.wsdl     .
	rd /q /s $(TEMPDIR) > NUL 2>&1

master : $(SRCDIR)\condor_$@.V6\soap_$@Stub.cpp \
			$(SRCDIR)\condor_$@.V6\gsoap_$@.h
	-2mkdir $(TEMPDIR)
	cd $(SRCDIR)\condor_$@.V6
	$(SOAPCPP) $(SOAPCPPFLAGS) -p soap_$@ -d $(TEMPDIR) gsoap_$@.h
	copy /Y $(TEMPDIR)\soap_$@C.cpp      .
	copy /Y $(TEMPDIR)\soap_$@Server.cpp .
	copy /Y $(TEMPDIR)\condor$@.nsmap    .
	copy /Y $(TEMPDIR)\soap_$@H.h        .
	copy /Y $(TEMPDIR)\soap_$@Stub.h     .
	copy /Y $(TEMPDIR)\condor$@.wsdl     .
	rd /q /s $(TEMPDIR) > NUL 2>&1

cgahp : $(SRCDIR)\condor_c-gahp\soap_$@Stub.cpp \
			$(SRCDIR)\condor_c-gahp\gsoap_$@.h
	-2mkdir $(TEMPDIR)
	cd $(SRCDIR)\condor_c-gahp
	$(SOAPCPP) $(SOAPCPPFLAGS) -p soap_$@ -d $(TEMPDIR) gsoap_$@.h
	copy /Y $(TEMPDIR)\soap_$@C.cpp      .
	copy /Y $(TEMPDIR)\soap_$@Server.cpp .
	copy /Y $(TEMPDIR)\condor$@.nsmap    .
	copy /Y $(TEMPDIR)\soap_$@H.h        .
	copy /Y $(TEMPDIR)\soap_$@Stub.h     .
	copy /Y $(TEMPDIR)\condor$@.wsdl     .
	rd /q /s $(TEMPDIR) > NUL 2>&1

cgahp_worker : $(SRCDIR)\condor_c-gahp\soap_$@Stub.cpp \
			$(SRCDIR)\condor_c-gahp\gsoap_$@.h
	-2mkdir $(TEMPDIR)
	cd $(SRCDIR)\condor_c-gahp
	$(SOAPCPP) $(SOAPCPPFLAGS) -p soap_$@ -d $(TEMPDIR) gsoap_$@.h
	copy /Y $(TEMPDIR)\soap_$@C.cpp      .
	copy /Y $(TEMPDIR)\soap_$@Server.cpp .
	copy /Y $(TEMPDIR)\condor$@.nsmap    .
	copy /Y $(TEMPDIR)\soap_$@H.h        .
	copy /Y $(TEMPDIR)\soap_$@Stub.h     .
	copy /Y $(TEMPDIR)\condor$@.wsdl     .
	rd /q /s $(TEMPDIR) > NUL 2>&1

quill : $(SRCDIR)\condor_$@\soap_$@Stub.cpp \
			$(SRCDIR)\condor_$@\gsoap_$@.h
	-2mkdir $(TEMPDIR)
	cd $(SRCDIR)\condor_$@
	$(SOAPCPP) $(SOAPCPPFLAGS) -p soap_$@ -d $(TEMPDIR) gsoap_$@.h
	copy /Y $(TEMPDIR)\soap_$@C.cpp      .
	copy /Y $(TEMPDIR)\soap_$@Server.cpp .
	copy /Y $(TEMPDIR)\condor$@.nsmap    .
	copy /Y $(TEMPDIR)\soap_$@H.h        .
	copy /Y $(TEMPDIR)\soap_$@Stub.h     .
	copy /Y $(TEMPDIR)\condor$@.wsdl     .
	rd /q /s $(TEMPDIR) > NUL 2>&1

credd : $(SRCDIR)\condor_$@\soap_$@Stub.cpp \
			$(SRCDIR)\condor_$@\gsoap_$@.h
	-2mkdir $(TEMPDIR)
	cd $(SRCDIR)\condor_$@
	$(SOAPCPP) $(SOAPCPPFLAGS) -p soap_$@ -d $(TEMPDIR) gsoap_$@.h
	copy /Y $(TEMPDIR)\soap_$@C.cpp      .
	copy /Y $(TEMPDIR)\soap_$@Server.cpp .
	copy /Y $(TEMPDIR)\condor$@.nsmap    .
	copy /Y $(TEMPDIR)\soap_$@H.h        .
	copy /Y $(TEMPDIR)\soap_$@Stub.h     .
	copy /Y $(TEMPDIR)\condor$@.wsdl     .
	rd /q /s $(TEMPDIR) > NUL 2>&1

had : $(SRCDIR)\condor_$@\soap_$@Stub.cpp \
			$(SRCDIR)\condor_$@\gsoap_$@.h
	-2mkdir $(TEMPDIR)
	cd $(SRCDIR)\condor_$@
	$(SOAPCPP) $(SOAPCPPFLAGS) -p soap_$@ -d $(TEMPDIR) gsoap_$@.h
	copy /Y $(TEMPDIR)\soap_$@C.cpp      .
	copy /Y $(TEMPDIR)\soap_$@Server.cpp .
	copy /Y $(TEMPDIR)\condor$@.nsmap    .
	copy /Y $(TEMPDIR)\soap_$@H.h        .
	copy /Y $(TEMPDIR)\soap_$@Stub.h     .
	copy /Y $(TEMPDIR)\condor$@.wsdl     .
	rd /q /s $(TEMPDIR) > NUL 2>&1

replication : $(SRCDIR)\condor_had\soap_$@Stub.cpp \
			$(SRCDIR)\condor_had\gsoap_$@.h
	-2mkdir $(TEMPDIR)
	cd $(SRCDIR)\condor_had
	$(SOAPCPP) $(SOAPCPPFLAGS) -p soap_$@ -d $(TEMPDIR) gsoap_$@.h
	copy /Y $(TEMPDIR)\soap_$@C.cpp      .
	copy /Y $(TEMPDIR)\soap_$@Server.cpp .
	copy /Y $(TEMPDIR)\condor$@.nsmap    .
	copy /Y $(TEMPDIR)\soap_$@H.h        .
	copy /Y $(TEMPDIR)\soap_$@Stub.h     .
	copy /Y $(TEMPDIR)\condor$@.wsdl     .
	rd /q /s $(TEMPDIR) > NUL 2>&1

transferer : $(SRCDIR)\condor_had\soap_$@Stub.cpp \
			$(SRCDIR)\condor_had\gsoap_$@.h
	-2mkdir $(TEMPDIR)
	cd $(SRCDIR)\condor_had
	$(SOAPCPP) $(SOAPCPPFLAGS) -p soap_$@ -d $(TEMPDIR) gsoap_$@.h
	copy /Y $(TEMPDIR)\soap_$@C.cpp      .
	copy /Y $(TEMPDIR)\soap_$@Server.cpp .
	copy /Y $(TEMPDIR)\condor$@.nsmap    .
	copy /Y $(TEMPDIR)\soap_$@H.h        .
	copy /Y $(TEMPDIR)\soap_$@Stub.h     .
	copy /Y $(TEMPDIR)\condor$@.wsdl     .
	rd /q /s $(TEMPDIR) > NUL 2>&1

vmgahp : $(SRCDIR)\condor_vm-gahp\soap_$@Stub.cpp \
			$(SRCDIR)\condor_vm-gahp\gsoap_$@.h
	-2mkdir $(TEMPDIR)
	cd $(SRCDIR)\condor_vm-gahp
	$(SOAPCPP) $(SOAPCPPFLAGS) -p soap_$@ -d $(TEMPDIR) gsoap_$@.h
	copy /Y $(TEMPDIR)\soap_$@C.cpp      .
	copy /Y $(TEMPDIR)\soap_$@Server.cpp .
	copy /Y $(TEMPDIR)\condor$@.nsmap    .
	copy /Y $(TEMPDIR)\soap_$@H.h        .
	copy /Y $(TEMPDIR)\soap_$@Stub.h     .
	copy /Y $(TEMPDIR)\condor$@.wsdl     .
	rd /q /s $(TEMPDIR) > NUL 2>&1

dbmsd : $(SRCDIR)\condor_dbmsd\soap_$@Stub.cpp \
			$(SRCDIR)\condor_dbmsd\gsoap_$@.h
	-2mkdir $(TEMPDIR)
	cd $(SRCDIR)\condor_dbmsd
	$(SOAPCPP) $(SOAPCPPFLAGS) -p soap_$@ -d $(TEMPDIR) gsoap_$@.h
	copy /Y $(TEMPDIR)\soap_$@C.cpp      .
	copy /Y $(TEMPDIR)\soap_$@Server.cpp .
	copy /Y $(TEMPDIR)\condor$@.nsmap    .
	copy /Y $(TEMPDIR)\soap_$@H.h        .
	copy /Y $(TEMPDIR)\soap_$@Stub.h     .
	copy /Y $(TEMPDIR)\condor$@.wsdl     .
	rd /q /s $(TEMPDIR) > NUL 2>&1

hdfs : $(SRCDIR)\condor_hdfs\soap_$@Stub.cpp \
			$(SRCDIR)\condor_hdfs\gsoap_$@.h
	-2mkdir $(TEMPDIR)
	cd $(SRCDIR)\condor_hdfs
	$(SOAPCPP) $(SOAPCPPFLAGS) -p soap_$@ -d $(TEMPDIR) gsoap_$@.h
	copy /Y $(TEMPDIR)\soap_$@C.cpp      .
	copy /Y $(TEMPDIR)\soap_$@Server.cpp .
	copy /Y $(TEMPDIR)\condor$@.nsmap    .
	copy /Y $(TEMPDIR)\soap_$@H.h        .
	copy /Y $(TEMPDIR)\soap_$@Stub.h     .
	copy /Y $(TEMPDIR)\condor$@.wsdl     .
	rd /q /s $(TEMPDIR) > NUL 2>&1

rooster : $(SRCDIR)\condor_power\soap_$@Stub.cpp \
			$(SRCDIR)\condor_power\gsoap_$@.h
	-2mkdir $(TEMPDIR)
	cd $(SRCDIR)\condor_power
	$(SOAPCPP) $(SOAPCPPFLAGS) -p soap_$@ -d $(TEMPDIR) gsoap_$@.h
	copy /Y $(TEMPDIR)\soap_$@C.cpp      .
	copy /Y $(TEMPDIR)\soap_$@Server.cpp .
	copy /Y $(TEMPDIR)\condor$@.nsmap    .
	copy /Y $(TEMPDIR)\soap_$@H.h        .
	copy /Y $(TEMPDIR)\soap_$@Stub.h     .
	copy /Y $(TEMPDIR)\condor$@.wsdl     .
	rd /q /s $(TEMPDIR) > NUL 2>&1

kbdd : $(SRCDIR)\condor_kbdd\soap_$@Stub.cpp \
			$(SRCDIR)\condor_kbdd\gsoap_$@.h
	-2mkdir $(TEMPDIR)
	cd $(SRCDIR)\condor_kbdd
	$(SOAPCPP) $(SOAPCPPFLAGS) -p soap_$@ -d $(TEMPDIR) gsoap_$@.h
	copy /Y $(TEMPDIR)\soap_$@C.cpp      .
	copy /Y $(TEMPDIR)\soap_$@Server.cpp .
	copy /Y $(TEMPDIR)\condor$@.nsmap    .
	copy /Y $(TEMPDIR)\soap_$@H.h        .
	copy /Y $(TEMPDIR)\soap_$@Stub.h     .
	copy /Y $(TEMPDIR)\condor$@.wsdl     .
	rd /q /s $(TEMPDIR) > NUL 2>&1

shared_port : $(SRCDIR)\condor_shared_port\soap_$@Stub.cpp \
			$(SRCDIR)\condor_shared_port\gsoap_$@.h
	-2mkdir $(TEMPDIR)
	cd $(SRCDIR)\condor_shared_port
	$(SOAPCPP) $(SOAPCPPFLAGS) -p soap_$@ -d $(TEMPDIR) gsoap_$@.h
	copy /Y $(TEMPDIR)\soap_$@C.cpp      .
	copy /Y $(TEMPDIR)\soap_$@Server.cpp .
	copy /Y $(TEMPDIR)\condor$@.nsmap    .
	copy /Y $(TEMPDIR)\soap_$@H.h        .
	copy /Y $(TEMPDIR)\soap_$@Stub.h     .
	copy /Y $(TEMPDIR)\condor$@.wsdl     .
	rd /q /s $(TEMPDIR) > NUL 2>&1