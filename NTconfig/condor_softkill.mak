# Microsoft Developer Studio Generated NMAKE File, Based on condor_softkill.dsp
!IF "$(CFG)" == ""
CFG=condor_softkill - Win32 Debug
!MESSAGE No configuration specified. Defaulting to condor_softkill - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "condor_softkill - Win32 Release" && "$(CFG)" != "condor_softkill - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_softkill.mak" CFG="condor_softkill - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_softkill - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "condor_softkill - Win32 Debug" (based on "Win32 (x86) Application")
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

!IF  "$(CFG)" == "condor_softkill - Win32 Release"

OUTDIR=.\../Release
INTDIR=.\../Release
# Begin Custom Macros
OutDir=.\../Release
# End Custom Macros

ALL : "$(OUTDIR)\condor_softkill.exe"


CLEAN :
	-@erase "$(INTDIR)\condor_softkill.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\condor_softkill.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /ML /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /Fp"$(INTDIR)\condor_softkill.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 
MTL_PROJ=/nologo /D "NDEBUG" /mktyplib203 /win32 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_softkill.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib psapi.lib /nologo /subsystem:windows /incremental:no /pdb:"$(OUTDIR)\condor_softkill.pdb" /machine:I386 /out:"$(OUTDIR)\condor_softkill.exe" 
LINK32_OBJS= \
	"$(INTDIR)\condor_softkill.obj"

"$(OUTDIR)\condor_softkill.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "condor_softkill - Win32 Debug"

OUTDIR=.\../Debug
INTDIR=.\../Debug
# Begin Custom Macros
OutDir=.\../Debug
# End Custom Macros

ALL : "$(OUTDIR)\condor_softkill.exe" "$(OUTDIR)\condor_softkill.bsc"


CLEAN :
	-@erase "$(INTDIR)\condor_softkill.obj"
	-@erase "$(INTDIR)\condor_softkill.sbr"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\condor_softkill.bsc"
	-@erase "$(OUTDIR)\condor_softkill.exe"
	-@erase "$(OUTDIR)\condor_softkill.ilk"
	-@erase "$(OUTDIR)\condor_softkill.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MLd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /FR"$(INTDIR)\\" /Fp"$(INTDIR)\condor_softkill.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /GZ /c 
MTL_PROJ=/nologo /D "_DEBUG" /mktyplib203 /win32 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_softkill.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\condor_softkill.sbr"

"$(OUTDIR)\condor_softkill.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib psapi.lib /nologo /subsystem:windows /incremental:yes /pdb:"$(OUTDIR)\condor_softkill.pdb" /debug /machine:I386 /out:"$(OUTDIR)\condor_softkill.exe" /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\condor_softkill.obj"

"$(OUTDIR)\condor_softkill.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
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
!IF EXISTS("condor_softkill.dep")
!INCLUDE "condor_softkill.dep"
!ELSE 
!MESSAGE Warning: cannot find "condor_softkill.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "condor_softkill - Win32 Release" || "$(CFG)" == "condor_softkill - Win32 Debug"
SOURCE=..\src\condor_daemon_core.V6\condor_softkill.C

!IF  "$(CFG)" == "condor_softkill - Win32 Release"


"$(INTDIR)\condor_softkill.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_softkill - Win32 Debug"


"$(INTDIR)\condor_softkill.obj"	"$(INTDIR)\condor_softkill.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 


!ENDIF 

