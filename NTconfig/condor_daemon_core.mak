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
	-@erase "$(INTDIR)\exphnd.WIN32.obj"
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
	"$(INTDIR)\exphnd.WIN32.obj" \
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

ALL : "$(OUTDIR)\condor_daemon_core.lib"

!ELSE 

ALL : "$(OUTDIR)\condor_daemon_core.lib"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\condor_ipverify.obj"
	-@erase "$(INTDIR)\daemon_core.obj"
	-@erase "$(INTDIR)\daemon_core_main.obj"
	-@erase "$(INTDIR)\exphnd.WIN32.obj"
	-@erase "$(INTDIR)\timer_manager.obj"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(OUTDIR)\condor_daemon_core.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MTd /W3 /GX /Z7 /Od /I "..\src\h" /I "..\src\condor_includes"\
 /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG" /D "_WINDOWS"\
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
	"$(INTDIR)\exphnd.WIN32.obj" \
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
	"..\src\condor_c++_util\HashTable.h"\
	"..\src\condor_c++_util\list.h"\
	"..\src\condor_c++_util\string_list.h"\
	"..\src\condor_daemon_core.V6\condor_ipverify.h"\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_attrlist.h"\
	"..\src\condor_includes\condor_classad.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_expressions.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\expr.h"\
	"..\src\h\internet.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	

"$(INTDIR)\condor_ipverify.obj" : $(SOURCE) $(DEP_CPP_CONDO) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_daemon_core.V6\daemon_core.C
DEP_CPP_DAEMO=\
	"..\src\condor_c++_util\daemon_types.h"\
	"..\src\condor_c++_util\extArray.h"\
	"..\src\condor_c++_util\get_daemon_addr.h"\
	"..\src\condor_c++_util\HashTable.h"\
	"..\src\condor_c++_util\list.h"\
	"..\src\condor_c++_util\ntsysinfo.h"\
	"..\src\condor_c++_util\string_list.h"\
	"..\src\condor_daemon_core.V6\condor_daemon_core.h"\
	"..\src\condor_daemon_core.V6\condor_ipverify.h"\
	"..\src\condor_daemon_core.V6\condor_timer_manager.h"\
	"..\src\condor_includes\buffers.h"\
	"..\src\condor_includes\condor_adtypes.h"\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_attrlist.h"\
	"..\src\condor_includes\condor_classad.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_expressions.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_io.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\condor_includes\reli_sock.h"\
	"..\src\condor_includes\safe_sock.h"\
	"..\src\condor_includes\sock.h"\
	"..\src\condor_includes\sockCache.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\expr.h"\
	"..\src\h\internet.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	

"$(INTDIR)\daemon_core.obj" : $(SOURCE) $(DEP_CPP_DAEMO) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_daemon_core.V6\daemon_core_main.C
DEP_CPP_DAEMON=\
	"..\src\condor_c++_util\condor_version.h"\
	"..\src\condor_c++_util\extArray.h"\
	"..\src\condor_c++_util\HashTable.h"\
	"..\src\condor_c++_util\limit.h"\
	"..\src\condor_c++_util\list.h"\
	"..\src\condor_c++_util\ntsysinfo.h"\
	"..\src\condor_c++_util\string_list.h"\
	"..\src\condor_daemon_core.V6\condor_daemon_core.h"\
	"..\src\condor_daemon_core.V6\condor_ipverify.h"\
	"..\src\condor_daemon_core.V6\condor_timer_manager.h"\
	"..\src\condor_daemon_core.V6\exphnd.WIN32.h"\
	"..\src\condor_includes\basename.h"\
	"..\src\condor_includes\buffers.h"\
	"..\src\condor_includes\condor_adtypes.h"\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_attrlist.h"\
	"..\src\condor_includes\condor_classad.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_expressions.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_io.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\condor_includes\reli_sock.h"\
	"..\src\condor_includes\safe_sock.h"\
	"..\src\condor_includes\sock.h"\
	"..\src\condor_includes\sockCache.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\sig_install.h"\
	"..\src\h\startup.h"\
	

"$(INTDIR)\daemon_core_main.obj" : $(SOURCE) $(DEP_CPP_DAEMON) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_daemon_core.V6\exphnd.WIN32.C
DEP_CPP_EXPHN=\
	"..\src\condor_daemon_core.V6\exphnd.WIN32.h"\
	"..\src\condor_includes\condor_debug.h"\
	

!IF  "$(CFG)" == "condor_daemon_core - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /O2 /I "..\src\h" /I "..\src\condor_includes"\
 /I "..\src\condor_c++_util" /D "WIN32" /D "NDEBUG" /D "_WINDOWS"\
 /Fp"..\src\condor_c++_util/condor_common.pch" /Yu"condor_common.h"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 

"$(INTDIR)\exphnd.WIN32.obj" : $(SOURCE) $(DEP_CPP_EXPHN) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_daemon_core - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /GX /Z7 /Od /I "..\src\h" /I\
 "..\src\condor_includes" /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG" /D\
 "_WINDOWS" /Fp"..\src\condor_c++_util/condor_common.pch" /Yu"condor_common.h"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 

"$(INTDIR)\exphnd.WIN32.obj" : $(SOURCE) $(DEP_CPP_EXPHN) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=..\src\condor_daemon_core.V6\timer_manager.C
DEP_CPP_TIMER=\
	"..\src\condor_c++_util\extArray.h"\
	"..\src\condor_c++_util\HashTable.h"\
	"..\src\condor_c++_util\list.h"\
	"..\src\condor_c++_util\ntsysinfo.h"\
	"..\src\condor_c++_util\string_list.h"\
	"..\src\condor_daemon_core.V6\condor_daemon_core.h"\
	"..\src\condor_daemon_core.V6\condor_ipverify.h"\
	"..\src\condor_daemon_core.V6\condor_timer_manager.h"\
	"..\src\condor_includes\buffers.h"\
	"..\src\condor_includes\condor_adtypes.h"\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_attrlist.h"\
	"..\src\condor_includes\condor_classad.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_io.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\condor_includes\reli_sock.h"\
	"..\src\condor_includes\safe_sock.h"\
	"..\src\condor_includes\sock.h"\
	"..\src\condor_includes\sockCache.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	

"$(INTDIR)\timer_manager.obj" : $(SOURCE) $(DEP_CPP_TIMER) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)



!ENDIF 

