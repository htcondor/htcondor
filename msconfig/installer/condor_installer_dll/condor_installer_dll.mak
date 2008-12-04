# Microsoft Developer Studio Generated NMAKE File, Based on condor_installer_dll.dsp
!IF "$(CFG)" == ""
CFG=condor_installer_dll - Win32 Release
!MESSAGE No configuration specified. Defaulting to condor_installer_dll - Win32 Release.
!ENDIF 

!IF "$(CFG)" != "condor_installer_dll - Win32 Release" && "$(CFG)" != "condor_installer_dll - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_installer_dll.mak" CFG="condor_installer_dll - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_installer_dll - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "condor_installer_dll - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "condor_installer_dll - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

ALL : "$(OUTDIR)\condor_installer_dll.dll"


CLEAN :
	-@erase "$(INTDIR)\condor_installer_dll.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\condor_installer_dll.dll"
	-@erase "$(OUTDIR)\condor_installer_dll.exp"
	-@erase "$(OUTDIR)\condor_installer_dll.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "CONDOR_INSTALLER_DLL_EXPORTS" /Fp"$(INTDIR)\condor_installer_dll.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

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

MTL=midl.exe
MTL_PROJ=/nologo /D "NDEBUG" /mktyplib203 /win32 
RSC=rc.exe
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_installer_dll.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ws2_32.lib msi.lib /nologo /dll /incremental:no /pdb:"$(OUTDIR)\condor_installer_dll.pdb" /machine:I386 /def:".\condor_installer_dll.def" /out:"$(OUTDIR)\condor_installer_dll.dll" /implib:"$(OUTDIR)\condor_installer_dll.lib" 
DEF_FILE= \
	".\condor_installer_dll.def"
LINK32_OBJS= \
	"$(INTDIR)\condor_installer_dll.obj"

"$(OUTDIR)\condor_installer_dll.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "condor_installer_dll - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

ALL : "$(OUTDIR)\condor_installer_dll.dll"


CLEAN :
	-@erase "$(INTDIR)\condor_installer_dll.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\condor_installer_dll.dll"
	-@erase "$(OUTDIR)\condor_installer_dll.exp"
	-@erase "$(OUTDIR)\condor_installer_dll.ilk"
	-@erase "$(OUTDIR)\condor_installer_dll.lib"
	-@erase "$(OUTDIR)\condor_installer_dll.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "CONDOR_INSTALLER_DLL_EXPORTS" /Fp"$(INTDIR)\condor_installer_dll.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

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

MTL=midl.exe
MTL_PROJ=/nologo /D "_DEBUG" /mktyplib203 /win32 
RSC=rc.exe
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_installer_dll.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=msi.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ws2_32.lib /nologo /dll /incremental:yes /pdb:"$(OUTDIR)\condor_installer_dll.pdb" /debug /machine:I386 /def:".\condor_installer_dll.def" /out:"$(OUTDIR)\condor_installer_dll.dll" /implib:"$(OUTDIR)\condor_installer_dll.lib" /pdbtype:sept 
DEF_FILE= \
	".\condor_installer_dll.def"
LINK32_OBJS= \
	"$(INTDIR)\condor_installer_dll.obj"

"$(OUTDIR)\condor_installer_dll.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("condor_installer_dll.dep")
!INCLUDE "condor_installer_dll.dep"
!ELSE 
!MESSAGE Warning: cannot find "condor_installer_dll.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "condor_installer_dll - Win32 Release" || "$(CFG)" == "condor_installer_dll - Win32 Debug"
SOURCE=.\condor_installer_dll.c

"$(INTDIR)\condor_installer_dll.obj" : $(SOURCE) "$(INTDIR)"



!ENDIF 

