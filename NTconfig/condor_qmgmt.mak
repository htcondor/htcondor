# Microsoft Developer Studio Generated NMAKE File, Based on condor_qmgmt.dsp
!IF "$(CFG)" == ""
CFG=condor_qmgmt - Win32 Debug
!MESSAGE No configuration specified. Defaulting to condor_qmgmt - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "condor_qmgmt - Win32 Release" && "$(CFG)" !=\
 "condor_qmgmt - Win32 Debug"
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

CPP=cl.exe

!IF  "$(CFG)" == "condor_qmgmt - Win32 Release"

OUTDIR=.\../src/condor_schedd.V6
INTDIR=.\../src/condor_schedd.V6
# Begin Custom Macros
OutDir=.\../src/condor_schedd.V6
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_qmgmt.lib"

!ELSE 

ALL : "$(OUTDIR)\condor_qmgmt.lib"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\qmgmt_common.obj"
	-@erase "$(INTDIR)\qmgmt_send_stubs.obj"
	-@erase "$(INTDIR)\qmgr_lib_support.obj"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(OUTDIR)\condor_qmgmt.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MT /W3 /GX /O2 /I "..\src\h" /I "..\src\condor_includes" /I\
 "..\src\condor_c++_util" /D "WIN32" /D "NDEBUG" /D "_WINDOWS"\
 /Fp"..\src\condor_c++_util/condor_common.pch" /Yu"condor_common.h"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 
CPP_OBJS=../src/condor_schedd.V6/
CPP_SBRS=.
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

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_qmgmt.lib"

!ELSE 

ALL : "$(OUTDIR)\condor_qmgmt.lib"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\qmgmt_common.obj"
	-@erase "$(INTDIR)\qmgmt_send_stubs.obj"
	-@erase "$(INTDIR)\qmgr_lib_support.obj"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(OUTDIR)\condor_qmgmt.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MTd /W3 /GX /Z7 /Od /I "..\src\h" /I "..\src\condor_includes"\
 /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG" /D "_WINDOWS"\
 /Fp"..\src\condor_c++_util/condor_common.pch" /Yu"condor_common.h"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 
CPP_OBJS=../src/condor_schedd.V6/
CPP_SBRS=.
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


!IF "$(CFG)" == "condor_qmgmt - Win32 Release" || "$(CFG)" ==\
 "condor_qmgmt - Win32 Debug"
SOURCE=..\src\condor_schedd.V6\qmgmt_common.C
DEP_CPP_QMGMT=\
	"..\src\condor_c++_util\classad_hashtable.h"\
	"..\src\condor_c++_util\daemon_types.h"\
	"..\src\condor_c++_util\my_hostname.h"\
	"..\src\condor_c++_util\string_list.h"\
	"..\src\condor_daemon_core.V6\condor_daemon_core.h"\
	"..\src\condor_daemon_core.V6\condor_ipverify.h"\
	"..\src\condor_includes\buffers.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_expressions.h"\
	"..\src\condor_includes\condor_qmgr.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\condor_includes\sock.h"\
	"..\src\condor_schedd.V6\prio_rec.h"\
	"..\src\condor_schedd.V6\schedd_main.C"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	{$(INCLUDE)}"condor_ast.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"condor_exprtype.h"\
	{$(INCLUDE)}"HashTable.h"\
	{$(INCLUDE)}"stream.h"\
	

"$(INTDIR)\qmgmt_common.obj" : $(SOURCE) $(DEP_CPP_QMGMT) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_schedd.V6\qmgmt_send_stubs.C
DEP_CPP_QMGMT_=\
	"..\src\condor_c++_util\classad_hashtable.h"\
	"..\src\condor_c++_util\daemon_types.h"\
	"..\src\condor_c++_util\my_hostname.h"\
	"..\src\condor_c++_util\string_list.h"\
	"..\src\condor_daemon_core.V6\condor_daemon_core.h"\
	"..\src\condor_includes\buffers.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_classad.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_expressions.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_qmgr.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\condor_includes\reli_sock.h"\
	"..\src\condor_includes\sock.h"\
	"..\src\condor_schedd.V6\prio_rec.h"\
	"..\src\condor_schedd.V6\qmgmt_constants.h"\
	"..\src\condor_schedd.V6\schedd_main.C"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	{$(INCLUDE)}"condor_ast.h"\
	{$(INCLUDE)}"condor_astbase.h"\
	{$(INCLUDE)}"condor_attrlist.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"condor_exprtype.h"\
	{$(INCLUDE)}"HashTable.h"\
	{$(INCLUDE)}"stream.h"\
	

"$(INTDIR)\qmgmt_send_stubs.obj" : $(SOURCE) $(DEP_CPP_QMGMT_) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_schedd.V6\qmgr_lib_support.C
DEP_CPP_QMGR_=\
	"..\src\condor_c++_util\classad_hashtable.h"\
	"..\src\condor_c++_util\daemon_types.h"\
	"..\src\condor_c++_util\get_daemon_addr.h"\
	"..\src\condor_c++_util\my_hostname.h"\
	"..\src\condor_c++_util\string_list.h"\
	"..\src\condor_daemon_core.V6\condor_daemon_core.h"\
	"..\src\condor_includes\buffers.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_classad.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_expressions.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_qmgr.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\condor_includes\reli_sock.h"\
	"..\src\condor_includes\sock.h"\
	"..\src\condor_schedd.V6\prio_rec.h"\
	"..\src\condor_schedd.V6\schedd_main.C"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	{$(INCLUDE)}"condor_ast.h"\
	{$(INCLUDE)}"condor_astbase.h"\
	{$(INCLUDE)}"condor_attrlist.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"condor_exprtype.h"\
	{$(INCLUDE)}"HashTable.h"\
	{$(INCLUDE)}"stream.h"\
	

"$(INTDIR)\qmgr_lib_support.obj" : $(SOURCE) $(DEP_CPP_QMGR_) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)



!ENDIF 

