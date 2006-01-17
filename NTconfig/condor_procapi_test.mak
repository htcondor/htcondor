# Microsoft Developer Studio Generated NMAKE File, Based on condor_procapi_test.dsp
!IF "$(CFG)" == ""
CFG=condor_procapi_test - Win32 Release
!MESSAGE No configuration specified. Defaulting to condor_procapi_test - Win32 Release.
!ENDIF 

!IF "$(CFG)" != "condor_procapi_test - Win32 Debug" && "$(CFG)" != "condor_procapi_test - Win32 Release"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_procapi_test.mak" CFG="condor_procapi_test - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_procapi_test - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE "condor_procapi_test - Win32 Release" (based on "Win32 (x86) Console Application")
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

!IF  "$(CFG)" == "condor_procapi_test - Win32 Debug"

OUTDIR=.\..\Debug
INTDIR=.\..\Debug
# Begin Custom Macros
OutDir=.\..\Debug
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_procapi_test.exe"

!ELSE 

ALL : "condor_procapi - Win32 Debug" "$(OUTDIR)\condor_procapi_test.exe"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"condor_procapi - Win32 DebugCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\ntsysinfo.obj"
	-@erase "$(INTDIR)\testprocapi.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\condor_procapi_test.exe"
	-@erase "$(OUTDIR)\condor_procapi_test.ilk"
	-@erase "$(OUTDIR)\condor_procapi_test.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /I "..\src\condor_daemon_client" /I "..\src\condor_daemon_core.V6" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /Fp"$(INTDIR)\condor_common.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_procapi_test.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ws2_32.lib netapi32.lib ../Debug/condor_common.obj ..\Debug\condor_common_c.obj /nologo /subsystem:console /incremental:yes /pdb:"$(OUTDIR)\condor_procapi_test.pdb" /debug /machine:I386 /out:"$(OUTDIR)\condor_procapi_test.exe" /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\ntsysinfo.obj" \
	"$(INTDIR)\testprocapi.obj" \
	"$(OUTDIR)\condor_procapi.lib"

"$(OUTDIR)\condor_procapi_test.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "condor_procapi_test - Win32 Release"

OUTDIR=.\..\Release
INTDIR=.\..\Release
# Begin Custom Macros
OutDir=.\..\Release
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_procapi_test.exe"

!ELSE 

ALL : "condor_procapi - Win32 Release" "$(OUTDIR)\condor_procapi_test.exe"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"condor_procapi - Win32 ReleaseCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\ntsysinfo.obj"
	-@erase "$(INTDIR)\testprocapi.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\condor_procapi_test.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MT /W3 /GX /Z7 /O1 /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /I "..\src\condor_daemon_client" /I "..\src\condor_daemon_core.V6" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /Fp"$(INTDIR)\condor_common.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_procapi_test.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ws2_32.lib netapi32.lib ../Release/condor_common.obj ../Release/condor_common_c.obj /nologo /subsystem:console /pdb:none /debug /machine:I386 /out:"$(OUTDIR)\condor_procapi_test.exe" 
LINK32_OBJS= \
	"$(INTDIR)\ntsysinfo.obj" \
	"$(INTDIR)\testprocapi.obj" \
	"$(OUTDIR)\condor_procapi.lib"

"$(OUTDIR)\condor_procapi_test.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
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
!IF EXISTS("condor_procapi_test.dep")
!INCLUDE "condor_procapi_test.dep"
!ELSE 
!MESSAGE Warning: cannot find "condor_procapi_test.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "condor_procapi_test - Win32 Debug" || "$(CFG)" == "condor_procapi_test - Win32 Release"

!IF  "$(CFG)" == "condor_procapi_test - Win32 Debug"

"condor_procapi - Win32 Debug" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_procapi.mak CFG="condor_procapi - Win32 Debug" 
   cd "."

"condor_procapi - Win32 DebugCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_procapi.mak CFG="condor_procapi - Win32 Debug" RECURSE=1 CLEAN 
   cd "."

!ELSEIF  "$(CFG)" == "condor_procapi_test - Win32 Release"

"condor_procapi - Win32 Release" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_procapi.mak CFG="condor_procapi - Win32 Release" 
   cd "."

"condor_procapi - Win32 ReleaseCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_procapi.mak CFG="condor_procapi - Win32 Release" RECURSE=1 CLEAN 
   cd "."

!ENDIF 

SOURCE="..\src\condor_c++_util\ntsysinfo.C"

"$(INTDIR)\ntsysinfo.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_procapi\testprocapi.C

"$(INTDIR)\testprocapi.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)



!ENDIF 

