# Microsoft Developer Studio Generated NMAKE File, Based on condor_kbdd_dll.dsp
!IF "$(CFG)" == ""
CFG=condor_kbdd_dll - Win32 Debug
!MESSAGE No configuration specified. Defaulting to condor_kbdd_dll - Win32\
 Debug.
!ENDIF 

!IF "$(CFG)" != "condor_kbdd_dll - Win32 Release" && "$(CFG)" !=\
 "condor_kbdd_dll - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_kbdd_dll.mak" CFG="condor_kbdd_dll - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_kbdd_dll - Win32 Release" (based on\
 "Win32 (x86) Dynamic-Link Library")
!MESSAGE "condor_kbdd_dll - Win32 Debug" (based on\
 "Win32 (x86) Dynamic-Link Library")
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

!IF  "$(CFG)" == "condor_kbdd_dll - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release

!IF "$(RECURSE)" == "0" 

ALL : "..\src\condor_kbdd\condor_kbdd_dll.dll"

!ELSE 

ALL : "..\src\condor_kbdd\condor_kbdd_dll.dll"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\NT-kbdd-dll.obj"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(OUTDIR)\condor_kbdd_dll.exp"
	-@erase "$(OUTDIR)\condor_kbdd_dll.lib"
	-@erase "..\src\condor_kbdd\condor_kbdd_dll.dll"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 
CPP_OBJS=.\Release/
CPP_SBRS=.
MTL_PROJ=/nologo /D "NDEBUG" /mktyplib203 /o NUL /win32 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_kbdd_dll.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib /nologo /subsystem:windows /dll /incremental:no\
 /pdb:"$(OUTDIR)\condor_kbdd_dll.pdb" /machine:I386\
 /out:"..\src\condor_kbdd/condor_kbdd_dll.dll"\
 /implib:"$(OUTDIR)\condor_kbdd_dll.lib" 
LINK32_OBJS= \
	"$(INTDIR)\NT-kbdd-dll.obj"

"..\src\condor_kbdd\condor_kbdd_dll.dll" : "$(OUTDIR)" $(DEF_FILE)\
 $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "condor_kbdd_dll - Win32 Debug"

OUTDIR=.\..\src\condor_kbdd
INTDIR=.\..\src\condor_kbdd
# Begin Custom Macros
OutDir=.\..\src\condor_kbdd
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_kbdd_dll.dll"

!ELSE 

ALL : "$(OUTDIR)\condor_kbdd_dll.dll"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\NT-kbdd-dll.obj"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(OUTDIR)\condor_kbdd_dll.dll"
	-@erase "$(OUTDIR)\condor_kbdd_dll.exp"
	-@erase "$(OUTDIR)\condor_kbdd_dll.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MT /W3 /GX /O2 /D "WIN32" /D "_DEBUG" /D "_WINDOWS"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 
CPP_OBJS=..\src\condor_kbdd/
CPP_SBRS=.
MTL_PROJ=/nologo /D "_DEBUG" /mktyplib203 /o NUL /win32 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_kbdd_dll.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib /nologo /subsystem:windows /dll /incremental:no\
 /pdb:"$(OUTDIR)\condor_kbdd_dll.pdb" /machine:I386\
 /out:"$(OUTDIR)\condor_kbdd_dll.dll" /implib:"$(OUTDIR)\condor_kbdd_dll.lib"\
 /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\NT-kbdd-dll.obj"

"$(OUTDIR)\condor_kbdd_dll.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 

.c{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<


!IF "$(CFG)" == "condor_kbdd_dll - Win32 Release" || "$(CFG)" ==\
 "condor_kbdd_dll - Win32 Debug"
SOURCE="..\src\condor_kbdd\NT-kbdd-dll.C"

"$(INTDIR)\NT-kbdd-dll.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)



!ENDIF 

