# Microsoft Developer Studio Generated NMAKE File, Based on condor_qmgmt.dsp
!IF "$(CFG)" == ""
CFG=condor_qmgmt - Win32 Release
!MESSAGE No configuration specified. Defaulting to condor_qmgmt - Win32 Release.
!ENDIF 

!IF "$(CFG)" != "condor_qmgmt - Win32 Debug" && "$(CFG)" != "condor_qmgmt - Win32 Release"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_qmgmt.mak" CFG="condor_qmgmt - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_qmgmt - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "condor_qmgmt - Win32 Release" (based on "Win32 (x86) Static Library")
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

!IF  "$(CFG)" == "condor_qmgmt - Win32 Debug"

OUTDIR=.\..\Debug
INTDIR=.\..\Debug
# Begin Custom Macros
OutDir=.\..\Debug
# End Custom Macros

ALL : "$(OUTDIR)\condor_qmgmt.lib"


CLEAN :
	-@erase "$(INTDIR)\qmgmt_common.obj"
	-@erase "$(INTDIR)\qmgmt_send_stubs.obj"
	-@erase "$(INTDIR)\qmgr_lib_support.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\condor_qmgmt.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /I "..\src\condor_daemon_client" /I "..\src\condor_daemon_core.V6" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /Fp"$(INTDIR)\condor_common.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_qmgmt.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\condor_qmgmt.lib" 
LIB32_OBJS= \
	"$(INTDIR)\qmgmt_common.obj" \
	"$(INTDIR)\qmgmt_send_stubs.obj" \
	"$(INTDIR)\qmgr_lib_support.obj"

"$(OUTDIR)\condor_qmgmt.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ELSEIF  "$(CFG)" == "condor_qmgmt - Win32 Release"

OUTDIR=.\..\Release
INTDIR=.\..\Release
# Begin Custom Macros
OutDir=.\..\Release
# End Custom Macros

ALL : "$(OUTDIR)\condor_qmgmt.lib"


CLEAN :
	-@erase "$(INTDIR)\qmgmt_common.obj"
	-@erase "$(INTDIR)\qmgmt_send_stubs.obj"
	-@erase "$(INTDIR)\qmgr_lib_support.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\condor_qmgmt.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MT /W3 /GX /Z7 /O1 /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /I "..\src\condor_daemon_client" /I "..\src\condor_daemon_core.V6" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /Fp"$(INTDIR)\condor_common.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_qmgmt.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\condor_qmgmt.lib" 
LIB32_OBJS= \
	"$(INTDIR)\qmgmt_common.obj" \
	"$(INTDIR)\qmgmt_send_stubs.obj" \
	"$(INTDIR)\qmgr_lib_support.obj"

"$(OUTDIR)\condor_qmgmt.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
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
!IF EXISTS("condor_qmgmt.dep")
!INCLUDE "condor_qmgmt.dep"
!ELSE 
!MESSAGE Warning: cannot find "condor_qmgmt.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "condor_qmgmt - Win32 Debug" || "$(CFG)" == "condor_qmgmt - Win32 Release"
SOURCE=..\src\condor_schedd.V6\qmgmt_common.C

"$(INTDIR)\qmgmt_common.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_schedd.V6\qmgmt_send_stubs.C

"$(INTDIR)\qmgmt_send_stubs.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_schedd.V6\qmgr_lib_support.C

"$(INTDIR)\qmgr_lib_support.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)



!ENDIF 

