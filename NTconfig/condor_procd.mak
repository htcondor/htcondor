# Microsoft Developer Studio Generated NMAKE File, Based on condor_procd.dsp
!IF "$(CFG)" == ""
CFG=condor_procd - Win32 Debug
!MESSAGE No configuration specified. Defaulting to condor_procd - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "condor_procd - Win32 Release" && "$(CFG)" != "condor_procd - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_procd.mak" CFG="condor_procd - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_procd - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "condor_procd - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "condor_procd - Win32 Release"

OUTDIR=.\..\Release
INTDIR=.\..\Release
# Begin Custom Macros
OutDir=.\..\Release
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_procd.exe"

!ELSE 

ALL : "condor_procapi - Win32 Release" "$(OUTDIR)\condor_procd.exe"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"condor_procapi - Win32 ReleaseCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\condor_pidenvid.obj"
	-@erase "$(INTDIR)\dprintf_lite.obj"
	-@erase "$(INTDIR)\local_server.WINDOWS.obj"
	-@erase "$(INTDIR)\login_tracker.obj"
	-@erase "$(INTDIR)\ntsysinfo.obj"
	-@erase "$(INTDIR)\parent_tracker.obj"
	-@erase "$(INTDIR)\proc_family.obj"
	-@erase "$(INTDIR)\proc_family_member.obj"
	-@erase "$(INTDIR)\proc_family_monitor.obj"
	-@erase "$(INTDIR)\proc_family_server.obj"
	-@erase "$(INTDIR)\proc_family_tracker.obj"
	-@erase "$(INTDIR)\procd_common.obj"
	-@erase "$(INTDIR)\procd_main.obj"
	-@erase "$(INTDIR)\process_control.WINDOWS.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\condor_procd.exe"
	-@erase "$(OUTDIR)\condor_procd.map"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MT /W3 /GX /Z7 /O1 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) /c 

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_procd.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=../Release/condor_common.obj ../Release/condor_common_c.obj $(CONDOR_LIB) $(CONDOR_LIBPATH) $(CONDOR_GSOAP_LIB) $(CONDOR_GSOAP_LIBPATH) $(CONDOR_KERB_LIB) $(CONDOR_KERB_LIBPATH) $(CONDOR_PCRE_LIB) $(CONDOR_PCRE_LIBPATH) $(CONDOR_GLOBUS_LIB) $(CONDOR_GLOBUS_LIBPATH) $(CONDOR_OPENSSL_LIB) $(CONDOR_POSTGRESQL_LIB) $(CONDOR_OPENSSL_LIBPATH) $(CONDOR_POSTGRESQL_LIBPATH) /nologo /subsystem:console /pdb:none /map:"$(INTDIR)\condor_procd.map" /debug /machine:I386 /out:"$(OUTDIR)\condor_procd.exe" 
LINK32_OBJS= \
	"$(INTDIR)\condor_pidenvid.obj" \
	"$(INTDIR)\dprintf_lite.obj" \
	"$(INTDIR)\local_server.WINDOWS.obj" \
	"$(INTDIR)\login_tracker.obj" \
	"$(INTDIR)\ntsysinfo.obj" \
	"$(INTDIR)\parent_tracker.obj" \
	"$(INTDIR)\proc_family.obj" \
	"$(INTDIR)\proc_family_member.obj" \
	"$(INTDIR)\proc_family_monitor.obj" \
	"$(INTDIR)\proc_family_server.obj" \
	"$(INTDIR)\proc_family_tracker.obj" \
	"$(INTDIR)\procd_common.obj" \
	"$(INTDIR)\procd_main.obj" \
	"$(INTDIR)\process_control.WINDOWS.obj" \
	"$(OUTDIR)\condor_procapi.lib"

"$(OUTDIR)\condor_procd.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "condor_procd - Win32 Debug"

OUTDIR=.\..\Debug
INTDIR=.\..\Debug
# Begin Custom Macros
OutDir=.\..\Debug
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_procd.exe"

!ELSE 

ALL : "condor_procapi - Win32 Debug" "$(OUTDIR)\condor_procd.exe"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"condor_procapi - Win32 DebugCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\condor_pidenvid.obj"
	-@erase "$(INTDIR)\dprintf_lite.obj"
	-@erase "$(INTDIR)\local_server.WINDOWS.obj"
	-@erase "$(INTDIR)\login_tracker.obj"
	-@erase "$(INTDIR)\ntsysinfo.obj"
	-@erase "$(INTDIR)\parent_tracker.obj"
	-@erase "$(INTDIR)\proc_family.obj"
	-@erase "$(INTDIR)\proc_family_member.obj"
	-@erase "$(INTDIR)\proc_family_monitor.obj"
	-@erase "$(INTDIR)\proc_family_server.obj"
	-@erase "$(INTDIR)\proc_family_tracker.obj"
	-@erase "$(INTDIR)\procd_common.obj"
	-@erase "$(INTDIR)\procd_main.obj"
	-@erase "$(INTDIR)\process_control.WINDOWS.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\condor_procd.exe"
	-@erase "$(OUTDIR)\condor_procd.ilk"
	-@erase "$(OUTDIR)\condor_procd.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /TP $(CONDOR_INCLUDE) /c 

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_procd.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=../Debug/condor_common.obj ..\Debug\condor_common_c.obj $(CONDOR_LIB) $(CONDOR_LIBPATH) $(CONDOR_GSOAP_LIB) $(CONDOR_GSOAP_LIBPATH) $(CONDOR_KERB_LIB) $(CONDOR_KERB_LIBPATH) $(CONDOR_PCRE_LIB) $(CONDOR_PCRE_LIBPATH) $(CONDOR_GLOBUS_LIB) $(CONDOR_GLOBUS_LIBPATH) $(CONDOR_OPENSSL_LIB) $(CONDOR_POSTGRESQL_LIB) $(CONDOR_OPENSSL_LIBPATH) $(CONDOR_POSTGRESQL_LIBPATH) /nologo /subsystem:console /incremental:yes /pdb:"$(OUTDIR)\condor_procd.pdb" /debug /machine:I386 /out:"$(OUTDIR)\condor_procd.exe" /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\condor_pidenvid.obj" \
	"$(INTDIR)\dprintf_lite.obj" \
	"$(INTDIR)\local_server.WINDOWS.obj" \
	"$(INTDIR)\login_tracker.obj" \
	"$(INTDIR)\ntsysinfo.obj" \
	"$(INTDIR)\parent_tracker.obj" \
	"$(INTDIR)\proc_family.obj" \
	"$(INTDIR)\proc_family_member.obj" \
	"$(INTDIR)\proc_family_monitor.obj" \
	"$(INTDIR)\proc_family_server.obj" \
	"$(INTDIR)\proc_family_tracker.obj" \
	"$(INTDIR)\procd_common.obj" \
	"$(INTDIR)\procd_main.obj" \
	"$(INTDIR)\process_control.WINDOWS.obj" \
	"$(OUTDIR)\condor_procapi.lib"

"$(OUTDIR)\condor_procd.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("condor_procd.dep")
!INCLUDE "condor_procd.dep"
!ELSE 
!MESSAGE Warning: cannot find "condor_procd.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "condor_procd - Win32 Release" || "$(CFG)" == "condor_procd - Win32 Debug"

!IF  "$(CFG)" == "condor_procd - Win32 Release"

"condor_procapi - Win32 Release" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F ".\condor_procapi.mak" CFG="condor_procapi - Win32 Release" 
   cd "."

"condor_procapi - Win32 ReleaseCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F ".\condor_procapi.mak" CFG="condor_procapi - Win32 Release" RECURSE=1 CLEAN 
   cd "."

!ELSEIF  "$(CFG)" == "condor_procd - Win32 Debug"

"condor_procapi - Win32 Debug" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F ".\condor_procapi.mak" CFG="condor_procapi - Win32 Debug" 
   cd "."

"condor_procapi - Win32 DebugCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F ".\condor_procapi.mak" CFG="condor_procapi - Win32 Debug" RECURSE=1 CLEAN 
   cd "."

!ENDIF 

SOURCE=..\src\condor_util_lib\condor_pidenvid.c

"$(INTDIR)\condor_pidenvid.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_procd\dprintf_lite.C

"$(INTDIR)\dprintf_lite.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_procd\local_server.WINDOWS.C

"$(INTDIR)\local_server.WINDOWS.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_procd\login_tracker.C

"$(INTDIR)\login_tracker.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\ntsysinfo.C"

"$(INTDIR)\ntsysinfo.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_procd\parent_tracker.C

"$(INTDIR)\parent_tracker.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_procd\proc_family.C

"$(INTDIR)\proc_family.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_procd\proc_family_member.C

"$(INTDIR)\proc_family_member.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_procd\proc_family_monitor.C

"$(INTDIR)\proc_family_monitor.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_procd\proc_family_server.C

"$(INTDIR)\proc_family_server.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_procd\proc_family_tracker.C

"$(INTDIR)\proc_family_tracker.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_procd\procd_common.C

"$(INTDIR)\procd_common.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_procd\procd_main.C

"$(INTDIR)\procd_main.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\process_control.WINDOWS.C"

"$(INTDIR)\process_control.WINDOWS.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)



!ENDIF 

