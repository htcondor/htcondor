# Microsoft Developer Studio Generated NMAKE File, Based on condor_classad.dsp
!IF "$(CFG)" == ""
CFG=condor_classad - Win32 Debug
!MESSAGE No configuration specified. Defaulting to condor_classad - Win32\
 Debug.
!ENDIF 

!IF "$(CFG)" != "condor_classad - Win32 Release" && "$(CFG)" !=\
 "condor_classad - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_classad.mak" CFG="condor_classad - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_classad - Win32 Release" (based on\
 "Win32 (x86) Static Library")
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

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_classad.lib"

!ELSE 

ALL : "$(OUTDIR)\condor_classad.lib"

!ENDIF 

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
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(OUTDIR)\condor_classad.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MT /W3 /GX /O2 /I "..\src\h" /I "..\src\condor_includes" /I\
 "..\src\condor_c++_util" /D "WIN32" /D "NDEBUG" /D "_WINDOWS"\
 /Fp"..\src\condor_c++_util/condor_common.pch" /Yu"condor_common.h"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 
CPP_OBJS=..\src\condor_classad/
CPP_SBRS=.

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

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_classad.lib"

!ELSE 

ALL : "$(OUTDIR)\condor_classad.lib"

!ENDIF 

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
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(OUTDIR)\condor_classad.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MTd /W3 /GX /Z7 /Od /Ob2 /I "..\src\h" /I\
 "..\src\condor_includes" /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG" /D\
 "_WINDOWS" /Fp"..\src\condor_c++_util/condor_common.pch" /Yu"condor_common.h"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 
CPP_OBJS=..\src\condor_classad/
CPP_SBRS=.

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


!IF "$(CFG)" == "condor_classad - Win32 Release" || "$(CFG)" ==\
 "condor_classad - Win32 Debug"
SOURCE=..\src\condor_classad\ast.C
DEP_CPP_AST_C=\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_attrlist.h"\
	"..\src\condor_includes\condor_buildtable.h"\
	"..\src\condor_includes\condor_classad.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	

"$(INTDIR)\ast.obj" : $(SOURCE) $(DEP_CPP_AST_C) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad\astbase.C

!IF  "$(CFG)" == "condor_classad - Win32 Release"

DEP_CPP_ASTBA=\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	

"$(INTDIR)\astbase.obj" : $(SOURCE) $(DEP_CPP_ASTBA) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_classad - Win32 Debug"

DEP_CPP_ASTBA=\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	

"$(INTDIR)\astbase.obj" : $(SOURCE) $(DEP_CPP_ASTBA) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_classad\attrlist.C
DEP_CPP_ATTRL=\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_attrlist.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	

"$(INTDIR)\attrlist.obj" : $(SOURCE) $(DEP_CPP_ATTRL) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad\buildtable.C

!IF  "$(CFG)" == "condor_classad - Win32 Release"

DEP_CPP_BUILD=\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_buildtable.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	

"$(INTDIR)\buildtable.obj" : $(SOURCE) $(DEP_CPP_BUILD) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_classad - Win32 Debug"

DEP_CPP_BUILD=\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_buildtable.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	

"$(INTDIR)\buildtable.obj" : $(SOURCE) $(DEP_CPP_BUILD) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_classad\classad.C
DEP_CPP_CLASS=\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_attrlist.h"\
	"..\src\condor_includes\condor_classad.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_registration.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	

"$(INTDIR)\classad.obj" : $(SOURCE) $(DEP_CPP_CLASS) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad\classad_util.C

!IF  "$(CFG)" == "condor_classad - Win32 Release"

DEP_CPP_CLASSA=\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_attrlist.h"\
	"..\src\condor_includes\condor_classad.h"\
	"..\src\condor_includes\condor_classad_util.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	

"$(INTDIR)\classad_util.obj" : $(SOURCE) $(DEP_CPP_CLASSA) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_classad - Win32 Debug"

DEP_CPP_CLASSA=\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_attrlist.h"\
	"..\src\condor_includes\condor_classad.h"\
	"..\src\condor_includes\condor_classad_util.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	

"$(INTDIR)\classad_util.obj" : $(SOURCE) $(DEP_CPP_CLASSA) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_classad\classifiedjobs.C
DEP_CPP_CLASSI=\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_attrlist.h"\
	"..\src\condor_includes\condor_classad.h"\
	"..\src\condor_includes\condor_classifiedjobs.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	

"$(INTDIR)\classifiedjobs.obj" : $(SOURCE) $(DEP_CPP_CLASSI) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad\environment.C

!IF  "$(CFG)" == "condor_classad - Win32 Release"

DEP_CPP_ENVIR=\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	

"$(INTDIR)\environment.obj" : $(SOURCE) $(DEP_CPP_ENVIR) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_classad - Win32 Debug"

DEP_CPP_ENVIR=\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	

"$(INTDIR)\environment.obj" : $(SOURCE) $(DEP_CPP_ENVIR) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_classad\evaluateOperators.C

!IF  "$(CFG)" == "condor_classad - Win32 Release"

DEP_CPP_EVALU=\
	"..\src\condor_classad\operators.h"\
	"..\src\condor_classad\value.h"\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_attrlist.h"\
	"..\src\condor_includes\condor_classad.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	

"$(INTDIR)\evaluateOperators.obj" : $(SOURCE) $(DEP_CPP_EVALU) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_classad - Win32 Debug"

DEP_CPP_EVALU=\
	"..\src\condor_classad\operators.h"\
	"..\src\condor_classad\value.h"\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_attrlist.h"\
	"..\src\condor_includes\condor_classad.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	

"$(INTDIR)\evaluateOperators.obj" : $(SOURCE) $(DEP_CPP_EVALU) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_classad\operators.C

!IF  "$(CFG)" == "condor_classad - Win32 Release"

DEP_CPP_OPERA=\
	"..\src\condor_classad\operators.h"\
	"..\src\condor_classad\value.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_debug.h"\
	

"$(INTDIR)\operators.obj" : $(SOURCE) $(DEP_CPP_OPERA) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_classad - Win32 Debug"

DEP_CPP_OPERA=\
	"..\src\condor_classad\operators.h"\
	"..\src\condor_classad\value.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_debug.h"\
	

"$(INTDIR)\operators.obj" : $(SOURCE) $(DEP_CPP_OPERA) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_classad\parser.C

!IF  "$(CFG)" == "condor_classad - Win32 Release"

DEP_CPP_PARSE=\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_attrlist.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_scanner.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	

"$(INTDIR)\parser.obj" : $(SOURCE) $(DEP_CPP_PARSE) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_classad - Win32 Debug"

DEP_CPP_PARSE=\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_attrlist.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_scanner.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	

"$(INTDIR)\parser.obj" : $(SOURCE) $(DEP_CPP_PARSE) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_classad\registration.C

!IF  "$(CFG)" == "condor_classad - Win32 Release"

DEP_CPP_REGIS=\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_registration.h"\
	

"$(INTDIR)\registration.obj" : $(SOURCE) $(DEP_CPP_REGIS) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_classad - Win32 Debug"

DEP_CPP_REGIS=\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_registration.h"\
	

"$(INTDIR)\registration.obj" : $(SOURCE) $(DEP_CPP_REGIS) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_classad\scanner.C

!IF  "$(CFG)" == "condor_classad - Win32 Release"

DEP_CPP_SCANN=\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_attrlist.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_scanner.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	

"$(INTDIR)\scanner.obj" : $(SOURCE) $(DEP_CPP_SCANN) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_classad - Win32 Debug"

DEP_CPP_SCANN=\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_attrlist.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_scanner.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	

"$(INTDIR)\scanner.obj" : $(SOURCE) $(DEP_CPP_SCANN) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_classad\value.C
DEP_CPP_VALUE=\
	"..\src\condor_classad\value.h"\
	"..\src\condor_includes\condor_common.h"\
	

"$(INTDIR)\value.obj" : $(SOURCE) $(DEP_CPP_VALUE) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)



!ENDIF 

