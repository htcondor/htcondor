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

CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "condor_classad - Win32 Debug"

OUTDIR=.\..\Debug
INTDIR=.\..\Debug
# Begin Custom Macros
OutDir=.\..\Debug
# End Custom Macros

ALL : "$(OUTDIR)\condor_classad.lib"


CLEAN :
	-@erase "$(INTDIR)\attrrefs.obj"
	-@erase "$(INTDIR)\classad.obj"
	-@erase "$(INTDIR)\collection.obj"
	-@erase "$(INTDIR)\collectionBase.obj"
	-@erase "$(INTDIR)\debug.obj"
	-@erase "$(INTDIR)\exprList.obj"
	-@erase "$(INTDIR)\exprTree.obj"
	-@erase "$(INTDIR)\fnCall.obj"
	-@erase "$(INTDIR)\indexfile.obj"
	-@erase "$(INTDIR)\lexer.obj"
	-@erase "$(INTDIR)\lexerSource.obj"
	-@erase "$(INTDIR)\literals.obj"
	-@erase "$(INTDIR)\matchClassad.obj"
	-@erase "$(INTDIR)\operators.obj"
	-@erase "$(INTDIR)\query.obj"
	-@erase "$(INTDIR)\sink.obj"
	-@erase "$(INTDIR)\source.obj"
	-@erase "$(INTDIR)\transaction.obj"
	-@erase "$(INTDIR)\util.obj"
	-@erase "$(INTDIR)\value.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(INTDIR)\view.obj"
	-@erase "$(INTDIR)\xmlLexer.obj"
	-@erase "$(INTDIR)\xmlSink.obj"
	-@erase "$(INTDIR)\xmlSource.obj"
	-@erase "$(OUTDIR)\condor_classad.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MDd /W3 /Gm /Gi /GX /ZI /Od /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /I "..\src\condor_daemon_client" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_classad.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\condor_classad.lib" 
LIB32_OBJS= \
	"$(INTDIR)\collection.obj" \
	"$(INTDIR)\collectionBase.obj" \
	"$(INTDIR)\debug.obj" \
	"$(INTDIR)\exprList.obj" \
	"$(INTDIR)\exprTree.obj" \
	"$(INTDIR)\fnCall.obj" \
	"$(INTDIR)\indexfile.obj" \
	"$(INTDIR)\lexer.obj" \
	"$(INTDIR)\lexerSource.obj" \
	"$(INTDIR)\literals.obj" \
	"$(INTDIR)\matchClassad.obj" \
	"$(INTDIR)\operators.obj" \
	"$(INTDIR)\query.obj" \
	"$(INTDIR)\sink.obj" \
	"$(INTDIR)\source.obj" \
	"$(INTDIR)\transaction.obj" \
	"$(INTDIR)\util.obj" \
	"$(INTDIR)\value.obj" \
	"$(INTDIR)\view.obj" \
	"$(INTDIR)\xmlLexer.obj" \
	"$(INTDIR)\xmlSink.obj" \
	"$(INTDIR)\xmlSource.obj" \
	"$(INTDIR)\attrrefs.obj" \
	"$(INTDIR)\classad.obj"

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

ALL : "$(OUTDIR)\condor_classad.lib"


CLEAN :
	-@erase "$(INTDIR)\attrrefs.obj"
	-@erase "$(INTDIR)\classad.obj"
	-@erase "$(INTDIR)\collection.obj"
	-@erase "$(INTDIR)\collectionBase.obj"
	-@erase "$(INTDIR)\debug.obj"
	-@erase "$(INTDIR)\exprList.obj"
	-@erase "$(INTDIR)\exprTree.obj"
	-@erase "$(INTDIR)\fnCall.obj"
	-@erase "$(INTDIR)\indexfile.obj"
	-@erase "$(INTDIR)\lexer.obj"
	-@erase "$(INTDIR)\lexerSource.obj"
	-@erase "$(INTDIR)\literals.obj"
	-@erase "$(INTDIR)\matchClassad.obj"
	-@erase "$(INTDIR)\operators.obj"
	-@erase "$(INTDIR)\query.obj"
	-@erase "$(INTDIR)\sink.obj"
	-@erase "$(INTDIR)\source.obj"
	-@erase "$(INTDIR)\transaction.obj"
	-@erase "$(INTDIR)\util.obj"
	-@erase "$(INTDIR)\value.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\view.obj"
	-@erase "$(INTDIR)\xmlLexer.obj"
	-@erase "$(INTDIR)\xmlSink.obj"
	-@erase "$(INTDIR)\xmlSource.obj"
	-@erase "$(OUTDIR)\condor_classad.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MD /W3 /GX /Z7 /O1 /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /I "..\src\condor_daemon_client" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "CLASSAD_DISTRIBUTION" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_classad.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\condor_classad.lib" 
LIB32_OBJS= \
	"$(INTDIR)\collection.obj" \
	"$(INTDIR)\collectionBase.obj" \
	"$(INTDIR)\debug.obj" \
	"$(INTDIR)\exprList.obj" \
	"$(INTDIR)\exprTree.obj" \
	"$(INTDIR)\fnCall.obj" \
	"$(INTDIR)\indexfile.obj" \
	"$(INTDIR)\lexer.obj" \
	"$(INTDIR)\lexerSource.obj" \
	"$(INTDIR)\literals.obj" \
	"$(INTDIR)\matchClassad.obj" \
	"$(INTDIR)\operators.obj" \
	"$(INTDIR)\query.obj" \
	"$(INTDIR)\sink.obj" \
	"$(INTDIR)\source.obj" \
	"$(INTDIR)\transaction.obj" \
	"$(INTDIR)\util.obj" \
	"$(INTDIR)\value.obj" \
	"$(INTDIR)\view.obj" \
	"$(INTDIR)\xmlLexer.obj" \
	"$(INTDIR)\xmlSink.obj" \
	"$(INTDIR)\xmlSource.obj" \
	"$(INTDIR)\attrrefs.obj" \
	"$(INTDIR)\classad.obj"

"$(OUTDIR)\condor_classad.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
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
!IF EXISTS("condor_classad.dep")
!INCLUDE "condor_classad.dep"
!ELSE 
!MESSAGE Warning: cannot find "condor_classad.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "condor_classad - Win32 Debug" || "$(CFG)" == "condor_classad - Win32 Release"
SOURCE=..\src\condor_classad.V6\attrrefs.C

"$(INTDIR)\attrrefs.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad.V6\classad.C

"$(INTDIR)\classad.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad.V6\collection.C

"$(INTDIR)\collection.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad.V6\collectionBase.C

"$(INTDIR)\collectionBase.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad.V6\debug.C

"$(INTDIR)\debug.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad.V6\exprList.C

"$(INTDIR)\exprList.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad.V6\exprTree.C

"$(INTDIR)\exprTree.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad.V6\fnCall.C

"$(INTDIR)\fnCall.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad.V6\indexfile.C

"$(INTDIR)\indexfile.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad.V6\lexer.C

"$(INTDIR)\lexer.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad.V6\lexerSource.C

"$(INTDIR)\lexerSource.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad.V6\literals.C

"$(INTDIR)\literals.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad.V6\matchClassad.C

"$(INTDIR)\matchClassad.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad.V6\operators.C

"$(INTDIR)\operators.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad.V6\query.C

"$(INTDIR)\query.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad.V6\sink.C

"$(INTDIR)\sink.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad.V6\source.C

"$(INTDIR)\source.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad.V6\transaction.C

"$(INTDIR)\transaction.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad.V6\util.C

"$(INTDIR)\util.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad.V6\value.C

"$(INTDIR)\value.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad.V6\view.C

"$(INTDIR)\view.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad.V6\xmlLexer.C

"$(INTDIR)\xmlLexer.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad.V6\xmlSink.C

"$(INTDIR)\xmlSink.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_classad.V6\xmlSource.C

"$(INTDIR)\xmlSource.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)



!ENDIF 

