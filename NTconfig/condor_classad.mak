# Microsoft Developer Studio Generated NMAKE File, Based on condor_classad.dsp
!IF "$(CFG)" == ""
CFG=condor_classad - Win32 Debug
!MESSAGE No configuration specified. Defaulting to condor_classad - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "condor_classad - Win32 Release" && "$(CFG)" != "condor_classad - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_classad.mak" CFG="condor_classad - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_classad - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "condor_classad - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "condor_classad - Win32 Release"

OUTDIR=.\..\src\condor_classad
INTDIR=.\..\src\condor_classad
# Begin Custom Macros
OutDir=.\..\src\condor_classad
# End Custom Macros

ALL : "$(OUTDIR)\condor_classad.lib"


CLEAN :
	-@erase "$(INTDIR)\ast.obj"
	-@erase "$(INTDIR)\astbase.obj"
	-@erase "$(INTDIR)\attrlist.obj"
	-@erase "$(INTDIR)\buildtable.obj"
	-@erase "$(INTDIR)\classad.obj"
	-@erase "$(INTDIR)\classad_util.obj"
	-@erase "$(INTDIR)\classifiedjobs.obj"
	-@erase "$(INTDIR)\environment.obj"
	-@erase "$(INTDIR)\evaluateOperators.obj"
	-@erase "$(INTDIR)\operators.obj"
	-@erase "$(INTDIR)\parser.obj"
	-@erase "$(INTDIR)\registration.obj"
	-@erase "$(INTDIR)\scanner.obj"
	-@erase "$(INTDIR)\value.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\condor_classad.lib"

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_classad.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\condor_classad.lib" 
LIB32_OBJS= \
	"$(INTDIR)\ast.obj" \
	"$(INTDIR)\astbase.obj" \
	"$(INTDIR)\attrlist.obj" \
	"$(INTDIR)\buildtable.obj" \
	"$(INTDIR)\classad.obj" \
	"$(INTDIR)\classad_util.obj" \
	"$(INTDIR)\classifiedjobs.obj" \
	"$(INTDIR)\environment.obj" \
	"$(INTDIR)\evaluateOperators.obj" \
	"$(INTDIR)\operators.obj" \
	"$(INTDIR)\parser.obj" \
	"$(INTDIR)\registration.obj" \
	"$(INTDIR)\scanner.obj" \
	"$(INTDIR)\value.obj"

"$(OUTDIR)\condor_classad.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ELSEIF  "$(CFG)" == "condor_classad - Win32 Debug"

OUTDIR=.\..\src\condor_classad
INTDIR=.\..\src\condor_classad
# Begin Custom Macros
OutDir=.\..\src\condor_classad
# End Custom Macros

ALL : "$(OUTDIR)\condor_classad.lib"


CLEAN :
	-@erase "$(INTDIR)\ast.obj"
	-@erase "$(INTDIR)\astbase.obj"
	-@erase "$(INTDIR)\attrlist.obj"
	-@erase "$(INTDIR)\buildtable.obj"
	-@erase "$(INTDIR)\classad.obj"
	-@erase "$(INTDIR)\classad_util.obj"
	-@erase "$(INTDIR)\classifiedjobs.obj"
	-@erase "$(INTDIR)\environment.obj"
	-@erase "$(INTDIR)\evaluateOperators.obj"
	-@erase "$(INTDIR)\operators.obj"
	-@erase "$(INTDIR)\parser.obj"
	-@erase "$(INTDIR)\registration.obj"
	-@erase "$(INTDIR)\scanner.obj"
	-@erase "$(INTDIR)\value.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\condor_classad.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MDd /W3 /GX /Z7 /Od /Ob2 /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /Fp"..\src\condor_c++_util/condor_common.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_classad.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\condor_classad.lib" 
LIB32_OBJS= \
	"$(INTDIR)\ast.obj" \
	"$(INTDIR)\astbase.obj" \
	"$(INTDIR)\attrlist.obj" \
	"$(INTDIR)\buildtable.obj" \
	"$(INTDIR)\classad.obj" \
	"$(INTDIR)\classad_util.obj" \
	"$(INTDIR)\classifiedjobs.obj" \
	"$(INTDIR)\environment.obj" \
	"$(INTDIR)\evaluateOperators.obj" \
	"$(INTDIR)\operators.obj" \
	"$(INTDIR)\parser.obj" \
	"$(INTDIR)\registration.obj" \
	"$(INTDIR)\scanner.obj" \
	"$(INTDIR)\value.obj"

"$(OUTDIR)\condor_classad.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ENDIF 


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("condor_classad.dep")
!INCLUDE "condor_classad.dep"
!ELSE 
!MESSAGE Warning: cannot find "condor_classad.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "condor_classad - Win32 Release" || "$(CFG)" == "condor_classad - Win32 Debug"
SOURCE=..\src\condor_classad\ast.C

"$(INTDIR)\ast.obj" : $(SOURCE) "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad\astbase.C

"$(INTDIR)\astbase.obj" : $(SOURCE) "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad\attrlist.C

"$(INTDIR)\attrlist.obj" : $(SOURCE) "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad\buildtable.C

"$(INTDIR)\buildtable.obj" : $(SOURCE) "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad\classad.C

"$(INTDIR)\classad.obj" : $(SOURCE) "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad\classad_util.C

"$(INTDIR)\classad_util.obj" : $(SOURCE) "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad\classifiedjobs.C

"$(INTDIR)\classifiedjobs.obj" : $(SOURCE) "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad\environment.C

"$(INTDIR)\environment.obj" : $(SOURCE) "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad\evaluateOperators.C

"$(INTDIR)\evaluateOperators.obj" : $(SOURCE) "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad\operators.C

"$(INTDIR)\operators.obj" : $(SOURCE) "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad\parser.C

"$(INTDIR)\parser.obj" : $(SOURCE) "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad\registration.C

"$(INTDIR)\registration.obj" : $(SOURCE) "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad\scanner.C

"$(INTDIR)\scanner.obj" : $(SOURCE) "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad\value.C

"$(INTDIR)\value.obj" : $(SOURCE) "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)



!ENDIF 

