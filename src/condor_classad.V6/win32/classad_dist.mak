# Microsoft Developer Studio Generated NMAKE File, Based on classad_dist.dsp
!IF "$(CFG)" == ""
CFG=classad_dist - Win32 Release
!MESSAGE No configuration specified. Defaulting to classad_dist - Win32 Release.
!ENDIF 

!IF "$(CFG)" != "classad_dist - Win32 Debug" && "$(CFG)" != "classad_dist - Win32 Release"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "classad_dist.mak" CFG="classad_dist - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "classad_dist - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "classad_dist - Win32 Release" (based on "Win32 (x86) Static Library")
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

!IF  "$(CFG)" == "classad_dist - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

ALL : "$(OUTDIR)\classad.lib"


CLEAN :
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
	-@erase "$(OUTDIR)\classad.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MDd /W3 /Gm /Gi /GX /ZI /Od /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "CLASSAD_DISTRIBUTION" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\classad_dist.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\classad.lib" 
LIB32_OBJS= \
	"$(INTDIR)\classad.obj" \
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
	"$(INTDIR)\xmlSource.obj"

"$(OUTDIR)\classad.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ELSEIF  "$(CFG)" == "classad_dist - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

ALL : "$(OUTDIR)\classad.lib"


CLEAN :
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
	-@erase "$(OUTDIR)\classad.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MD /W3 /GX /Z7 /O1 /D "NDEBUG" /D "CLASSAD_DISTRIBUTION" /D "WIN32" /D "_WINDOWS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\classad_dist.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\classad.lib" 
LIB32_OBJS= \
	"$(INTDIR)\classad.obj" \
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
	"$(INTDIR)\xmlSource.obj"

"$(OUTDIR)\classad.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
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
!IF EXISTS("classad_dist.dep")
!INCLUDE "classad_dist.dep"
!ELSE 
!MESSAGE Warning: cannot find "classad_dist.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "classad_dist - Win32 Debug" || "$(CFG)" == "classad_dist - Win32 Release"
SOURCE=..\attrrefs.C

!IF  "$(CFG)" == "classad_dist - Win32 Debug"

InputPath=..\attrrefs.C

"..\classad_features.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	<<tempfile.bat 
	@echo off 
	echo #ifndef __CLASSAD_FEATURES_H > ..\classad_features.h 
	echo #define __CLASSAD_FEATURES_H >> ..\classad_features.h 
	echo #endif >> ..\classad_features.h
<< 
	

!ELSEIF  "$(CFG)" == "classad_dist - Win32 Release"

InputPath=..\attrrefs.C

"..\classad_features.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	<<tempfile.bat 
	@echo off 
	echo #ifndef __CLASSAD_FEATURES_H > ..\classad_features.h 
	echo #define __CLASSAD_FEATURES_H >> ..\classad_features.h 
	echo #endif >> ..\classad_features.h
<< 
	

!ENDIF 

SOURCE=..\classad.C

"$(INTDIR)\classad.obj" : $(SOURCE) "$(INTDIR)" "..\classad_features.h"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\collection.C

"$(INTDIR)\collection.obj" : $(SOURCE) "$(INTDIR)" "..\classad_features.h"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\collectionBase.C

"$(INTDIR)\collectionBase.obj" : $(SOURCE) "$(INTDIR)" "..\classad_features.h"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\debug.C

"$(INTDIR)\debug.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\exprList.C

"$(INTDIR)\exprList.obj" : $(SOURCE) "$(INTDIR)" "..\classad_features.h"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\exprTree.C

"$(INTDIR)\exprTree.obj" : $(SOURCE) "$(INTDIR)" "..\classad_features.h"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\fnCall.C

"$(INTDIR)\fnCall.obj" : $(SOURCE) "$(INTDIR)" "..\classad_features.h"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\indexfile.C

"$(INTDIR)\indexfile.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\lexer.C

"$(INTDIR)\lexer.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\lexerSource.C

"$(INTDIR)\lexerSource.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\literals.C

"$(INTDIR)\literals.obj" : $(SOURCE) "$(INTDIR)" "..\classad_features.h"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\matchClassad.C

"$(INTDIR)\matchClassad.obj" : $(SOURCE) "$(INTDIR)" "..\classad_features.h"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\operators.C

"$(INTDIR)\operators.obj" : $(SOURCE) "$(INTDIR)" "..\classad_features.h"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\query.C

"$(INTDIR)\query.obj" : $(SOURCE) "$(INTDIR)" "..\classad_features.h"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\sink.C

"$(INTDIR)\sink.obj" : $(SOURCE) "$(INTDIR)" "..\classad_features.h"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\source.C

"$(INTDIR)\source.obj" : $(SOURCE) "$(INTDIR)" "..\classad_features.h"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\transaction.C

"$(INTDIR)\transaction.obj" : $(SOURCE) "$(INTDIR)" "..\classad_features.h"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\util.C

"$(INTDIR)\util.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\value.C

"$(INTDIR)\value.obj" : $(SOURCE) "$(INTDIR)" "..\classad_features.h"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\view.C

"$(INTDIR)\view.obj" : $(SOURCE) "$(INTDIR)" "..\classad_features.h"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\xmlLexer.C

"$(INTDIR)\xmlLexer.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\xmlSink.C

"$(INTDIR)\xmlSink.obj" : $(SOURCE) "$(INTDIR)" "..\classad_features.h"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\xmlSource.C

"$(INTDIR)\xmlSource.obj" : $(SOURCE) "$(INTDIR)" "..\classad_features.h"
	$(CPP) $(CPP_PROJ) $(SOURCE)



!ENDIF 

