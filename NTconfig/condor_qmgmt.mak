# Microsoft Developer Studio Generated NMAKE File, Based on condor_qmgmt.dsp
!IF "$(CFG)" == ""
CFG=condor_qmgmt - Win32 Debug
!MESSAGE No configuration specified. Defaulting to condor_qmgmt - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "condor_qmgmt - Win32 Release" && "$(CFG)" != "condor_qmgmt - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_qmgmt.mak" CFG="condor_qmgmt - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_qmgmt - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "condor_qmgmt - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "condor_qmgmt - Win32 Release"

OUTDIR=.\../src/condor_schedd.V6
INTDIR=.\../src/condor_schedd.V6
# Begin Custom Macros
OutDir=.\../src/condor_schedd.V6
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

CPP=cl.exe
CPP_PROJ=/nologo /MT /W3 /GX /O2 /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /Fp"..\src\condor_c++_util/condor_common.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 

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

!ELSEIF  "$(CFG)" == "condor_qmgmt - Win32 Debug"

OUTDIR=.\../src/condor_schedd.V6
INTDIR=.\../src/condor_schedd.V6
# Begin Custom Macros
OutDir=.\../src/condor_schedd.V6
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

CPP=cl.exe
CPP_PROJ=/nologo /MDd /W3 /GX /Z7 /Od /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /Fp"..\src\condor_c++_util/condor_common.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 

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


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("condor_qmgmt.dep")
!INCLUDE "condor_qmgmt.dep"
!ELSE 
!MESSAGE Warning: cannot find "condor_qmgmt.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "condor_qmgmt - Win32 Release" || "$(CFG)" == "condor_qmgmt - Win32 Debug"
SOURCE=..\src\condor_schedd.V6\qmgmt_common.C

"$(INTDIR)\qmgmt_common.obj" : $(SOURCE) "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_schedd.V6\qmgmt_send_stubs.C

"$(INTDIR)\qmgmt_send_stubs.obj" : $(SOURCE) "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_schedd.V6\qmgr_lib_support.C

"$(INTDIR)\qmgr_lib_support.obj" : $(SOURCE) "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)



!ENDIF 

