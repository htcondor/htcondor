# Microsoft Developer Studio Generated NMAKE File, Based on condor_procapi.dsp
!IF "$(CFG)" == ""
CFG=condor_procapi - Win32 Release
!MESSAGE No configuration specified. Defaulting to condor_procapi - Win32 Release.
!ENDIF 

!IF "$(CFG)" != "condor_procapi - Win32 Debug" && "$(CFG)" != "condor_procapi - Win32 Release"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_procapi.mak" CFG="condor_procapi - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_procapi - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "condor_procapi - Win32 Release" (based on "Win32 (x86) Static Library")
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

!IF  "$(CFG)" == "condor_procapi - Win32 Debug"

OUTDIR=.\..\Debug
INTDIR=.\..\Debug
# Begin Custom Macros
OutDir=.\..\Debug
# End Custom Macros

ALL : "$(OUTDIR)\condor_procapi.lib"


CLEAN :
	-@erase "$(INTDIR)\procapi.obj"
	-@erase "$(INTDIR)\procinterface.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\condor_procapi.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /D "WIN32" /D "_DEBUG" /Fp"$(INTDIR)\condor_common.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_procapi.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\condor_procapi.lib" 
LIB32_OBJS= \
	"$(INTDIR)\procapi.obj" \
	"$(INTDIR)\procinterface.obj"

"$(OUTDIR)\condor_procapi.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ELSEIF  "$(CFG)" == "condor_procapi - Win32 Release"

OUTDIR=.\..\Release
INTDIR=.\..\Release
# Begin Custom Macros
OutDir=.\..\Release
# End Custom Macros

ALL : "$(OUTDIR)\condor_procapi.lib"


CLEAN :
	-@erase "$(INTDIR)\procapi.obj"
	-@erase "$(INTDIR)\procinterface.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\condor_procapi.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MT /W3 /GX /Z7 /O1 /D "WIN32" /D "NDEBUG" /Fp"$(INTDIR)\condor_common.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_procapi.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\condor_procapi.lib" 
LIB32_OBJS= \
	"$(INTDIR)\procapi.obj" \
	"$(INTDIR)\procinterface.obj"

"$(OUTDIR)\condor_procapi.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
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
!IF EXISTS("condor_procapi.dep")
!INCLUDE "condor_procapi.dep"
!ELSE 
!MESSAGE Warning: cannot find "condor_procapi.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "condor_procapi - Win32 Debug" || "$(CFG)" == "condor_procapi - Win32 Release"
SOURCE=..\src\condor_procapi\procapi.C

"$(INTDIR)\procapi.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_procapi\procinterface.C

"$(INTDIR)\procinterface.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)



!ENDIF 

