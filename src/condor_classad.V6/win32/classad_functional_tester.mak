# Microsoft Developer Studio Generated NMAKE File, Based on classad_functional_tester.dsp
!IF "$(CFG)" == ""
CFG=classad_functional_tester - Win32 Release
!MESSAGE No configuration specified. Defaulting to classad_functional_tester - Win32 Release.
!ENDIF 

!IF "$(CFG)" != "classad_functional_tester - Win32 Debug" && "$(CFG)" != "classad_functional_tester - Win32 Release"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "classad_functional_tester.mak" CFG="classad_functional_tester - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "classad_functional_tester - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE "classad_functional_tester - Win32 Release" (based on "Win32 (x86) Console Application")
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

!IF  "$(CFG)" == "classad_functional_tester - Win32 Debug"

OUTDIR=.\..\Debug
INTDIR=.\..\Debug
# Begin Custom Macros
OutDir=.\..\Debug
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\classad_functional_tester.exe"

!ELSE 

ALL : "classad_lib - Win32 Debug" "$(OUTDIR)\classad_functional_tester.exe"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"classad_lib - Win32 DebugCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\classad_functional_tester.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\classad_functional_tester.exe"
	-@erase "$(OUTDIR)\classad_functional_tester.ilk"
	-@erase "$(OUTDIR)\classad_functional_tester.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\classad_functional_tester.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=$(CONDOR_LIB) $(CONDOR_LIBPATH) $(CONDOR_GSOAP_LIB) $(CONDOR_GSOAP_LIBPATH) $(CONDOR_KERB_LIB) $(CONDOR_KERB_LIBPATH) $(CONDOR_PCRE_LIB) $(CONDOR_PCRE_LIBPATH) /nologo /subsystem:console /incremental:yes /pdb:"$(OUTDIR)\classad_functional_tester.pdb" /debug /machine:I386 /out:"$(OUTDIR)\classad_functional_tester.exe" /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\classad_functional_tester.obj" \
	"$(OUTDIR)\classad_lib.lib"

"$(OUTDIR)\classad_functional_tester.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "classad_functional_tester - Win32 Release"

OUTDIR=.\..\Release
INTDIR=.\..\Release
# Begin Custom Macros
OutDir=.\..\Release
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\classad_functional_tester.exe"

!ELSE 

ALL : "classad_lib - Win32 Release" "$(OUTDIR)\classad_functional_tester.exe"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"classad_lib - Win32 ReleaseCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\classad_functional_tester.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\classad_functional_tester.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MT /W3 /GX /Z7 /O1 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /D "CLASSAD_DISTRIBUTION" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\classad_functional_tester.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=mswsock.lib $(CONDOR_LIB) $(CONDOR_LIBPATH) $(CONDOR_GSOAP_LIB) $(CONDOR_GSOAP_LIBPATH) $(CONDOR_KERB_LIB) $(CONDOR_KERB_LIBPATH) $(CONDOR_PCRE_LIB) $(CONDOR_PCRE_LIBPATH) /nologo /subsystem:console /pdb:none /debug /machine:I386 /out:"$(OUTDIR)\classad_functional_tester.exe" 
LINK32_OBJS= \
	"$(INTDIR)\classad_functional_tester.obj" \
	"$(OUTDIR)\classad_lib.lib"

"$(OUTDIR)\classad_functional_tester.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
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
!IF EXISTS("classad_functional_tester.dep")
!INCLUDE "classad_functional_tester.dep"
!ELSE 
!MESSAGE Warning: cannot find "classad_functional_tester.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "classad_functional_tester - Win32 Debug" || "$(CFG)" == "classad_functional_tester - Win32 Release"

!IF  "$(CFG)" == "classad_functional_tester - Win32 Debug"

"classad_lib - Win32 Debug" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\classad_lib.mak CFG="classad_lib - Win32 Debug" 
   cd "."

"classad_lib - Win32 DebugCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\classad_lib.mak CFG="classad_lib - Win32 Debug" RECURSE=1 CLEAN 
   cd "."

!ELSEIF  "$(CFG)" == "classad_functional_tester - Win32 Release"

"classad_lib - Win32 Release" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\classad_lib.mak CFG="classad_lib - Win32 Release" 
   cd "."

"classad_lib - Win32 ReleaseCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\classad_lib.mak CFG="classad_lib - Win32 Release" RECURSE=1 CLEAN 
   cd "."

!ENDIF 

SOURCE=..\classad_functional_tester.C

"$(INTDIR)\classad_functional_tester.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)



!ENDIF 

