# Microsoft Developer Studio Generated NMAKE File, Based on condor_acct.dsp
!IF "$(CFG)" == ""
CFG=condor_acct - Win32 Debug
!MESSAGE No configuration specified. Defaulting to condor_acct - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "condor_acct - Win32 Release" && "$(CFG)" !=\
 "condor_acct - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_acct.mak" CFG="condor_acct - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_acct - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "condor_acct - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

CPP=cl.exe

!IF  "$(CFG)" == "condor_acct - Win32 Release"

OUTDIR=.\..\src\condor_accountant.V6
INTDIR=.\..\src\condor_accountant.V6
# Begin Custom Macros
OutDir=.\..\src\condor_accountant.V6
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_acct.lib"

!ELSE 

ALL : "$(OUTDIR)\condor_acct.lib"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\Accountant.obj"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(OUTDIR)\condor_acct.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MT /W3 /GX /O2 /I "..\src\h" /I "..\src\condor_includes" /I\
 "..\src\condor_c++_util" /D "WIN32" /D "NDEBUG" /D "_WINDOWS"\
 /Fp"..\src\condor_c++_util/condor_common.pch" /Yu"condor_common.h"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 
CPP_OBJS=..\src\condor_accountant.V6/
CPP_SBRS=.
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_acct.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\condor_acct.lib" 
LIB32_OBJS= \
	"$(INTDIR)\Accountant.obj"

"$(OUTDIR)\condor_acct.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ELSEIF  "$(CFG)" == "condor_acct - Win32 Debug"

OUTDIR=.\..\src\condor_accountant.V6
INTDIR=.\..\src\condor_accountant.V6
# Begin Custom Macros
OutDir=.\..\src\condor_accountant.V6
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_acct.lib"

!ELSE 

ALL : "$(OUTDIR)\condor_acct.lib"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\Accountant.obj"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(OUTDIR)\condor_acct.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MTd /W3 /GX /Z7 /Od /I "..\src\h" /I "..\src\condor_includes"\
 /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG" /D "_WINDOWS"\
 /Fp"..\src\condor_c++_util/condor_common.pch" /Yu"condor_common.h"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 
CPP_OBJS=..\src\condor_accountant.V6/
CPP_SBRS=.
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_acct.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\condor_acct.lib" 
LIB32_OBJS= \
	"$(INTDIR)\Accountant.obj"

"$(OUTDIR)\condor_acct.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ENDIF 

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


!IF "$(CFG)" == "condor_acct - Win32 Release" || "$(CFG)" ==\
 "condor_acct - Win32 Debug"
SOURCE=..\src\condor_accountant.V6\Accountant.C
DEP_CPP_ACCOU=\
	"..\src\condor_c++_util\classad_hashtable.h"\
	"..\src\condor_c++_util\classad_log.h"\
	"..\src\condor_c++_util\HashTable.h"\
	"..\src\condor_c++_util\log.h"\
	"..\src\condor_c++_util\log_transaction.h"\
	"..\src\condor_c++_util\mystring.h"\
	"..\src\condor_includes\condor_accountant.h"\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_attrlist.h"\
	"..\src\condor_includes\condor_classad.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_expressions.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_state.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	

"$(INTDIR)\Accountant.obj" : $(SOURCE) $(DEP_CPP_ACCOU) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)



!ENDIF 

