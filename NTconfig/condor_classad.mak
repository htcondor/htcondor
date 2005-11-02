# Microsoft Developer Studio Generated NMAKE File, Based on condor_classad.dsp
!IF "$(CFG)" == ""
CFG=condor_classad - Win32 Release
!MESSAGE No configuration specified. Defaulting to condor_classad - Win32 Release.
!ENDIF 

!IF "$(CFG)" != "condor_classad - Win32 Debug" && "$(CFG)" != "condor_classad - Win32 Release"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_classad.mak" CFG="condor_classad - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_classad - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "condor_classad - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "condor_classad - Win32 Debug"

OUTDIR=.\..\Debug
INTDIR=.\..\Debug
# Begin Custom Macros
OutDir=.\..\Debug
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_classad.lib"

!ELSE 

ALL : "condor_util_lib - Win32 Debug" "condor_cpp_util - Win32 Debug" "$(OUTDIR)\condor_classad.lib"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"condor_cpp_util - Win32 DebugCLEAN" "condor_util_lib - Win32 DebugCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\ast.obj"
	-@erase "$(INTDIR)\astbase.obj"
	-@erase "$(INTDIR)\attrlist.obj"
	-@erase "$(INTDIR)\buildtable.obj"
	-@erase "$(INTDIR)\classad.obj"
	-@erase "$(INTDIR)\classad_list.obj"
	-@erase "$(INTDIR)\classad_util.obj"
	-@erase "$(INTDIR)\classifiedjobs.obj"
	-@erase "$(INTDIR)\environment.obj"
	-@erase "$(INTDIR)\evaluateOperators.obj"
	-@erase "$(INTDIR)\new_classads.obj"
	-@erase "$(INTDIR)\operators.obj"
	-@erase "$(INTDIR)\parser.obj"
	-@erase "$(INTDIR)\registration.obj"
	-@erase "$(INTDIR)\scanner.obj"
	-@erase "$(INTDIR)\value.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(INTDIR)\xml_classads.obj"
	-@erase "$(OUTDIR)\condor_classad.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /Fp"$(INTDIR)\condor_common.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

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
	"$(INTDIR)\classad_list.obj" \
	"$(INTDIR)\classad_util.obj" \
	"$(INTDIR)\classifiedjobs.obj" \
	"$(INTDIR)\environment.obj" \
	"$(INTDIR)\evaluateOperators.obj" \
	"$(INTDIR)\new_classads.obj" \
	"$(INTDIR)\operators.obj" \
	"$(INTDIR)\parser.obj" \
	"$(INTDIR)\registration.obj" \
	"$(INTDIR)\scanner.obj" \
	"$(INTDIR)\value.obj" \
	"$(INTDIR)\xml_classads.obj" \
	"$(OUTDIR)\condor_cpp_util.lib" \
	"..\src\condor_util_lib\condor_util.lib"

"$(OUTDIR)\condor_classad.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ELSEIF  "$(CFG)" == "condor_classad - Win32 Release"

OUTDIR=.\..\Release
INTDIR=.\..\Release
# Begin Custom Macros
OutDir=.\..\Release
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_classad.lib"

!ELSE 

ALL : "condor_util_lib - Win32 Release" "condor_cpp_util - Win32 Release" "$(OUTDIR)\condor_classad.lib"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"condor_cpp_util - Win32 ReleaseCLEAN" "condor_util_lib - Win32 ReleaseCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\ast.obj"
	-@erase "$(INTDIR)\astbase.obj"
	-@erase "$(INTDIR)\attrlist.obj"
	-@erase "$(INTDIR)\buildtable.obj"
	-@erase "$(INTDIR)\classad.obj"
	-@erase "$(INTDIR)\classad_list.obj"
	-@erase "$(INTDIR)\classad_util.obj"
	-@erase "$(INTDIR)\classifiedjobs.obj"
	-@erase "$(INTDIR)\environment.obj"
	-@erase "$(INTDIR)\evaluateOperators.obj"
	-@erase "$(INTDIR)\new_classads.obj"
	-@erase "$(INTDIR)\operators.obj"
	-@erase "$(INTDIR)\parser.obj"
	-@erase "$(INTDIR)\registration.obj"
	-@erase "$(INTDIR)\scanner.obj"
	-@erase "$(INTDIR)\value.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\xml_classads.obj"
	-@erase "$(OUTDIR)\condor_classad.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MT /W3 /GX /Z7 /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /Fp"$(INTDIR)\condor_common.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

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
	"$(INTDIR)\classad_list.obj" \
	"$(INTDIR)\classad_util.obj" \
	"$(INTDIR)\classifiedjobs.obj" \
	"$(INTDIR)\environment.obj" \
	"$(INTDIR)\evaluateOperators.obj" \
	"$(INTDIR)\new_classads.obj" \
	"$(INTDIR)\operators.obj" \
	"$(INTDIR)\parser.obj" \
	"$(INTDIR)\registration.obj" \
	"$(INTDIR)\scanner.obj" \
	"$(INTDIR)\value.obj" \
	"$(INTDIR)\xml_classads.obj" \
	"$(OUTDIR)\condor_cpp_util.lib" \
	"..\src\condor_util_lib\condor_util.lib"

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


!IF "$(CFG)" == "condor_classad - Win32 Debug" || "$(CFG)" == "condor_classad - Win32 Release"

!IF  "$(CFG)" == "condor_classad - Win32 Debug"

"condor_cpp_util - Win32 Debug" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F ".\condor_cpp_util.mak" CFG="condor_cpp_util - Win32 Debug" 
   cd "."

"condor_cpp_util - Win32 DebugCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F ".\condor_cpp_util.mak" CFG="condor_cpp_util - Win32 Debug" RECURSE=1 CLEAN 
   cd "."

!ELSEIF  "$(CFG)" == "condor_classad - Win32 Release"

"condor_cpp_util - Win32 Release" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F ".\condor_cpp_util.mak" CFG="condor_cpp_util - Win32 Release" 
   cd "."

"condor_cpp_util - Win32 ReleaseCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F ".\condor_cpp_util.mak" CFG="condor_cpp_util - Win32 Release" RECURSE=1 CLEAN 
   cd "."

!ENDIF 

!IF  "$(CFG)" == "condor_classad - Win32 Debug"

"condor_util_lib - Win32 Debug" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F ".\condor_util_lib.mak" CFG="condor_util_lib - Win32 Debug" 
   cd "."

"condor_util_lib - Win32 DebugCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F ".\condor_util_lib.mak" CFG="condor_util_lib - Win32 Debug" RECURSE=1 CLEAN 
   cd "."

!ELSEIF  "$(CFG)" == "condor_classad - Win32 Release"

"condor_util_lib - Win32 Release" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F ".\condor_util_lib.mak" CFG="condor_util_lib - Win32 Release" 
   cd "."

"condor_util_lib - Win32 ReleaseCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F ".\condor_util_lib.mak" CFG="condor_util_lib - Win32 Release" RECURSE=1 CLEAN 
   cd "."

!ENDIF 

SOURCE=..\src\condor_classad\ast.C

"$(INTDIR)\ast.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad\astbase.C

"$(INTDIR)\astbase.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad\attrlist.C

"$(INTDIR)\attrlist.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad\buildtable.C

"$(INTDIR)\buildtable.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad\classad.C

"$(INTDIR)\classad.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad\classad_list.C

"$(INTDIR)\classad_list.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad\classad_util.C

"$(INTDIR)\classad_util.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad\classifiedjobs.C

"$(INTDIR)\classifiedjobs.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad\environment.C

"$(INTDIR)\environment.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad\evaluateOperators.C

"$(INTDIR)\evaluateOperators.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad\new_classads.C

"$(INTDIR)\new_classads.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad\operators.C

"$(INTDIR)\operators.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad\parser.C

"$(INTDIR)\parser.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad\registration.C

"$(INTDIR)\registration.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad\scanner.C

"$(INTDIR)\scanner.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad\value.C

"$(INTDIR)\value.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad\xml_classads.C

"$(INTDIR)\xml_classads.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)



!ENDIF 

