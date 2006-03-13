# Microsoft Developer Studio Generated NMAKE File, Based on condor_startd.dsp
!IF "$(CFG)" == ""
CFG=condor_startd - Win32 Release
!MESSAGE No configuration specified. Defaulting to condor_startd - Win32 Release.
!ENDIF 

!IF "$(CFG)" != "condor_startd - Win32 Debug" && "$(CFG)" != "condor_startd - Win32 Release"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_startd.mak" CFG="condor_startd - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_startd - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE "condor_startd - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "condor_startd - Win32 Debug"

OUTDIR=.\..\Debug
INTDIR=.\..\Debug
# Begin Custom Macros
OutDir=.\..\Debug
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_startd.exe"

!ELSE 

ALL : "condor_sysapi - Win32 Debug" "condor_procapi - Win32 Debug" "condor_daemon_core - Win32 Debug" "condor_io - Win32 Debug" "condor_classad - Win32 Debug" "condor_cpp_util - Win32 Debug" "condor_util_lib - Win32 Debug" "$(OUTDIR)\condor_startd.exe"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"condor_util_lib - Win32 DebugCLEAN" "condor_cpp_util - Win32 DebugCLEAN" "condor_classad - Win32 DebugCLEAN" "condor_io - Win32 DebugCLEAN" "condor_daemon_core - Win32 DebugCLEAN" "condor_procapi - Win32 DebugCLEAN" "condor_sysapi - Win32 DebugCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\AvailStats.obj"
	-@erase "$(INTDIR)\backfill_mgr.obj"
	-@erase "$(INTDIR)\boinc_mgr.obj"
	-@erase "$(INTDIR)\claim.obj"
	-@erase "$(INTDIR)\cod_mgr.obj"
	-@erase "$(INTDIR)\command.obj"
	-@erase "$(INTDIR)\CondorSystrayNotifier.obj"
	-@erase "$(INTDIR)\LoadQueue.obj"
	-@erase "$(INTDIR)\mds.obj"
	-@erase "$(INTDIR)\Reqexp.obj"
	-@erase "$(INTDIR)\ResAttributes.obj"
	-@erase "$(INTDIR)\ResMgr.obj"
	-@erase "$(INTDIR)\Resource.obj"
	-@erase "$(INTDIR)\ResState.obj"
	-@erase "$(INTDIR)\soap_startdC.obj"
	-@erase "$(INTDIR)\soap_startdServer.obj"
	-@erase "$(INTDIR)\soap_startdStub.obj"
	-@erase "$(INTDIR)\startd_cronjob.obj"
	-@erase "$(INTDIR)\startd_cronmgr.obj"
	-@erase "$(INTDIR)\startd_main.obj"
	-@erase "$(INTDIR)\Starter.obj"
	-@erase "$(INTDIR)\starter_mgr.obj"
	-@erase "$(INTDIR)\util.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(INTDIR)\vm_common.obj"
	-@erase "$(INTDIR)\VMMachine.obj"
	-@erase "$(INTDIR)\VMManager.obj"
	-@erase "$(INTDIR)\VMRegister.obj"
	-@erase "$(OUTDIR)\condor_startd.exe"
	-@erase "$(OUTDIR)\condor_startd.ilk"
	-@erase "$(OUTDIR)\condor_startd.map"
	-@erase "$(OUTDIR)\condor_startd.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /Fp"$(INTDIR)\condor_common.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

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

RSC=rc.exe
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_startd.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=../Debug/condor_common.obj ..\Debug\condor_common_c.obj pdh.lib $(CONDOR_LIB) $(CONDOR_LIBPATH) $(CONDOR_GSOAP_LIB) $(CONDOR_GSOAP_LIBPATH) $(CONDOR_KERB_LIB) $(CONDOR_KERB_LIBPATH) $(CONDOR_PCRE_LIB) $(CONDOR_PCRE_LIBPATH) $(CONDOR_GLOBUS_LIB) $(CONDOR_GLOBUS_LIBPATH) $(CONDOR_OPENSSL_LIB) $(CONDOR_POSTGRESQL_LIB) $(CONDOR_OPENSSL_LIBPATH) $(CONDOR_POSTGRESQL_LIBPATH) /nologo /subsystem:console /incremental:yes /pdb:"$(OUTDIR)\condor_startd.pdb" /map:"$(INTDIR)\condor_startd.map" /debug /machine:I386 /out:"$(OUTDIR)\condor_startd.exe" /SWAPRUN:NET 
LINK32_OBJS= \
	"$(INTDIR)\AvailStats.obj" \
	"$(INTDIR)\backfill_mgr.obj" \
	"$(INTDIR)\boinc_mgr.obj" \
	"$(INTDIR)\claim.obj" \
	"$(INTDIR)\cod_mgr.obj" \
	"$(INTDIR)\command.obj" \
	"$(INTDIR)\CondorSystrayNotifier.obj" \
	"$(INTDIR)\LoadQueue.obj" \
	"$(INTDIR)\mds.obj" \
	"$(INTDIR)\Reqexp.obj" \
	"$(INTDIR)\ResAttributes.obj" \
	"$(INTDIR)\ResMgr.obj" \
	"$(INTDIR)\Resource.obj" \
	"$(INTDIR)\ResState.obj" \
	"$(INTDIR)\soap_startdC.obj" \
	"$(INTDIR)\soap_startdServer.obj" \
	"$(INTDIR)\soap_startdStub.obj" \
	"$(INTDIR)\startd_cronjob.obj" \
	"$(INTDIR)\startd_cronmgr.obj" \
	"$(INTDIR)\startd_main.obj" \
	"$(INTDIR)\Starter.obj" \
	"$(INTDIR)\starter_mgr.obj" \
	"$(INTDIR)\util.obj" \
	"$(INTDIR)\vm_common.obj" \
	"$(INTDIR)\VMMachine.obj" \
	"$(INTDIR)\VMManager.obj" \
	"$(INTDIR)\VMRegister.obj" \
	"..\src\condor_util_lib\condor_util.lib" \
	"$(OUTDIR)\condor_cpp_util.lib" \
	"$(OUTDIR)\condor_classad.lib" \
	"$(OUTDIR)\condor_io.lib" \
	"$(OUTDIR)\condor_daemon_core.lib" \
	"$(OUTDIR)\condor_procapi.lib" \
	"$(OUTDIR)\condor_sysapi.lib"

"$(OUTDIR)\condor_startd.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "condor_startd - Win32 Release"

OUTDIR=.\..\Release
INTDIR=.\..\Release
# Begin Custom Macros
OutDir=.\..\Release
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_startd.exe"

!ELSE 

ALL : "condor_sysapi - Win32 Release" "condor_procapi - Win32 Release" "condor_daemon_core - Win32 Release" "condor_io - Win32 Release" "condor_classad - Win32 Release" "condor_cpp_util - Win32 Release" "condor_util_lib - Win32 Release" "$(OUTDIR)\condor_startd.exe"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"condor_util_lib - Win32 ReleaseCLEAN" "condor_cpp_util - Win32 ReleaseCLEAN" "condor_classad - Win32 ReleaseCLEAN" "condor_io - Win32 ReleaseCLEAN" "condor_daemon_core - Win32 ReleaseCLEAN" "condor_procapi - Win32 ReleaseCLEAN" "condor_sysapi - Win32 ReleaseCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\AvailStats.obj"
	-@erase "$(INTDIR)\backfill_mgr.obj"
	-@erase "$(INTDIR)\boinc_mgr.obj"
	-@erase "$(INTDIR)\claim.obj"
	-@erase "$(INTDIR)\cod_mgr.obj"
	-@erase "$(INTDIR)\command.obj"
	-@erase "$(INTDIR)\CondorSystrayNotifier.obj"
	-@erase "$(INTDIR)\LoadQueue.obj"
	-@erase "$(INTDIR)\mds.obj"
	-@erase "$(INTDIR)\Reqexp.obj"
	-@erase "$(INTDIR)\ResAttributes.obj"
	-@erase "$(INTDIR)\ResMgr.obj"
	-@erase "$(INTDIR)\Resource.obj"
	-@erase "$(INTDIR)\ResState.obj"
	-@erase "$(INTDIR)\soap_startdC.obj"
	-@erase "$(INTDIR)\soap_startdServer.obj"
	-@erase "$(INTDIR)\soap_startdStub.obj"
	-@erase "$(INTDIR)\startd_cronjob.obj"
	-@erase "$(INTDIR)\startd_cronmgr.obj"
	-@erase "$(INTDIR)\startd_main.obj"
	-@erase "$(INTDIR)\Starter.obj"
	-@erase "$(INTDIR)\starter_mgr.obj"
	-@erase "$(INTDIR)\util.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vm_common.obj"
	-@erase "$(INTDIR)\VMMachine.obj"
	-@erase "$(INTDIR)\VMManager.obj"
	-@erase "$(INTDIR)\VMRegister.obj"
	-@erase "$(OUTDIR)\condor_startd.exe"
	-@erase "$(OUTDIR)\condor_startd.map"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MT /W3 /GX /Z7 /O1 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /Fp"$(INTDIR)\condor_common.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

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

RSC=rc.exe
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_startd.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=../Release/condor_common.obj ../Release/condor_common_c.obj pdh.lib $(CONDOR_LIB) $(CONDOR_LIBPATH) $(CONDOR_GSOAP_LIB) $(CONDOR_GSOAP_LIBPATH) $(CONDOR_KERB_LIB) $(CONDOR_KERB_LIBPATH) $(CONDOR_PCRE_LIB) $(CONDOR_PCRE_LIBPATH) $(CONDOR_GLOBUS_LIB) $(CONDOR_GLOBUS_LIBPATH) $(CONDOR_OPENSSL_LIB) $(CONDOR_POSTGRESQL_LIB) $(CONDOR_OPENSSL_LIBPATH) $(CONDOR_POSTGRESQL_LIBPATH) /nologo /subsystem:console /pdb:none /map:"$(INTDIR)\condor_startd.map" /debug /machine:I386 /out:"$(OUTDIR)\condor_startd.exe" /SWAPRUN:NET 
LINK32_OBJS= \
	"$(INTDIR)\AvailStats.obj" \
	"$(INTDIR)\backfill_mgr.obj" \
	"$(INTDIR)\boinc_mgr.obj" \
	"$(INTDIR)\claim.obj" \
	"$(INTDIR)\cod_mgr.obj" \
	"$(INTDIR)\command.obj" \
	"$(INTDIR)\CondorSystrayNotifier.obj" \
	"$(INTDIR)\LoadQueue.obj" \
	"$(INTDIR)\mds.obj" \
	"$(INTDIR)\Reqexp.obj" \
	"$(INTDIR)\ResAttributes.obj" \
	"$(INTDIR)\ResMgr.obj" \
	"$(INTDIR)\Resource.obj" \
	"$(INTDIR)\ResState.obj" \
	"$(INTDIR)\soap_startdC.obj" \
	"$(INTDIR)\soap_startdServer.obj" \
	"$(INTDIR)\soap_startdStub.obj" \
	"$(INTDIR)\startd_cronjob.obj" \
	"$(INTDIR)\startd_cronmgr.obj" \
	"$(INTDIR)\startd_main.obj" \
	"$(INTDIR)\Starter.obj" \
	"$(INTDIR)\starter_mgr.obj" \
	"$(INTDIR)\util.obj" \
	"$(INTDIR)\vm_common.obj" \
	"$(INTDIR)\VMMachine.obj" \
	"$(INTDIR)\VMManager.obj" \
	"$(INTDIR)\VMRegister.obj" \
	"..\src\condor_util_lib\condor_util.lib" \
	"$(OUTDIR)\condor_cpp_util.lib" \
	"$(OUTDIR)\condor_classad.lib" \
	"$(OUTDIR)\condor_io.lib" \
	"$(OUTDIR)\condor_daemon_core.lib" \
	"$(OUTDIR)\condor_procapi.lib" \
	"$(OUTDIR)\condor_sysapi.lib"

"$(OUTDIR)\condor_startd.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("condor_startd.dep")
!INCLUDE "condor_startd.dep"
!ELSE 
!MESSAGE Warning: cannot find "condor_startd.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "condor_startd - Win32 Debug" || "$(CFG)" == "condor_startd - Win32 Release"

!IF  "$(CFG)" == "condor_startd - Win32 Debug"

"condor_util_lib - Win32 Debug" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F ".\condor_util_lib.mak" CFG="condor_util_lib - Win32 Debug" 
   cd "."

"condor_util_lib - Win32 DebugCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F ".\condor_util_lib.mak" CFG="condor_util_lib - Win32 Debug" RECURSE=1 CLEAN 
   cd "."

!ELSEIF  "$(CFG)" == "condor_startd - Win32 Release"

"condor_util_lib - Win32 Release" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F ".\condor_util_lib.mak" CFG="condor_util_lib - Win32 Release" 
   cd "."

"condor_util_lib - Win32 ReleaseCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F ".\condor_util_lib.mak" CFG="condor_util_lib - Win32 Release" RECURSE=1 CLEAN 
   cd "."

!ENDIF 

!IF  "$(CFG)" == "condor_startd - Win32 Debug"

"condor_cpp_util - Win32 Debug" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F ".\condor_cpp_util.mak" CFG="condor_cpp_util - Win32 Debug" 
   cd "."

"condor_cpp_util - Win32 DebugCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F ".\condor_cpp_util.mak" CFG="condor_cpp_util - Win32 Debug" RECURSE=1 CLEAN 
   cd "."

!ELSEIF  "$(CFG)" == "condor_startd - Win32 Release"

"condor_cpp_util - Win32 Release" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F ".\condor_cpp_util.mak" CFG="condor_cpp_util - Win32 Release" 
   cd "."

"condor_cpp_util - Win32 ReleaseCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F ".\condor_cpp_util.mak" CFG="condor_cpp_util - Win32 Release" RECURSE=1 CLEAN 
   cd "."

!ENDIF 

!IF  "$(CFG)" == "condor_startd - Win32 Debug"

"condor_classad - Win32 Debug" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F ".\condor_classad.mak" CFG="condor_classad - Win32 Debug" 
   cd "."

"condor_classad - Win32 DebugCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F ".\condor_classad.mak" CFG="condor_classad - Win32 Debug" RECURSE=1 CLEAN 
   cd "."

!ELSEIF  "$(CFG)" == "condor_startd - Win32 Release"

"condor_classad - Win32 Release" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F ".\condor_classad.mak" CFG="condor_classad - Win32 Release" 
   cd "."

"condor_classad - Win32 ReleaseCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F ".\condor_classad.mak" CFG="condor_classad - Win32 Release" RECURSE=1 CLEAN 
   cd "."

!ENDIF 

!IF  "$(CFG)" == "condor_startd - Win32 Debug"

"condor_io - Win32 Debug" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F ".\condor_io.mak" CFG="condor_io - Win32 Debug" 
   cd "."

"condor_io - Win32 DebugCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F ".\condor_io.mak" CFG="condor_io - Win32 Debug" RECURSE=1 CLEAN 
   cd "."

!ELSEIF  "$(CFG)" == "condor_startd - Win32 Release"

"condor_io - Win32 Release" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F ".\condor_io.mak" CFG="condor_io - Win32 Release" 
   cd "."

"condor_io - Win32 ReleaseCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F ".\condor_io.mak" CFG="condor_io - Win32 Release" RECURSE=1 CLEAN 
   cd "."

!ENDIF 

!IF  "$(CFG)" == "condor_startd - Win32 Debug"

"condor_daemon_core - Win32 Debug" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F ".\condor_daemon_core.mak" CFG="condor_daemon_core - Win32 Debug" 
   cd "."

"condor_daemon_core - Win32 DebugCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F ".\condor_daemon_core.mak" CFG="condor_daemon_core - Win32 Debug" RECURSE=1 CLEAN 
   cd "."

!ELSEIF  "$(CFG)" == "condor_startd - Win32 Release"

"condor_daemon_core - Win32 Release" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F ".\condor_daemon_core.mak" CFG="condor_daemon_core - Win32 Release" 
   cd "."

"condor_daemon_core - Win32 ReleaseCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F ".\condor_daemon_core.mak" CFG="condor_daemon_core - Win32 Release" RECURSE=1 CLEAN 
   cd "."

!ENDIF 

!IF  "$(CFG)" == "condor_startd - Win32 Debug"

"condor_procapi - Win32 Debug" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F ".\condor_procapi.mak" CFG="condor_procapi - Win32 Debug" 
   cd "."

"condor_procapi - Win32 DebugCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F ".\condor_procapi.mak" CFG="condor_procapi - Win32 Debug" RECURSE=1 CLEAN 
   cd "."

!ELSEIF  "$(CFG)" == "condor_startd - Win32 Release"

"condor_procapi - Win32 Release" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F ".\condor_procapi.mak" CFG="condor_procapi - Win32 Release" 
   cd "."

"condor_procapi - Win32 ReleaseCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F ".\condor_procapi.mak" CFG="condor_procapi - Win32 Release" RECURSE=1 CLEAN 
   cd "."

!ENDIF 

!IF  "$(CFG)" == "condor_startd - Win32 Debug"

"condor_sysapi - Win32 Debug" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F ".\condor_sysapi.mak" CFG="condor_sysapi - Win32 Debug" 
   cd "."

"condor_sysapi - Win32 DebugCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F ".\condor_sysapi.mak" CFG="condor_sysapi - Win32 Debug" RECURSE=1 CLEAN 
   cd "."

!ELSEIF  "$(CFG)" == "condor_startd - Win32 Release"

"condor_sysapi - Win32 Release" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F ".\condor_sysapi.mak" CFG="condor_sysapi - Win32 Release" 
   cd "."

"condor_sysapi - Win32 ReleaseCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F ".\condor_sysapi.mak" CFG="condor_sysapi - Win32 Release" RECURSE=1 CLEAN 
   cd "."

!ENDIF 

SOURCE=..\src\condor_startd.V6\AvailStats.C

"$(INTDIR)\AvailStats.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_startd.V6\backfill_mgr.C

"$(INTDIR)\backfill_mgr.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_startd.V6\boinc_mgr.C

"$(INTDIR)\boinc_mgr.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_startd.V6\claim.C

"$(INTDIR)\claim.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_startd.V6\cod_mgr.C

"$(INTDIR)\cod_mgr.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_startd.V6\command.C

"$(INTDIR)\command.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_startd.V6\CondorSystrayNotifier.cpp

"$(INTDIR)\CondorSystrayNotifier.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_startd.V6\LoadQueue.C

"$(INTDIR)\LoadQueue.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_startd.V6\mds.C

"$(INTDIR)\mds.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_startd.V6\Reqexp.C

"$(INTDIR)\Reqexp.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_startd.V6\ResAttributes.C

"$(INTDIR)\ResAttributes.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_startd.V6\ResMgr.C

"$(INTDIR)\ResMgr.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_startd.V6\Resource.C

"$(INTDIR)\Resource.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_startd.V6\ResState.C

"$(INTDIR)\ResState.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_startd.V6\soap_startdC.C

!IF  "$(CFG)" == "condor_startd - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

"$(INTDIR)\soap_startdC.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_startd - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /Z7 /O1 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

"$(INTDIR)\soap_startdC.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=..\src\condor_startd.V6\soap_startdServer.C

!IF  "$(CFG)" == "condor_startd - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

"$(INTDIR)\soap_startdServer.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_startd - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /Z7 /O1 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

"$(INTDIR)\soap_startdServer.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=..\src\condor_startd.V6\soap_startdStub.C

!IF  "$(CFG)" == "condor_startd - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

"$(INTDIR)\soap_startdStub.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_startd - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /Z7 /O1 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

"$(INTDIR)\soap_startdStub.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=..\src\condor_startd.V6\startd_cronjob.C

"$(INTDIR)\startd_cronjob.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_startd.V6\startd_cronmgr.C

"$(INTDIR)\startd_cronmgr.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_startd.V6\startd_main.C

"$(INTDIR)\startd_main.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_startd.V6\Starter.C

"$(INTDIR)\Starter.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_startd.V6\starter_mgr.C

"$(INTDIR)\starter_mgr.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_startd.V6\util.C

"$(INTDIR)\util.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_startd.V6\vm_common.C

"$(INTDIR)\vm_common.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_startd.V6\VMMachine.C

"$(INTDIR)\VMMachine.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_startd.V6\VMManager.C

"$(INTDIR)\VMManager.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_startd.V6\VMRegister.C

"$(INTDIR)\VMRegister.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)



!ENDIF 

