# Microsoft Developer Studio Generated NMAKE File, Based on condor_api_util.dsp
!IF "$(CFG)" == ""
CFG=condor_api_util - Win32 Release
!MESSAGE No configuration specified. Defaulting to condor_api_util - Win32 Release.
!ENDIF 

!IF "$(CFG)" != "condor_api_util - Win32 Debug" && "$(CFG)" != "condor_api_util - Win32 Release"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_api_util.mak" CFG="condor_api_util - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_api_util - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "condor_api_util - Win32 Release" (based on "Win32 (x86) Static Library")
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

!IF  "$(CFG)" == "condor_api_util - Win32 Debug"

OUTDIR=.\..\Debug
INTDIR=.\..\Debug
# Begin Custom Macros
OutDir=.\..\Debug
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_api.lib"

!ELSE 

ALL : "condor_util_lib - Win32 Debug" "condor_cpp_util - Win32 Debug" "$(OUTDIR)\condor_api.lib"

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
	-@erase "$(INTDIR)\classifiedjobs.obj"
	-@erase "$(INTDIR)\environment.obj"
	-@erase "$(INTDIR)\evaluateOperators.obj"
	-@erase "$(INTDIR)\operators.obj"
	-@erase "$(INTDIR)\parser.obj"
	-@erase "$(INTDIR)\registration.obj"
	-@erase "$(INTDIR)\scanner.obj"
	-@erase "$(INTDIR)\simple_arg.obj"
	-@erase "$(INTDIR)\strupr.obj"
	-@erase "$(INTDIR)\value.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(INTDIR)\xml_classads.obj"
	-@erase "$(OUTDIR)\condor_api.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /D "WIN32" /D "_DEBUG" /Fp"$(INTDIR)\condor_common.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD $(CONDOR_INCLUDE) $(CONDOR_DEFINES) $(CONDOR_CPPARGS) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_api_util.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\condor_api.lib" 
LIB32_OBJS= \
	"$(INTDIR)\ast.obj" \
	"$(INTDIR)\astbase.obj" \
	"$(INTDIR)\attrlist.obj" \
	"$(INTDIR)\buildtable.obj" \
	"$(INTDIR)\classad.obj" \
	"$(INTDIR)\classifiedjobs.obj" \
	"$(INTDIR)\environment.obj" \
	"$(INTDIR)\evaluateOperators.obj" \
	"$(INTDIR)\operators.obj" \
	"$(INTDIR)\parser.obj" \
	"$(INTDIR)\registration.obj" \
	"$(INTDIR)\scanner.obj" \
	"$(INTDIR)\simple_arg.obj" \
	"$(INTDIR)\strupr.obj" \
	"$(INTDIR)\value.obj" \
	"$(INTDIR)\xml_classads.obj" \
	"$(OUTDIR)\condor_cpp_util.lib" \
	"..\src\condor_util_lib\condor_util.lib"

"$(OUTDIR)\condor_api.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ELSEIF  "$(CFG)" == "condor_api_util - Win32 Release"

OUTDIR=.\..\Release
INTDIR=.\..\Release
# Begin Custom Macros
OutDir=.\..\Release
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_api.lib"

!ELSE 

ALL : "condor_util_lib - Win32 Release" "condor_cpp_util - Win32 Release" "$(OUTDIR)\condor_api.lib"

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
	-@erase "$(INTDIR)\classifiedjobs.obj"
	-@erase "$(INTDIR)\environment.obj"
	-@erase "$(INTDIR)\evaluateOperators.obj"
	-@erase "$(INTDIR)\operators.obj"
	-@erase "$(INTDIR)\parser.obj"
	-@erase "$(INTDIR)\registration.obj"
	-@erase "$(INTDIR)\scanner.obj"
	-@erase "$(INTDIR)\simple_arg.obj"
	-@erase "$(INTDIR)\strupr.obj"
	-@erase "$(INTDIR)\value.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\xml_classads.obj"
	-@erase "$(OUTDIR)\condor_api.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MT /W3 /GX /Z7 /O1 /D "WIN32" /D "NDEBUG" /Fp"$(INTDIR)\condor_common.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD $(CONDOR_INCLUDE) $(CONDOR_DEFINES) $(CONDOR_CPPARGS) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_api_util.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\condor_api.lib" 
LIB32_OBJS= \
	"$(INTDIR)\ast.obj" \
	"$(INTDIR)\astbase.obj" \
	"$(INTDIR)\attrlist.obj" \
	"$(INTDIR)\buildtable.obj" \
	"$(INTDIR)\classad.obj" \
	"$(INTDIR)\classifiedjobs.obj" \
	"$(INTDIR)\environment.obj" \
	"$(INTDIR)\evaluateOperators.obj" \
	"$(INTDIR)\operators.obj" \
	"$(INTDIR)\parser.obj" \
	"$(INTDIR)\registration.obj" \
	"$(INTDIR)\scanner.obj" \
	"$(INTDIR)\simple_arg.obj" \
	"$(INTDIR)\strupr.obj" \
	"$(INTDIR)\value.obj" \
	"$(INTDIR)\xml_classads.obj" \
	"$(OUTDIR)\condor_cpp_util.lib" \
	"..\src\condor_util_lib\condor_util.lib"

"$(OUTDIR)\condor_api.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
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
!IF EXISTS("condor_api_util.dep")
!INCLUDE "condor_api_util.dep"
!ELSE 
!MESSAGE Warning: cannot find "condor_api_util.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "condor_api_util - Win32 Debug" || "$(CFG)" == "condor_api_util - Win32 Release"

!IF  "$(CFG)" == "condor_api_util - Win32 Debug"

"condor_cpp_util - Win32 Debug" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_cpp_util.mak CFG="condor_cpp_util - Win32 Debug" 
   cd "."

"condor_cpp_util - Win32 DebugCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_cpp_util.mak CFG="condor_cpp_util - Win32 Debug" RECURSE=1 CLEAN 
   cd "."

!ELSEIF  "$(CFG)" == "condor_api_util - Win32 Release"

"condor_cpp_util - Win32 Release" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_cpp_util.mak CFG="condor_cpp_util - Win32 Release" 
   cd "."

"condor_cpp_util - Win32 ReleaseCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_cpp_util.mak CFG="condor_cpp_util - Win32 Release" RECURSE=1 CLEAN 
   cd "."

!ENDIF 

!IF  "$(CFG)" == "condor_api_util - Win32 Debug"

"condor_util_lib - Win32 Debug" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_util_lib.mak CFG="condor_util_lib - Win32 Debug" 
   cd "."

"condor_util_lib - Win32 DebugCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_util_lib.mak CFG="condor_util_lib - Win32 Debug" RECURSE=1 CLEAN 
   cd "."

!ELSEIF  "$(CFG)" == "condor_api_util - Win32 Release"

"condor_util_lib - Win32 Release" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_util_lib.mak CFG="condor_util_lib - Win32 Release" 
   cd "."

"condor_util_lib - Win32 ReleaseCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_util_lib.mak CFG="condor_util_lib - Win32 Release" RECURSE=1 CLEAN 
   cd "."

!ENDIF 

SOURCE=..\src\classad.old\ast.cpp

"$(INTDIR)\ast.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\classad.old\astbase.cpp"

"$(INTDIR)\astbase.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\classad.old\attrlist.cpp"

"$(INTDIR)\attrlist.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\classad.old\buildtable.cpp"

"$(INTDIR)\buildtable.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\classad.old\classad.cpp"

"$(INTDIR)\classad.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\classad.old\classifiedjobs.cpp"

"$(INTDIR)\classifiedjobs.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\classad.old\environment.cpp"

"$(INTDIR)\environment.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\classad.old\evaluateOperators.cpp"

"$(INTDIR)\evaluateOperators.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\classad.old\operators.cpp"

"$(INTDIR)\operators.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\classad.old\parser.cpp"

"$(INTDIR)\parser.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\classad.old\registration.cpp"

"$(INTDIR)\registration.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\classad.old\scanner.cpp"

"$(INTDIR)\scanner.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\simple_arg.cpp"

"$(INTDIR)\simple_arg.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_util_lib\strupr.c"

!IF  "$(CFG)" == "condor_api_util - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /D "WIN32" /D "_DEBUG" /Fp"$(INTDIR)\condor_common.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD $(CONDOR_INCLUDE) $(CONDOR_DEFINES) $(CONDOR_CPPARGS) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

"$(INTDIR)\strupr.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_api_util - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /Z7 /O1 /D "WIN32" /D "NDEBUG" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD $(CONDOR_INCLUDE) $(CONDOR_DEFINES) $(CONDOR_CPPARGS) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

"$(INTDIR)\strupr.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE="..\src\classad.old\value.cpp"

"$(INTDIR)\value.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\classad.old\xml_classads.cpp"

"$(INTDIR)\xml_classads.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)



!ENDIF 

