# Microsoft Developer Studio Generated NMAKE File, Based on condor_daemon_core.dsp
!IF "$(CFG)" == ""
CFG=condor_daemon_core - Win32 Release
!MESSAGE No configuration specified. Defaulting to condor_daemon_core - Win32 Release.
!ENDIF 

!IF "$(CFG)" != "condor_daemon_core - Win32 Debug" && "$(CFG)" != "condor_daemon_core - Win32 Release"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_daemon_core.mak" CFG="condor_daemon_core - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_daemon_core - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "condor_daemon_core - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "condor_daemon_core - Win32 Debug"

OUTDIR=.\..\Debug
INTDIR=.\..\Debug
# Begin Custom Macros
OutDir=.\..\Debug
# End Custom Macros

ALL : "$(OUTDIR)\condor_daemon_core.lib"


CLEAN :
	-@erase "$(INTDIR)\accessdesktop.WIN32.obj"
	-@erase "$(INTDIR)\condor_ipverify.obj"
	-@erase "$(INTDIR)\condor_lock.obj"
	-@erase "$(INTDIR)\condor_lock_base.obj"
	-@erase "$(INTDIR)\condor_lock_file.obj"
	-@erase "$(INTDIR)\condor_lock_implementation.obj"
	-@erase "$(INTDIR)\daemon_core.obj"
	-@erase "$(INTDIR)\daemon_core_main.obj"
	-@erase "$(INTDIR)\datathread.obj"
	-@erase "$(INTDIR)\exphnd.WIN32.obj"
	-@erase "$(INTDIR)\httpget.obj"
	-@erase "$(INTDIR)\self_draining_queue.obj"
	-@erase "$(INTDIR)\self_monitor.obj"
	-@erase "$(INTDIR)\soap_core.obj"
	-@erase "$(INTDIR)\stdsoap2.obj"
	-@erase "$(INTDIR)\timer_manager.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\condor_daemon_core.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /Fp"$(INTDIR)\condor_common.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_daemon_core.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\condor_daemon_core.lib" 
LIB32_OBJS= \
	"$(INTDIR)\accessdesktop.WIN32.obj" \
	"$(INTDIR)\condor_ipverify.obj" \
	"$(INTDIR)\condor_lock.obj" \
	"$(INTDIR)\condor_lock_base.obj" \
	"$(INTDIR)\condor_lock_file.obj" \
	"$(INTDIR)\condor_lock_implementation.obj" \
	"$(INTDIR)\daemon_core.obj" \
	"$(INTDIR)\daemon_core_main.obj" \
	"$(INTDIR)\datathread.obj" \
	"$(INTDIR)\exphnd.WIN32.obj" \
	"$(INTDIR)\httpget.obj" \
	"$(INTDIR)\self_draining_queue.obj" \
	"$(INTDIR)\self_monitor.obj" \
	"$(INTDIR)\soap_core.obj" \
	"$(INTDIR)\stdsoap2.obj" \
	"$(INTDIR)\timer_manager.obj"

"$(OUTDIR)\condor_daemon_core.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ELSEIF  "$(CFG)" == "condor_daemon_core - Win32 Release"

OUTDIR=.\../Release
INTDIR=.\../Release
# Begin Custom Macros
OutDir=.\../Release
# End Custom Macros

ALL : "$(OUTDIR)\condor_daemon_core.lib"


CLEAN :
	-@erase "$(INTDIR)\accessdesktop.WIN32.obj"
	-@erase "$(INTDIR)\condor_ipverify.obj"
	-@erase "$(INTDIR)\condor_lock.obj"
	-@erase "$(INTDIR)\condor_lock_base.obj"
	-@erase "$(INTDIR)\condor_lock_file.obj"
	-@erase "$(INTDIR)\condor_lock_implementation.obj"
	-@erase "$(INTDIR)\daemon_core.obj"
	-@erase "$(INTDIR)\daemon_core_main.obj"
	-@erase "$(INTDIR)\datathread.obj"
	-@erase "$(INTDIR)\exphnd.WIN32.obj"
	-@erase "$(INTDIR)\httpget.obj"
	-@erase "$(INTDIR)\self_draining_queue.obj"
	-@erase "$(INTDIR)\self_monitor.obj"
	-@erase "$(INTDIR)\soap_core.obj"
	-@erase "$(INTDIR)\stdsoap2.obj"
	-@erase "$(INTDIR)\timer_manager.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\condor_daemon_core.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MT /W3 /GX /Z7 /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /Fp"$(INTDIR)\condor_common.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_daemon_core.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\condor_daemon_core.lib" 
LIB32_OBJS= \
	"$(INTDIR)\accessdesktop.WIN32.obj" \
	"$(INTDIR)\condor_ipverify.obj" \
	"$(INTDIR)\condor_lock.obj" \
	"$(INTDIR)\condor_lock_base.obj" \
	"$(INTDIR)\condor_lock_file.obj" \
	"$(INTDIR)\condor_lock_implementation.obj" \
	"$(INTDIR)\daemon_core.obj" \
	"$(INTDIR)\daemon_core_main.obj" \
	"$(INTDIR)\datathread.obj" \
	"$(INTDIR)\exphnd.WIN32.obj" \
	"$(INTDIR)\httpget.obj" \
	"$(INTDIR)\self_draining_queue.obj" \
	"$(INTDIR)\self_monitor.obj" \
	"$(INTDIR)\soap_core.obj" \
	"$(INTDIR)\stdsoap2.obj" \
	"$(INTDIR)\timer_manager.obj"

"$(OUTDIR)\condor_daemon_core.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ENDIF 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("condor_daemon_core.dep")
!INCLUDE "condor_daemon_core.dep"
!ELSE 
!MESSAGE Warning: cannot find "condor_daemon_core.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "condor_daemon_core - Win32 Debug" || "$(CFG)" == "condor_daemon_core - Win32 Release"
SOURCE=..\src\condor_daemon_core.V6\accessdesktop.WIN32.C

"$(INTDIR)\accessdesktop.WIN32.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_daemon_core.V6\condor_ipverify.C

"$(INTDIR)\condor_ipverify.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_daemon_core.V6\condor_lock.C

"$(INTDIR)\condor_lock.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_daemon_core.V6\condor_lock_base.C

"$(INTDIR)\condor_lock_base.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_daemon_core.V6\condor_lock_file.C

"$(INTDIR)\condor_lock_file.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_daemon_core.V6\condor_lock_implementation.C

"$(INTDIR)\condor_lock_implementation.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_daemon_core.V6\daemon_core.C

"$(INTDIR)\daemon_core.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_daemon_core.V6\daemon_core_main.C

"$(INTDIR)\daemon_core_main.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_daemon_core.V6\datathread.C

"$(INTDIR)\datathread.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_daemon_core.V6\exphnd.WIN32.C

!IF  "$(CFG)" == "condor_daemon_core - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /Fp"$(INTDIR)\condor_common.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

"$(INTDIR)\exphnd.WIN32.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_daemon_core - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /Z7 /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /Fp"$(INTDIR)\condor_common.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

"$(INTDIR)\exphnd.WIN32.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=..\src\condor_daemon_core.V6\httpget.C

"$(INTDIR)\httpget.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_daemon_core.V6\self_draining_queue.C

"$(INTDIR)\self_draining_queue.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_daemon_core.V6\self_monitor.C

"$(INTDIR)\self_monitor.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_daemon_core.V6\soap_core.C

"$(INTDIR)\soap_core.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="$(EXT_INSTALL)\$(EXT_GSOAP_VERSION)\src\stdsoap2.cpp"

!IF  "$(CFG)" == "condor_daemon_core - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /Fp"$(INTDIR)\condor_common.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

"$(INTDIR)\stdsoap2.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_daemon_core - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /Z7 /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /Fp"$(INTDIR)\condor_common.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

"$(INTDIR)\stdsoap2.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=..\src\condor_daemon_core.V6\timer_manager.C

"$(INTDIR)\timer_manager.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)



!ENDIF 

