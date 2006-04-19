# Microsoft Developer Studio Generated NMAKE File, Based on condor_kbdd_dll.dsp
!IF "$(CFG)" == ""
CFG=condor_kbdd_dll - Win32 Release
!MESSAGE No configuration specified. Defaulting to condor_kbdd_dll - Win32 Release.
!ENDIF 

!IF "$(CFG)" != "condor_kbdd_dll - Win32 Debug" && "$(CFG)" != "condor_kbdd_dll - Win32 Release"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_kbdd_dll.mak" CFG="condor_kbdd_dll - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_kbdd_dll - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "condor_kbdd_dll - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "condor_kbdd_dll - Win32 Debug"

OUTDIR=.\..\Debug
INTDIR=.\..\Debug
# Begin Custom Macros
OutDir=.\..\Debug
# End Custom Macros

ALL : "$(OUTDIR)\condor_kbdd_dll.dll"


CLEAN :
	-@erase "$(INTDIR)\NT-kbdd-dll.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\condor_kbdd_dll.dll"
	-@erase "$(OUTDIR)\condor_kbdd_dll.exp"
	-@erase "$(OUTDIR)\condor_kbdd_dll.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MT /W3 /GX /O2 /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /I "..\src\condor_daemon_client" /I "..\src\condor_daemon_core.V6" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_kbdd_dll.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /pdb:none /machine:I386 /out:"$(OUTDIR)\condor_kbdd_dll.dll" /implib:"$(OUTDIR)\condor_kbdd_dll.lib" 
LINK32_OBJS= \
	"$(INTDIR)\NT-kbdd-dll.obj"

"$(OUTDIR)\condor_kbdd_dll.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "condor_kbdd_dll - Win32 Release"

OUTDIR=.\..\Release
INTDIR=.\..\Release
# Begin Custom Macros
OutDir=.\..\Release
# End Custom Macros

ALL : "$(OUTDIR)\condor_kbdd_dll.dll"


CLEAN :
	-@erase "$(INTDIR)\NT-kbdd-dll.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\condor_kbdd_dll.dll"
	-@erase "$(OUTDIR)\condor_kbdd_dll.exp"
	-@erase "$(OUTDIR)\condor_kbdd_dll.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MT /W3 /GX /O1 /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /I "..\src\condor_daemon_client" /I "..\src\condor_daemon_core.V6" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_kbdd_dll.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /pdb:none /machine:I386 /out:"$(OUTDIR)\condor_kbdd_dll.dll" /implib:"$(OUTDIR)\condor_kbdd_dll.lib" 
LINK32_OBJS= \
	"$(INTDIR)\NT-kbdd-dll.obj"

"$(OUTDIR)\condor_kbdd_dll.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
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

MTL_PROJ=/nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32 

!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("condor_kbdd_dll.dep")
!INCLUDE "condor_kbdd_dll.dep"
!ELSE 
!MESSAGE Warning: cannot find "condor_kbdd_dll.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "condor_kbdd_dll - Win32 Debug" || "$(CFG)" == "condor_kbdd_dll - Win32 Release"
SOURCE="..\src\condor_kbdd\NT-kbdd-dll.C"

"$(INTDIR)\NT-kbdd-dll.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)



!ENDIF 

