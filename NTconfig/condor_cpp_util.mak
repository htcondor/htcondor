# Microsoft Developer Studio Generated NMAKE File, Based on condor_cpp_util.dsp
!IF "$(CFG)" == ""
CFG=condor_cpp_util - Win32 Debug
!MESSAGE No configuration specified. Defaulting to condor_cpp_util - Win32\
 Debug.
!ENDIF 

!IF "$(CFG)" != "condor_cpp_util - Win32 Release" && "$(CFG)" !=\
 "condor_cpp_util - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_cpp_util.mak" CFG="condor_cpp_util - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_cpp_util - Win32 Release" (based on\
 "Win32 (x86) Static Library")
!MESSAGE "condor_cpp_util - Win32 Debug" (based on\
 "Win32 (x86) Static Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"

OUTDIR=.\../src/condor_c++_util
INTDIR=.\../src/condor_c++_util
# Begin Custom Macros
OutDir=.\../src/condor_c++_util
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_cpp_util.lib"

!ELSE 

ALL : "$(OUTDIR)\condor_cpp_util.lib"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\access.obj"
	-@erase "$(INTDIR)\ad_printmask.obj"
	-@erase "$(INTDIR)\classad_collection.obj"
	-@erase "$(INTDIR)\classad_hashtable.obj"
	-@erase "$(INTDIR)\classad_log.obj"
	-@erase "$(INTDIR)\condor_common.obj"
	-@erase "$(INTDIR)\condor_common.pch"
	-@erase "$(INTDIR)\condor_config.obj"
	-@erase "$(INTDIR)\condor_event.obj"
	-@erase "$(INTDIR)\condor_q.obj"
	-@erase "$(INTDIR)\condor_query.obj"
	-@erase "$(INTDIR)\condor_state.obj"
	-@erase "$(INTDIR)\condor_version.obj"
	-@erase "$(INTDIR)\config.obj"
	-@erase "$(INTDIR)\daemon.obj"
	-@erase "$(INTDIR)\daemon_types.obj"
	-@erase "$(INTDIR)\directory.obj"
	-@erase "$(INTDIR)\disk.obj"
	-@erase "$(INTDIR)\dynuser.obj"
	-@erase "$(INTDIR)\environ.obj"
	-@erase "$(INTDIR)\file_lock.obj"
	-@erase "$(INTDIR)\file_transfer.obj"
	-@erase "$(INTDIR)\format_time.obj"
	-@erase "$(INTDIR)\generic_query.obj"
	-@erase "$(INTDIR)\get_daemon_addr.obj"
	-@erase "$(INTDIR)\get_full_hostname.obj"
	-@erase "$(INTDIR)\killfamily.obj"
	-@erase "$(INTDIR)\log.obj"
	-@erase "$(INTDIR)\log_transaction.obj"
	-@erase "$(INTDIR)\misc_utils.obj"
	-@erase "$(INTDIR)\my_hostname.obj"
	-@erase "$(INTDIR)\my_subsystem.obj"
	-@erase "$(INTDIR)\my_username.obj"
	-@erase "$(INTDIR)\ntsysinfo.obj"
	-@erase "$(INTDIR)\perm.obj"
	-@erase "$(INTDIR)\string_list.obj"
	-@erase "$(INTDIR)\strnewp.obj"
	-@erase "$(INTDIR)\uids.obj"
	-@erase "$(INTDIR)\up_down.obj"
	-@erase "$(INTDIR)\usagemon.obj"
	-@erase "$(INTDIR)\user_log.obj"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(OUTDIR)\condor_cpp_util.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MT /W3 /GX /O2 /I "..\src\h" /I "..\src\condor_includes" /I\
 "..\src\condor_c++_util" /D "WIN32" /D "NDEBUG" /D "_WINDOWS"\
 /Fp"$(INTDIR)\condor_common.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\"\
 /Fd"$(INTDIR)\\" /FD /TP /c 
CPP_OBJS="../src/condor_c++_util/"
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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_cpp_util.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\condor_cpp_util.lib" 
LIB32_OBJS= \
	"$(INTDIR)\access.obj" \
	"$(INTDIR)\ad_printmask.obj" \
	"$(INTDIR)\classad_collection.obj" \
	"$(INTDIR)\classad_hashtable.obj" \
	"$(INTDIR)\classad_log.obj" \
	"$(INTDIR)\condor_common.obj" \
	"$(INTDIR)\condor_config.obj" \
	"$(INTDIR)\condor_event.obj" \
	"$(INTDIR)\condor_q.obj" \
	"$(INTDIR)\condor_query.obj" \
	"$(INTDIR)\condor_state.obj" \
	"$(INTDIR)\condor_version.obj" \
	"$(INTDIR)\config.obj" \
	"$(INTDIR)\daemon.obj" \
	"$(INTDIR)\daemon_types.obj" \
	"$(INTDIR)\directory.obj" \
	"$(INTDIR)\disk.obj" \
	"$(INTDIR)\dynuser.obj" \
	"$(INTDIR)\environ.obj" \
	"$(INTDIR)\file_lock.obj" \
	"$(INTDIR)\file_transfer.obj" \
	"$(INTDIR)\format_time.obj" \
	"$(INTDIR)\generic_query.obj" \
	"$(INTDIR)\get_daemon_addr.obj" \
	"$(INTDIR)\get_full_hostname.obj" \
	"$(INTDIR)\killfamily.obj" \
	"$(INTDIR)\log.obj" \
	"$(INTDIR)\log_transaction.obj" \
	"$(INTDIR)\misc_utils.obj" \
	"$(INTDIR)\my_hostname.obj" \
	"$(INTDIR)\my_subsystem.obj" \
	"$(INTDIR)\my_username.obj" \
	"$(INTDIR)\ntsysinfo.obj" \
	"$(INTDIR)\perm.obj" \
	"$(INTDIR)\string_list.obj" \
	"$(INTDIR)\strnewp.obj" \
	"$(INTDIR)\uids.obj" \
	"$(INTDIR)\up_down.obj" \
	"$(INTDIR)\usagemon.obj" \
	"$(INTDIR)\user_log.obj"

"$(OUTDIR)\condor_cpp_util.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

OUTDIR=.\../src/condor_c++_util
INTDIR=.\../src/condor_c++_util
# Begin Custom Macros
OutDir=.\../src/condor_c++_util
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_cpp_util.lib"

!ELSE 

ALL : "$(OUTDIR)\condor_cpp_util.lib"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\access.obj"
	-@erase "$(INTDIR)\ad_printmask.obj"
	-@erase "$(INTDIR)\classad_collection.obj"
	-@erase "$(INTDIR)\classad_hashtable.obj"
	-@erase "$(INTDIR)\classad_log.obj"
	-@erase "$(INTDIR)\condor_common.obj"
	-@erase "$(INTDIR)\condor_common.pch"
	-@erase "$(INTDIR)\condor_config.obj"
	-@erase "$(INTDIR)\condor_event.obj"
	-@erase "$(INTDIR)\condor_q.obj"
	-@erase "$(INTDIR)\condor_query.obj"
	-@erase "$(INTDIR)\condor_state.obj"
	-@erase "$(INTDIR)\condor_version.obj"
	-@erase "$(INTDIR)\config.obj"
	-@erase "$(INTDIR)\daemon.obj"
	-@erase "$(INTDIR)\daemon_types.obj"
	-@erase "$(INTDIR)\directory.obj"
	-@erase "$(INTDIR)\disk.obj"
	-@erase "$(INTDIR)\dynuser.obj"
	-@erase "$(INTDIR)\environ.obj"
	-@erase "$(INTDIR)\file_lock.obj"
	-@erase "$(INTDIR)\file_transfer.obj"
	-@erase "$(INTDIR)\format_time.obj"
	-@erase "$(INTDIR)\generic_query.obj"
	-@erase "$(INTDIR)\get_daemon_addr.obj"
	-@erase "$(INTDIR)\get_full_hostname.obj"
	-@erase "$(INTDIR)\killfamily.obj"
	-@erase "$(INTDIR)\log.obj"
	-@erase "$(INTDIR)\log_transaction.obj"
	-@erase "$(INTDIR)\misc_utils.obj"
	-@erase "$(INTDIR)\my_hostname.obj"
	-@erase "$(INTDIR)\my_subsystem.obj"
	-@erase "$(INTDIR)\my_username.obj"
	-@erase "$(INTDIR)\ntsysinfo.obj"
	-@erase "$(INTDIR)\perm.obj"
	-@erase "$(INTDIR)\string_list.obj"
	-@erase "$(INTDIR)\strnewp.obj"
	-@erase "$(INTDIR)\uids.obj"
	-@erase "$(INTDIR)\up_down.obj"
	-@erase "$(INTDIR)\usagemon.obj"
	-@erase "$(INTDIR)\user_log.obj"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(OUTDIR)\condor_cpp_util.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MTd /W3 /GX /Z7 /Od /I "..\src\h" /I "..\src\condor_includes"\
 /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG"\
 /Fp"$(INTDIR)\condor_common.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\"\
 /Fd"$(INTDIR)\\" /FD /TP /c 
CPP_OBJS="../src/condor_c++_util/"
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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_cpp_util.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\condor_cpp_util.lib" 
LIB32_OBJS= \
	"$(INTDIR)\access.obj" \
	"$(INTDIR)\ad_printmask.obj" \
	"$(INTDIR)\classad_collection.obj" \
	"$(INTDIR)\classad_hashtable.obj" \
	"$(INTDIR)\classad_log.obj" \
	"$(INTDIR)\condor_common.obj" \
	"$(INTDIR)\condor_config.obj" \
	"$(INTDIR)\condor_event.obj" \
	"$(INTDIR)\condor_q.obj" \
	"$(INTDIR)\condor_query.obj" \
	"$(INTDIR)\condor_state.obj" \
	"$(INTDIR)\condor_version.obj" \
	"$(INTDIR)\config.obj" \
	"$(INTDIR)\daemon.obj" \
	"$(INTDIR)\daemon_types.obj" \
	"$(INTDIR)\directory.obj" \
	"$(INTDIR)\disk.obj" \
	"$(INTDIR)\dynuser.obj" \
	"$(INTDIR)\environ.obj" \
	"$(INTDIR)\file_lock.obj" \
	"$(INTDIR)\file_transfer.obj" \
	"$(INTDIR)\format_time.obj" \
	"$(INTDIR)\generic_query.obj" \
	"$(INTDIR)\get_daemon_addr.obj" \
	"$(INTDIR)\get_full_hostname.obj" \
	"$(INTDIR)\killfamily.obj" \
	"$(INTDIR)\log.obj" \
	"$(INTDIR)\log_transaction.obj" \
	"$(INTDIR)\misc_utils.obj" \
	"$(INTDIR)\my_hostname.obj" \
	"$(INTDIR)\my_subsystem.obj" \
	"$(INTDIR)\my_username.obj" \
	"$(INTDIR)\ntsysinfo.obj" \
	"$(INTDIR)\perm.obj" \
	"$(INTDIR)\string_list.obj" \
	"$(INTDIR)\strnewp.obj" \
	"$(INTDIR)\uids.obj" \
	"$(INTDIR)\up_down.obj" \
	"$(INTDIR)\usagemon.obj" \
	"$(INTDIR)\user_log.obj"

"$(OUTDIR)\condor_cpp_util.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ENDIF 


!IF "$(CFG)" == "condor_cpp_util - Win32 Release" || "$(CFG)" ==\
 "condor_cpp_util - Win32 Debug"
SOURCE="..\src\condor_c++_util\access.C"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"

DEP_CPP_ACCES=\
	"..\src\condor_c++_util\access.h"\
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
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_io.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\condor_includes\reli_sock.h"\
	"..\src\condor_includes\safe_sock.h"\
	"..\src\condor_includes\sock.h"\
	"..\src\condor_includes\sockCache.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	

"$(INTDIR)\access.obj" : $(SOURCE) $(DEP_CPP_ACCES) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

DEP_CPP_ACCES=\
	"..\src\condor_c++_util\access.h"\
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
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_io.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\condor_includes\reli_sock.h"\
	"..\src\condor_includes\safe_sock.h"\
	"..\src\condor_includes\sock.h"\
	"..\src\condor_includes\sockCache.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	

"$(INTDIR)\access.obj" : $(SOURCE) $(DEP_CPP_ACCES) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\ad_printmask.C"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"

DEP_CPP_AD_PR=\
	"..\src\condor_c++_util\ad_printmask.h"\
	"..\src\condor_c++_util\list.h"\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_attrlist.h"\
	"..\src\condor_includes\condor_classad.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\escapes.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	

"$(INTDIR)\ad_printmask.obj" : $(SOURCE) $(DEP_CPP_AD_PR) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

DEP_CPP_AD_PR=\
	"..\src\condor_c++_util\ad_printmask.h"\
	"..\src\condor_c++_util\list.h"\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_attrlist.h"\
	"..\src\condor_includes\condor_classad.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\escapes.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	

"$(INTDIR)\ad_printmask.obj" : $(SOURCE) $(DEP_CPP_AD_PR) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\classad_collection.C"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"

DEP_CPP_CLASS=\
	"..\src\condor_c++_util\classad_collection.h"\
	"..\src\condor_c++_util\classad_collection_types.h"\
	"..\src\condor_c++_util\classad_hashtable.h"\
	"..\src\condor_c++_util\classad_log.h"\
	"..\src\condor_c++_util\HashTable.h"\
	"..\src\condor_c++_util\log.h"\
	"..\src\condor_c++_util\log_transaction.h"\
	"..\src\condor_c++_util\MyString.h"\
	"..\src\condor_c++_util\Set.h"\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_attrlist.h"\
	"..\src\condor_includes\condor_classad.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	

"$(INTDIR)\classad_collection.obj" : $(SOURCE) $(DEP_CPP_CLASS) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

DEP_CPP_CLASS=\
	"..\src\condor_c++_util\classad_collection.h"\
	"..\src\condor_c++_util\classad_collection_types.h"\
	"..\src\condor_c++_util\classad_hashtable.h"\
	"..\src\condor_c++_util\classad_log.h"\
	"..\src\condor_c++_util\HashTable.h"\
	"..\src\condor_c++_util\log.h"\
	"..\src\condor_c++_util\log_transaction.h"\
	"..\src\condor_c++_util\MyString.h"\
	"..\src\condor_c++_util\Set.h"\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_attrlist.h"\
	"..\src\condor_includes\condor_classad.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	

"$(INTDIR)\classad_collection.obj" : $(SOURCE) $(DEP_CPP_CLASS) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\classad_hashtable.C"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"

DEP_CPP_CLASSA=\
	"..\src\condor_c++_util\classad_hashtable.h"\
	"..\src\condor_c++_util\HashTable.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\classad_hashtable.obj" : $(SOURCE) $(DEP_CPP_CLASSA) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

DEP_CPP_CLASSA=\
	"..\src\condor_c++_util\classad_hashtable.h"\
	"..\src\condor_c++_util\HashTable.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\classad_hashtable.obj" : $(SOURCE) $(DEP_CPP_CLASSA) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\classad_log.C"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"

DEP_CPP_CLASSAD=\
	"..\src\condor_c++_util\classad_hashtable.h"\
	"..\src\condor_c++_util\classad_log.h"\
	"..\src\condor_c++_util\HashTable.h"\
	"..\src\condor_c++_util\log.h"\
	"..\src\condor_c++_util\log_transaction.h"\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_attrlist.h"\
	"..\src\condor_includes\condor_ckpt_name.h"\
	"..\src\condor_includes\condor_classad.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_expressions.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_getmnt.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_status.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\condor_types.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	"..\src\h\util_lib_proto.h"\
	

"$(INTDIR)\classad_log.obj" : $(SOURCE) $(DEP_CPP_CLASSAD) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

DEP_CPP_CLASSAD=\
	"..\src\condor_c++_util\classad_hashtable.h"\
	"..\src\condor_c++_util\classad_log.h"\
	"..\src\condor_c++_util\HashTable.h"\
	"..\src\condor_c++_util\log.h"\
	"..\src\condor_c++_util\log_transaction.h"\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_attrlist.h"\
	"..\src\condor_includes\condor_ckpt_name.h"\
	"..\src\condor_includes\condor_classad.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_expressions.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_getmnt.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_status.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\condor_types.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	"..\src\h\util_lib_proto.h"\
	

"$(INTDIR)\classad_log.obj" : $(SOURCE) $(DEP_CPP_CLASSAD) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\condor_common.C"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"

DEP_CPP_CONDO=\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_file_lock.h"\
	"..\src\condor_includes\condor_fix_assert.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_macros.h"\
	"..\src\condor_includes\condor_sys_nt.h"\
	"..\src\condor_includes\condor_system.h"\
	"..\src\condor_sysapi\sysapi.h"\
	"..\src\h\fake_flock.h"\
	"..\src\h\file_lock.h"\
	
CPP_SWITCHES=/nologo /MT /W3 /Gi- /GX /O2 /I "..\src\h" /I\
 "..\src\condor_includes" /I "..\src\condor_c++_util" /D "WIN32" /D "NDEBUG" /D\
 "_WINDOWS" /Fp"$(INTDIR)\condor_common.pch" /Yc"condor_common.h"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 

"$(INTDIR)\condor_common.obj"	"$(INTDIR)\condor_common.pch" : $(SOURCE)\
 $(DEP_CPP_CONDO) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

DEP_CPP_CONDO=\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_file_lock.h"\
	"..\src\condor_includes\condor_fix_assert.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_macros.h"\
	"..\src\condor_includes\condor_sys_nt.h"\
	"..\src\condor_includes\condor_system.h"\
	"..\src\condor_sysapi\sysapi.h"\
	"..\src\h\fake_flock.h"\
	"..\src\h\file_lock.h"\
	
CPP_SWITCHES=/nologo /MTd /W3 /Gi- /GX /Z7 /Od /I "..\src\h" /I\
 "..\src\condor_includes" /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG"\
 /Fp"$(INTDIR)\condor_common.pch" /Yc"condor_common.h" /Fo"$(INTDIR)\\"\
 /Fd"$(INTDIR)\\" /FD /TP /c 

"$(INTDIR)\condor_common.obj"	"$(INTDIR)\condor_common.pch" : $(SOURCE)\
 $(DEP_CPP_CONDO) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE="..\src\condor_c++_util\condor_config.C"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"

DEP_CPP_CONDOR=\
	"..\src\condor_c++_util\condor_version.h"\
	"..\src\condor_c++_util\extArray.h"\
	"..\src\condor_c++_util\list.h"\
	"..\src\condor_c++_util\my_hostname.h"\
	"..\src\condor_c++_util\string_list.h"\
	"..\src\condor_includes\basename.h"\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_attrlist.h"\
	"..\src\condor_includes\condor_ckpt_name.h"\
	"..\src\condor_includes\condor_classad.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_expressions.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_getmnt.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_status.h"\
	"..\src\condor_includes\condor_string.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\clib.h"\
	"..\src\h\condor_sys.h"\
	"..\src\h\condor_types.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	"..\src\h\syscall_numbers.h"\
	"..\src\h\util_lib_proto.h"\
	

"$(INTDIR)\condor_config.obj" : $(SOURCE) $(DEP_CPP_CONDOR) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

DEP_CPP_CONDOR=\
	"..\src\condor_c++_util\condor_version.h"\
	"..\src\condor_c++_util\extArray.h"\
	"..\src\condor_c++_util\list.h"\
	"..\src\condor_c++_util\my_hostname.h"\
	"..\src\condor_c++_util\string_list.h"\
	"..\src\condor_includes\basename.h"\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_attrlist.h"\
	"..\src\condor_includes\condor_ckpt_name.h"\
	"..\src\condor_includes\condor_classad.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_expressions.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_getmnt.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_status.h"\
	"..\src\condor_includes\condor_string.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\clib.h"\
	"..\src\h\condor_sys.h"\
	"..\src\h\condor_types.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	"..\src\h\syscall_numbers.h"\
	"..\src\h\util_lib_proto.h"\
	

"$(INTDIR)\condor_config.obj" : $(SOURCE) $(DEP_CPP_CONDOR) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\condor_event.C"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"

DEP_CPP_CONDOR_=\
	"..\src\condor_c++_util\condor_event.h"\
	"..\src\condor_c++_util\user_log.c++.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\condor_event.obj" : $(SOURCE) $(DEP_CPP_CONDOR_) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

DEP_CPP_CONDOR_=\
	"..\src\condor_c++_util\condor_event.h"\
	"..\src\condor_c++_util\user_log.c++.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\condor_event.obj" : $(SOURCE) $(DEP_CPP_CONDOR_) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\condor_q.C"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"

DEP_CPP_CONDOR_Q=\
	"..\src\condor_c++_util\condor_q.h"\
	"..\src\condor_c++_util\format_time.h"\
	"..\src\condor_c++_util\generic_query.h"\
	"..\src\condor_c++_util\list.h"\
	"..\src\condor_c++_util\query_result_type.h"\
	"..\src\condor_c++_util\simplelist.h"\
	"..\src\condor_includes\condor_adtypes.h"\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_attrlist.h"\
	"..\src\condor_includes\condor_classad.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_qmgr.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	

"$(INTDIR)\condor_q.obj" : $(SOURCE) $(DEP_CPP_CONDOR_Q) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

DEP_CPP_CONDOR_Q=\
	"..\src\condor_c++_util\condor_q.h"\
	"..\src\condor_c++_util\format_time.h"\
	"..\src\condor_c++_util\generic_query.h"\
	"..\src\condor_c++_util\list.h"\
	"..\src\condor_c++_util\query_result_type.h"\
	"..\src\condor_c++_util\simplelist.h"\
	"..\src\condor_includes\condor_adtypes.h"\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_attrlist.h"\
	"..\src\condor_includes\condor_classad.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_qmgr.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	

"$(INTDIR)\condor_q.obj" : $(SOURCE) $(DEP_CPP_CONDOR_Q) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\condor_query.C"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"

DEP_CPP_CONDOR_QU=\
	"..\src\condor_c++_util\condor_query.h"\
	"..\src\condor_c++_util\daemon_types.h"\
	"..\src\condor_c++_util\generic_query.h"\
	"..\src\condor_c++_util\get_daemon_addr.h"\
	"..\src\condor_c++_util\list.h"\
	"..\src\condor_c++_util\query_result_type.h"\
	"..\src\condor_c++_util\simplelist.h"\
	"..\src\condor_includes\buffers.h"\
	"..\src\condor_includes\condor_adtypes.h"\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_attrlist.h"\
	"..\src\condor_includes\condor_classad.h"\
	"..\src\condor_includes\condor_collector.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_expressions.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_io.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_parser.h"\
	"..\src\condor_includes\reli_sock.h"\
	"..\src\condor_includes\safe_sock.h"\
	"..\src\condor_includes\sock.h"\
	"..\src\condor_includes\sockCache.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	

"$(INTDIR)\condor_query.obj" : $(SOURCE) $(DEP_CPP_CONDOR_QU) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

DEP_CPP_CONDOR_QU=\
	"..\src\condor_c++_util\condor_query.h"\
	"..\src\condor_c++_util\daemon_types.h"\
	"..\src\condor_c++_util\generic_query.h"\
	"..\src\condor_c++_util\get_daemon_addr.h"\
	"..\src\condor_c++_util\list.h"\
	"..\src\condor_c++_util\query_result_type.h"\
	"..\src\condor_c++_util\simplelist.h"\
	"..\src\condor_includes\buffers.h"\
	"..\src\condor_includes\condor_adtypes.h"\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_attrlist.h"\
	"..\src\condor_includes\condor_classad.h"\
	"..\src\condor_includes\condor_collector.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_expressions.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_io.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_parser.h"\
	"..\src\condor_includes\reli_sock.h"\
	"..\src\condor_includes\safe_sock.h"\
	"..\src\condor_includes\sock.h"\
	"..\src\condor_includes\sockCache.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	

"$(INTDIR)\condor_query.obj" : $(SOURCE) $(DEP_CPP_CONDOR_QU) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\condor_state.C"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"

DEP_CPP_CONDOR_S=\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_state.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\condor_state.obj" : $(SOURCE) $(DEP_CPP_CONDOR_S) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

DEP_CPP_CONDOR_S=\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_state.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\condor_state.obj" : $(SOURCE) $(DEP_CPP_CONDOR_S) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\condor_version.C"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /O2 /I "..\src\h" /I "..\src\condor_includes"\
 /I "..\src\condor_c++_util" /D "WIN32" /D "NDEBUG" /D "_WINDOWS"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 

"$(INTDIR)\condor_version.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /GX /Z7 /Od /I "..\src\h" /I\
 "..\src\condor_includes" /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 

"$(INTDIR)\condor_version.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE="..\src\condor_c++_util\config.C"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"

DEP_CPP_CONFI=\
	"..\src\condor_includes\basename.h"\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_attrlist.h"\
	"..\src\condor_includes\condor_ckpt_name.h"\
	"..\src\condor_includes\condor_classad.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_expressions.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_getmnt.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_status.h"\
	"..\src\condor_includes\condor_string.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\condor_types.h"\
	"..\src\h\expr.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	"..\src\h\util_lib_proto.h"\
	

"$(INTDIR)\config.obj" : $(SOURCE) $(DEP_CPP_CONFI) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

DEP_CPP_CONFI=\
	"..\src\condor_includes\basename.h"\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_attrlist.h"\
	"..\src\condor_includes\condor_ckpt_name.h"\
	"..\src\condor_includes\condor_classad.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_expressions.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_getmnt.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_status.h"\
	"..\src\condor_includes\condor_string.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\condor_types.h"\
	"..\src\h\expr.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	"..\src\h\util_lib_proto.h"\
	

"$(INTDIR)\config.obj" : $(SOURCE) $(DEP_CPP_CONFI) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\daemon.C"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"

DEP_CPP_DAEMO=\
	"..\src\condor_c++_util\condor_query.h"\
	"..\src\condor_c++_util\daemon.h"\
	"..\src\condor_c++_util\daemon_types.h"\
	"..\src\condor_c++_util\generic_query.h"\
	"..\src\condor_c++_util\get_daemon_addr.h"\
	"..\src\condor_c++_util\get_full_hostname.h"\
	"..\src\condor_c++_util\list.h"\
	"..\src\condor_c++_util\my_hostname.h"\
	"..\src\condor_c++_util\query_result_type.h"\
	"..\src\condor_c++_util\simplelist.h"\
	"..\src\condor_includes\basename.h"\
	"..\src\condor_includes\buffers.h"\
	"..\src\condor_includes\condor_adtypes.h"\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_attrlist.h"\
	"..\src\condor_includes\condor_ckpt_name.h"\
	"..\src\condor_includes\condor_classad.h"\
	"..\src\condor_includes\condor_collector.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_expressions.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_getmnt.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_io.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_status.h"\
	"..\src\condor_includes\condor_string.h"\
	"..\src\condor_includes\reli_sock.h"\
	"..\src\condor_includes\safe_sock.h"\
	"..\src\condor_includes\sock.h"\
	"..\src\condor_includes\sockCache.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\condor_types.h"\
	"..\src\h\expr.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\internet.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	"..\src\h\util_lib_proto.h"\
	

"$(INTDIR)\daemon.obj" : $(SOURCE) $(DEP_CPP_DAEMO) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

DEP_CPP_DAEMO=\
	"..\src\condor_c++_util\condor_query.h"\
	"..\src\condor_c++_util\daemon.h"\
	"..\src\condor_c++_util\daemon_types.h"\
	"..\src\condor_c++_util\generic_query.h"\
	"..\src\condor_c++_util\get_daemon_addr.h"\
	"..\src\condor_c++_util\get_full_hostname.h"\
	"..\src\condor_c++_util\list.h"\
	"..\src\condor_c++_util\my_hostname.h"\
	"..\src\condor_c++_util\query_result_type.h"\
	"..\src\condor_c++_util\simplelist.h"\
	"..\src\condor_includes\basename.h"\
	"..\src\condor_includes\buffers.h"\
	"..\src\condor_includes\condor_adtypes.h"\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_attrlist.h"\
	"..\src\condor_includes\condor_ckpt_name.h"\
	"..\src\condor_includes\condor_classad.h"\
	"..\src\condor_includes\condor_collector.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_expressions.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_getmnt.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_io.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_status.h"\
	"..\src\condor_includes\condor_string.h"\
	"..\src\condor_includes\reli_sock.h"\
	"..\src\condor_includes\safe_sock.h"\
	"..\src\condor_includes\sock.h"\
	"..\src\condor_includes\sockCache.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\condor_types.h"\
	"..\src\h\expr.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\internet.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	"..\src\h\util_lib_proto.h"\
	

"$(INTDIR)\daemon.obj" : $(SOURCE) $(DEP_CPP_DAEMO) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\daemon_types.C"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"

DEP_CPP_DAEMON=\
	"..\src\condor_c++_util\daemon_types.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\daemon_types.obj" : $(SOURCE) $(DEP_CPP_DAEMON) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

DEP_CPP_DAEMON=\
	"..\src\condor_c++_util\daemon_types.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\daemon_types.obj" : $(SOURCE) $(DEP_CPP_DAEMON) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\directory.C"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"

DEP_CPP_DIREC=\
	"..\src\condor_c++_util\directory.h"\
	"..\src\condor_includes\basename.h"\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_attrlist.h"\
	"..\src\condor_includes\condor_ckpt_name.h"\
	"..\src\condor_includes\condor_classad.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_expressions.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_getmnt.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_status.h"\
	"..\src\condor_includes\condor_string.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\condor_types.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	"..\src\h\util_lib_proto.h"\
	

"$(INTDIR)\directory.obj" : $(SOURCE) $(DEP_CPP_DIREC) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

DEP_CPP_DIREC=\
	"..\src\condor_c++_util\directory.h"\
	"..\src\condor_includes\basename.h"\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_attrlist.h"\
	"..\src\condor_includes\condor_ckpt_name.h"\
	"..\src\condor_includes\condor_classad.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_expressions.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_getmnt.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_status.h"\
	"..\src\condor_includes\condor_string.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\condor_types.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	"..\src\h\util_lib_proto.h"\
	

"$(INTDIR)\directory.obj" : $(SOURCE) $(DEP_CPP_DIREC) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\disk.C"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"

DEP_CPP_DISK_=\
	"..\src\condor_c++_util\condor_disk.h"\
	"..\src\condor_c++_util\condor_updown.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\h\debug.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\disk.obj" : $(SOURCE) $(DEP_CPP_DISK_) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

DEP_CPP_DISK_=\
	"..\src\condor_c++_util\condor_disk.h"\
	"..\src\condor_c++_util\condor_updown.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\h\debug.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\disk.obj" : $(SOURCE) $(DEP_CPP_DISK_) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\dynuser.C"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"

DEP_CPP_DYNUS=\
	"..\src\condor_c++_util\dynuser.h"\
	"..\src\condor_c++_util\ntsecapi.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\dynuser.obj" : $(SOURCE) $(DEP_CPP_DYNUS) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

DEP_CPP_DYNUS=\
	"..\src\condor_c++_util\dynuser.h"\
	"..\src\condor_c++_util\ntsecapi.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\dynuser.obj" : $(SOURCE) $(DEP_CPP_DYNUS) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\environ.C"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"

DEP_CPP_ENVIR=\
	"..\src\condor_c++_util\environ.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\environ.obj" : $(SOURCE) $(DEP_CPP_ENVIR) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

DEP_CPP_ENVIR=\
	"..\src\condor_c++_util\environ.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\environ.obj" : $(SOURCE) $(DEP_CPP_ENVIR) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\file_lock.C"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"

DEP_CPP_FILE_=\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\file_lock.obj" : $(SOURCE) $(DEP_CPP_FILE_) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

DEP_CPP_FILE_=\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\file_lock.obj" : $(SOURCE) $(DEP_CPP_FILE_) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\file_transfer.C"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"

DEP_CPP_FILE_T=\
	"..\src\condor_c++_util\directory.h"\
	"..\src\condor_c++_util\extArray.h"\
	"..\src\condor_c++_util\file_transfer.h"\
	"..\src\condor_c++_util\HashTable.h"\
	"..\src\condor_c++_util\list.h"\
	"..\src\condor_c++_util\MyString.h"\
	"..\src\condor_c++_util\ntsysinfo.h"\
	"..\src\condor_c++_util\perm.h"\
	"..\src\condor_c++_util\string_list.h"\
	"..\src\condor_daemon_core.V6\condor_daemon_core.h"\
	"..\src\condor_daemon_core.V6\condor_ipverify.h"\
	"..\src\condor_daemon_core.V6\condor_timer_manager.h"\
	"..\src\condor_includes\basename.h"\
	"..\src\condor_includes\buffers.h"\
	"..\src\condor_includes\condor_adtypes.h"\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_attrlist.h"\
	"..\src\condor_includes\condor_ckpt_name.h"\
	"..\src\condor_includes\condor_classad.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_expressions.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_io.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\condor_includes\reli_sock.h"\
	"..\src\condor_includes\safe_sock.h"\
	"..\src\condor_includes\sock.h"\
	"..\src\condor_includes\sockCache.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	

"$(INTDIR)\file_transfer.obj" : $(SOURCE) $(DEP_CPP_FILE_T) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

DEP_CPP_FILE_T=\
	"..\src\condor_c++_util\directory.h"\
	"..\src\condor_c++_util\extArray.h"\
	"..\src\condor_c++_util\file_transfer.h"\
	"..\src\condor_c++_util\HashTable.h"\
	"..\src\condor_c++_util\list.h"\
	"..\src\condor_c++_util\MyString.h"\
	"..\src\condor_c++_util\ntsysinfo.h"\
	"..\src\condor_c++_util\perm.h"\
	"..\src\condor_c++_util\string_list.h"\
	"..\src\condor_daemon_core.V6\condor_daemon_core.h"\
	"..\src\condor_daemon_core.V6\condor_ipverify.h"\
	"..\src\condor_daemon_core.V6\condor_timer_manager.h"\
	"..\src\condor_includes\basename.h"\
	"..\src\condor_includes\buffers.h"\
	"..\src\condor_includes\condor_adtypes.h"\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_attrlist.h"\
	"..\src\condor_includes\condor_ckpt_name.h"\
	"..\src\condor_includes\condor_classad.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_expressions.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_io.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\condor_includes\reli_sock.h"\
	"..\src\condor_includes\safe_sock.h"\
	"..\src\condor_includes\sock.h"\
	"..\src\condor_includes\sockCache.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	

"$(INTDIR)\file_transfer.obj" : $(SOURCE) $(DEP_CPP_FILE_T) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\format_time.C"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"

DEP_CPP_FORMA=\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\format_time.obj" : $(SOURCE) $(DEP_CPP_FORMA) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

DEP_CPP_FORMA=\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\format_time.obj" : $(SOURCE) $(DEP_CPP_FORMA) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\generic_query.C"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"

DEP_CPP_GENER=\
	"..\src\condor_c++_util\generic_query.h"\
	"..\src\condor_c++_util\list.h"\
	"..\src\condor_c++_util\query_result_type.h"\
	"..\src\condor_c++_util\simplelist.h"\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_attrlist.h"\
	"..\src\condor_includes\condor_classad.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_parser.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	

"$(INTDIR)\generic_query.obj" : $(SOURCE) $(DEP_CPP_GENER) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

DEP_CPP_GENER=\
	"..\src\condor_c++_util\generic_query.h"\
	"..\src\condor_c++_util\list.h"\
	"..\src\condor_c++_util\query_result_type.h"\
	"..\src\condor_c++_util\simplelist.h"\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_attrlist.h"\
	"..\src\condor_includes\condor_classad.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_parser.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	

"$(INTDIR)\generic_query.obj" : $(SOURCE) $(DEP_CPP_GENER) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\get_daemon_addr.C"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"

DEP_CPP_GET_D=\
	"..\src\condor_c++_util\daemon.h"\
	"..\src\condor_c++_util\daemon_types.h"\
	"..\src\condor_c++_util\get_full_hostname.h"\
	"..\src\condor_c++_util\my_hostname.h"\
	"..\src\condor_includes\basename.h"\
	"..\src\condor_includes\buffers.h"\
	"..\src\condor_includes\condor_adtypes.h"\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_attrlist.h"\
	"..\src\condor_includes\condor_ckpt_name.h"\
	"..\src\condor_includes\condor_classad.h"\
	"..\src\condor_includes\condor_collector.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_expressions.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_getmnt.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_io.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_status.h"\
	"..\src\condor_includes\condor_string.h"\
	"..\src\condor_includes\reli_sock.h"\
	"..\src\condor_includes\safe_sock.h"\
	"..\src\condor_includes\sock.h"\
	"..\src\condor_includes\sockCache.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\condor_types.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	"..\src\h\util_lib_proto.h"\
	

"$(INTDIR)\get_daemon_addr.obj" : $(SOURCE) $(DEP_CPP_GET_D) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

DEP_CPP_GET_D=\
	"..\src\condor_c++_util\daemon.h"\
	"..\src\condor_c++_util\daemon_types.h"\
	"..\src\condor_c++_util\get_full_hostname.h"\
	"..\src\condor_c++_util\my_hostname.h"\
	"..\src\condor_includes\basename.h"\
	"..\src\condor_includes\buffers.h"\
	"..\src\condor_includes\condor_adtypes.h"\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_attrlist.h"\
	"..\src\condor_includes\condor_ckpt_name.h"\
	"..\src\condor_includes\condor_classad.h"\
	"..\src\condor_includes\condor_collector.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_expressions.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_getmnt.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_io.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_status.h"\
	"..\src\condor_includes\condor_string.h"\
	"..\src\condor_includes\reli_sock.h"\
	"..\src\condor_includes\safe_sock.h"\
	"..\src\condor_includes\sock.h"\
	"..\src\condor_includes\sockCache.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\condor_types.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	"..\src\h\util_lib_proto.h"\
	

"$(INTDIR)\get_daemon_addr.obj" : $(SOURCE) $(DEP_CPP_GET_D) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\get_full_hostname.C"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"

DEP_CPP_GET_F=\
	"..\src\condor_includes\basename.h"\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_attrlist.h"\
	"..\src\condor_includes\condor_ckpt_name.h"\
	"..\src\condor_includes\condor_classad.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_expressions.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_getmnt.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_status.h"\
	"..\src\condor_includes\condor_string.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\condor_types.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	"..\src\h\util_lib_proto.h"\
	

"$(INTDIR)\get_full_hostname.obj" : $(SOURCE) $(DEP_CPP_GET_F) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

DEP_CPP_GET_F=\
	"..\src\condor_includes\basename.h"\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_attrlist.h"\
	"..\src\condor_includes\condor_ckpt_name.h"\
	"..\src\condor_includes\condor_classad.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_expressions.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_getmnt.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_status.h"\
	"..\src\condor_includes\condor_string.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\condor_types.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	"..\src\h\util_lib_proto.h"\
	

"$(INTDIR)\get_full_hostname.obj" : $(SOURCE) $(DEP_CPP_GET_F) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\killfamily.C"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"

DEP_CPP_KILLF=\
	"..\src\condor_c++_util\extArray.h"\
	"..\src\condor_c++_util\HashTable.h"\
	"..\src\condor_c++_util\killfamily.h"\
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
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_io.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\condor_includes\reli_sock.h"\
	"..\src\condor_includes\safe_sock.h"\
	"..\src\condor_includes\sock.h"\
	"..\src\condor_includes\sockCache.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\condor_procapi\procapi.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	

"$(INTDIR)\killfamily.obj" : $(SOURCE) $(DEP_CPP_KILLF) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

DEP_CPP_KILLF=\
	"..\src\condor_c++_util\extArray.h"\
	"..\src\condor_c++_util\HashTable.h"\
	"..\src\condor_c++_util\killfamily.h"\
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
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_io.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\condor_includes\reli_sock.h"\
	"..\src\condor_includes\safe_sock.h"\
	"..\src\condor_includes\sock.h"\
	"..\src\condor_includes\sockCache.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\condor_procapi\procapi.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	

"$(INTDIR)\killfamily.obj" : $(SOURCE) $(DEP_CPP_KILLF) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\log.C"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"

DEP_CPP_LOG_C=\
	"..\src\condor_c++_util\log.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\log.obj" : $(SOURCE) $(DEP_CPP_LOG_C) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

DEP_CPP_LOG_C=\
	"..\src\condor_c++_util\log.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\log.obj" : $(SOURCE) $(DEP_CPP_LOG_C) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\log_transaction.C"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"

DEP_CPP_LOG_T=\
	"..\src\condor_c++_util\log.h"\
	"..\src\condor_c++_util\log_transaction.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\log_transaction.obj" : $(SOURCE) $(DEP_CPP_LOG_T) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

DEP_CPP_LOG_T=\
	"..\src\condor_c++_util\log.h"\
	"..\src\condor_c++_util\log_transaction.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\log_transaction.obj" : $(SOURCE) $(DEP_CPP_LOG_T) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\misc_utils.C"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"

DEP_CPP_MISC_=\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\misc_utils.obj" : $(SOURCE) $(DEP_CPP_MISC_) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

DEP_CPP_MISC_=\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\misc_utils.obj" : $(SOURCE) $(DEP_CPP_MISC_) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\my_hostname.C"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"

DEP_CPP_MY_HO=\
	"..\src\condor_c++_util\get_full_hostname.h"\
	"..\src\condor_c++_util\my_hostname.h"\
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
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_io.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\reli_sock.h"\
	"..\src\condor_includes\safe_sock.h"\
	"..\src\condor_includes\sock.h"\
	"..\src\condor_includes\sockCache.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\expr.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\internet.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	

"$(INTDIR)\my_hostname.obj" : $(SOURCE) $(DEP_CPP_MY_HO) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

DEP_CPP_MY_HO=\
	"..\src\condor_c++_util\get_full_hostname.h"\
	"..\src\condor_c++_util\my_hostname.h"\
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
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_io.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\reli_sock.h"\
	"..\src\condor_includes\safe_sock.h"\
	"..\src\condor_includes\sock.h"\
	"..\src\condor_includes\sockCache.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\expr.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\internet.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	

"$(INTDIR)\my_hostname.obj" : $(SOURCE) $(DEP_CPP_MY_HO) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\my_subsystem.C"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /O2 /I "..\src\h" /I "..\src\condor_includes"\
 /I "..\src\condor_c++_util" /D "WIN32" /D "NDEBUG" /D "_WINDOWS"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 

"$(INTDIR)\my_subsystem.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /GX /Z7 /Od /I "..\src\h" /I\
 "..\src\condor_includes" /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 

"$(INTDIR)\my_subsystem.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE="..\src\condor_c++_util\my_username.C"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"

DEP_CPP_MY_US=\
	"..\src\condor_c++_util\my_username.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\my_username.obj" : $(SOURCE) $(DEP_CPP_MY_US) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

DEP_CPP_MY_US=\
	"..\src\condor_c++_util\my_username.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\my_username.obj" : $(SOURCE) $(DEP_CPP_MY_US) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\ntsysinfo.C"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"

DEP_CPP_NTSYS=\
	"..\src\condor_c++_util\extArray.h"\
	"..\src\condor_c++_util\ntsysinfo.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\ntsysinfo.obj" : $(SOURCE) $(DEP_CPP_NTSYS) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

DEP_CPP_NTSYS=\
	"..\src\condor_c++_util\extArray.h"\
	"..\src\condor_c++_util\ntsysinfo.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\ntsysinfo.obj" : $(SOURCE) $(DEP_CPP_NTSYS) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\perm.C"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"

DEP_CPP_PERM_=\
	"..\src\condor_c++_util\perm.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\perm.obj" : $(SOURCE) $(DEP_CPP_PERM_) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

DEP_CPP_PERM_=\
	"..\src\condor_c++_util\perm.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\perm.obj" : $(SOURCE) $(DEP_CPP_PERM_) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\string_list.C"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"

DEP_CPP_STRIN=\
	"..\src\condor_c++_util\list.h"\
	"..\src\condor_c++_util\string_list.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\string_list.obj" : $(SOURCE) $(DEP_CPP_STRIN) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

DEP_CPP_STRIN=\
	"..\src\condor_c++_util\list.h"\
	"..\src\condor_c++_util\string_list.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\string_list.obj" : $(SOURCE) $(DEP_CPP_STRIN) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\strnewp.C"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"

DEP_CPP_STRNE=\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\strnewp.obj" : $(SOURCE) $(DEP_CPP_STRNE) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

DEP_CPP_STRNE=\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\strnewp.obj" : $(SOURCE) $(DEP_CPP_STRNE) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\uids.C"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"

DEP_CPP_UIDS_=\
	"..\src\condor_c++_util\dynuser.h"\
	"..\src\condor_c++_util\ntsecapi.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_syscall_mode.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\uids.obj" : $(SOURCE) $(DEP_CPP_UIDS_) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

DEP_CPP_UIDS_=\
	"..\src\condor_c++_util\dynuser.h"\
	"..\src\condor_c++_util\ntsecapi.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_syscall_mode.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\uids.obj" : $(SOURCE) $(DEP_CPP_UIDS_) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\up_down.C"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"

DEP_CPP_UP_DO=\
	"..\src\condor_c++_util\condor_updown.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\up_down.obj" : $(SOURCE) $(DEP_CPP_UP_DO) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

DEP_CPP_UP_DO=\
	"..\src\condor_c++_util\condor_updown.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\up_down.obj" : $(SOURCE) $(DEP_CPP_UP_DO) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\usagemon.C"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"

DEP_CPP_USAGE=\
	"..\src\condor_c++_util\usagemon.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\usagemon.obj" : $(SOURCE) $(DEP_CPP_USAGE) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

DEP_CPP_USAGE=\
	"..\src\condor_c++_util\usagemon.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\usagemon.obj" : $(SOURCE) $(DEP_CPP_USAGE) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\user_log.C"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"

DEP_CPP_USER_=\
	"..\src\condor_c++_util\condor_event.h"\
	"..\src\condor_c++_util\user_log.c++.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\user_log.obj" : $(SOURCE) $(DEP_CPP_USER_) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

DEP_CPP_USER_=\
	"..\src\condor_c++_util\condor_event.h"\
	"..\src\condor_c++_util\user_log.c++.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\user_log.obj" : $(SOURCE) $(DEP_CPP_USER_) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 


!ENDIF 

