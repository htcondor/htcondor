# Microsoft Developer Studio Generated NMAKE File, Based on condor_daemon_core.dsp
!IF "$(CFG)" == ""
CFG=condor_daemon_core - Win32 Debug
!MESSAGE No configuration specified. Defaulting to condor_daemon_core - Win32\
 Debug.
!ENDIF 

!IF "$(CFG)" != "condor_daemon_core - Win32 Release" && "$(CFG)" !=\
 "condor_daemon_core - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_daemon_core.mak"\
 CFG="condor_daemon_core - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_daemon_core - Win32 Release" (based on\
 "Win32 (x86) Static Library")
!MESSAGE "condor_daemon_core - Win32 Debug" (based on\
 "Win32 (x86) Static Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "condor_daemon_core - Win32 Release"

OUTDIR=.\..\src\condor_daemon_core.V6
INTDIR=.\..\src\condor_daemon_core.V6
# Begin Custom Macros
OutDir=.\..\src\condor_daemon_core.V6
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_daemon_core.lib"

!ELSE 

ALL : "$(OUTDIR)\condor_daemon_core.lib"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\condor_ipverify.obj"
	-@erase "$(INTDIR)\daemon_core.obj"
	-@erase "$(INTDIR)\daemon_core_main.obj"
	-@erase "$(INTDIR)\timer_manager.obj"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(OUTDIR)\condor_daemon_core.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MT /W3 /GX /O2 /I "..\src\h" /I "..\src\condor_includes" /I\
 "..\src\condor_c++_util" /D "WIN32" /D "NDEBUG" /D "_WINDOWS"\
 /Fp"..\src\condor_c++_util/condor_common.pch" /Yu"condor_common.h"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 
CPP_OBJS=..\src\condor_daemon_core.V6/
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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_daemon_core.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\condor_daemon_core.lib" 
LIB32_OBJS= \
	"$(INTDIR)\condor_ipverify.obj" \
	"$(INTDIR)\daemon_core.obj" \
	"$(INTDIR)\daemon_core_main.obj" \
	"$(INTDIR)\timer_manager.obj"

"$(OUTDIR)\condor_daemon_core.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ELSEIF  "$(CFG)" == "condor_daemon_core - Win32 Debug"

OUTDIR=.\..\src\condor_daemon_core.V6
INTDIR=.\..\src\condor_daemon_core.V6
# Begin Custom Macros
OutDir=.\..\src\condor_daemon_core.V6
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_daemon_core.lib" "$(OUTDIR)\condor_daemon_core.bsc"

!ELSE 

ALL : "$(OUTDIR)\condor_daemon_core.lib" "$(OUTDIR)\condor_daemon_core.bsc"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\condor_ipverify.obj"
	-@erase "$(INTDIR)\condor_ipverify.sbr"
	-@erase "$(INTDIR)\daemon_core.obj"
	-@erase "$(INTDIR)\daemon_core.sbr"
	-@erase "$(INTDIR)\daemon_core_main.obj"
	-@erase "$(INTDIR)\daemon_core_main.sbr"
	-@erase "$(INTDIR)\timer_manager.obj"
	-@erase "$(INTDIR)\timer_manager.sbr"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(OUTDIR)\condor_daemon_core.bsc"
	-@erase "$(OUTDIR)\condor_daemon_core.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MTd /W3 /GX /Z7 /Od /I "..\src\h" /I "..\src\condor_includes"\
 /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG" /D "_WINDOWS"\
 /FR"$(INTDIR)\\" /Fp"..\src\condor_c++_util/condor_common.pch"\
 /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 
CPP_OBJS=..\src\condor_daemon_core.V6/
CPP_SBRS=..\src\condor_daemon_core.V6/

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_daemon_core.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\condor_ipverify.sbr" \
	"$(INTDIR)\daemon_core.sbr" \
	"$(INTDIR)\daemon_core_main.sbr" \
	"$(INTDIR)\timer_manager.sbr"

"$(OUTDIR)\condor_daemon_core.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\condor_daemon_core.lib" 
LIB32_OBJS= \
	"$(INTDIR)\condor_ipverify.obj" \
	"$(INTDIR)\daemon_core.obj" \
	"$(INTDIR)\daemon_core_main.obj" \
	"$(INTDIR)\timer_manager.obj"

"$(OUTDIR)\condor_daemon_core.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ENDIF 


!IF "$(CFG)" == "condor_daemon_core - Win32 Release" || "$(CFG)" ==\
 "condor_daemon_core - Win32 Debug"
SOURCE=..\src\condor_daemon_core.V6\condor_ipverify.C
DEP_CPP_CONDO=\
	"..\src\condor_c++_util\string_list.h"\
	"..\src\condor_daemon_core.V6\condor_ipverify.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\h\internet.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	{$(INCLUDE)}"condor_ast.h"\
	{$(INCLUDE)}"condor_astbase.h"\
	{$(INCLUDE)}"condor_attrlist.h"\
	{$(INCLUDE)}"condor_classad.h"\
	{$(INCLUDE)}"condor_commands.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"condor_expressions.h"\
	{$(INCLUDE)}"condor_exprtype.h"\
	{$(INCLUDE)}"expr.h"\
	{$(INCLUDE)}"HashTable.h"\
	{$(INCLUDE)}"list.h"\
	{$(INCLUDE)}"proc.h"\
	{$(INCLUDE)}"stream.h"\
	

!IF  "$(CFG)" == "condor_daemon_core - Win32 Release"


"$(INTDIR)\condor_ipverify.obj" : $(SOURCE) $(DEP_CPP_CONDO) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_daemon_core - Win32 Debug"


"$(INTDIR)\condor_ipverify.obj"	"$(INTDIR)\condor_ipverify.sbr" : $(SOURCE)\
 $(DEP_CPP_CONDO) "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_daemon_core.V6\daemon_core.C
DEP_CPP_DAEMO=\
	"..\src\condor_c++_util\daemon_types.h"\
	"..\src\condor_c++_util\get_daemon_addr.h"\
	"..\src\condor_c++_util\string_list.h"\
	"..\src\condor_daemon_core.V6\condor_daemon_core.h"\
	"..\src\condor_daemon_core.V6\condor_ipverify.h"\
	"..\src\condor_daemon_core.V6\condor_timer_manager.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\h\internet.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	{$(INCLUDE)}"buffers.h"\
	{$(INCLUDE)}"condor_commands.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"condor_expressions.h"\
	{$(INCLUDE)}"condor_io.h"\
	{$(INCLUDE)}"expr.h"\
	{$(INCLUDE)}"HashTable.h"\
	{$(INCLUDE)}"list.h"\
	{$(INCLUDE)}"proc.h"\
	{$(INCLUDE)}"reli_sock.h"\
	{$(INCLUDE)}"safe_sock.h"\
	{$(INCLUDE)}"sock.h"\
	{$(INCLUDE)}"sockCache.h"\
	{$(INCLUDE)}"stream.h"\
	

!IF  "$(CFG)" == "condor_daemon_core - Win32 Release"


"$(INTDIR)\daemon_core.obj" : $(SOURCE) $(DEP_CPP_DAEMO) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_daemon_core - Win32 Debug"


"$(INTDIR)\daemon_core.obj"	"$(INTDIR)\daemon_core.sbr" : $(SOURCE)\
 $(DEP_CPP_DAEMO) "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_daemon_core.V6\daemon_core_main.C
DEP_CPP_DAEMON=\
	"..\src\condor_c++_util\condor_version.h"\
	"..\src\condor_c++_util\limit.h"\
	"..\src\condor_c++_util\string_list.h"\
	"..\src\condor_daemon_core.V6\condor_daemon_core.h"\
	"..\src\condor_daemon_core.V6\condor_ipverify.h"\
	"..\src\condor_daemon_core.V6\condor_timer_manager.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	{$(INCLUDE)}"buffers.h"\
	{$(INCLUDE)}"condor_ast.h"\
	{$(INCLUDE)}"condor_astbase.h"\
	{$(INCLUDE)}"condor_attrlist.h"\
	{$(INCLUDE)}"condor_classad.h"\
	{$(INCLUDE)}"condor_commands.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"condor_expressions.h"\
	{$(INCLUDE)}"condor_exprtype.h"\
	{$(INCLUDE)}"condor_io.h"\
	{$(INCLUDE)}"HashTable.h"\
	{$(INCLUDE)}"list.h"\
	{$(INCLUDE)}"proc.h"\
	{$(INCLUDE)}"reli_sock.h"\
	{$(INCLUDE)}"safe_sock.h"\
	{$(INCLUDE)}"sock.h"\
	{$(INCLUDE)}"sockCache.h"\
	{$(INCLUDE)}"stream.h"\
	

!IF  "$(CFG)" == "condor_daemon_core - Win32 Release"


"$(INTDIR)\daemon_core_main.obj" : $(SOURCE) $(DEP_CPP_DAEMON) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_daemon_core - Win32 Debug"


"$(INTDIR)\daemon_core_main.obj"	"$(INTDIR)\daemon_core_main.sbr" : $(SOURCE)\
 $(DEP_CPP_DAEMON) "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_daemon_core.V6\timer_manager.C
DEP_CPP_TIMER=\
	"..\src\condor_c++_util\string_list.h"\
	"..\src\condor_daemon_core.V6\condor_daemon_core.h"\
	"..\src\condor_daemon_core.V6\condor_ipverify.h"\
	"..\src\condor_daemon_core.V6\condor_timer_manager.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	{$(INCLUDE)}"buffers.h"\
	{$(INCLUDE)}"condor_commands.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"condor_io.h"\
	{$(INCLUDE)}"HashTable.h"\
	{$(INCLUDE)}"list.h"\
	{$(INCLUDE)}"proc.h"\
	{$(INCLUDE)}"reli_sock.h"\
	{$(INCLUDE)}"safe_sock.h"\
	{$(INCLUDE)}"sock.h"\
	{$(INCLUDE)}"sockCache.h"\
	{$(INCLUDE)}"stream.h"\
	

!IF  "$(CFG)" == "condor_daemon_core - Win32 Release"


"$(INTDIR)\timer_manager.obj" : $(SOURCE) $(DEP_CPP_TIMER) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_daemon_core - Win32 Debug"


"$(INTDIR)\timer_manager.obj"	"$(INTDIR)\timer_manager.sbr" : $(SOURCE)\
 $(DEP_CPP_TIMER) "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 


!ENDIF 

