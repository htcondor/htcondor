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
	-@erase "$(INTDIR)\ad_printmask.obj"
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
	-@erase "$(INTDIR)\daemon_types.obj"
	-@erase "$(INTDIR)\disk.obj"
	-@erase "$(INTDIR)\environ.obj"
	-@erase "$(INTDIR)\file_lock.obj"
	-@erase "$(INTDIR)\format_time.obj"
	-@erase "$(INTDIR)\generic_query.obj"
	-@erase "$(INTDIR)\get_daemon_addr.obj"
	-@erase "$(INTDIR)\get_full_hostname.obj"
	-@erase "$(INTDIR)\log.obj"
	-@erase "$(INTDIR)\log_transaction.obj"
	-@erase "$(INTDIR)\my_arch.obj"
	-@erase "$(INTDIR)\my_hostname.obj"
	-@erase "$(INTDIR)\my_subsystem.obj"
	-@erase "$(INTDIR)\string_list.obj"
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
	"$(INTDIR)\ad_printmask.obj" \
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
	"$(INTDIR)\daemon_types.obj" \
	"$(INTDIR)\disk.obj" \
	"$(INTDIR)\environ.obj" \
	"$(INTDIR)\file_lock.obj" \
	"$(INTDIR)\format_time.obj" \
	"$(INTDIR)\generic_query.obj" \
	"$(INTDIR)\get_daemon_addr.obj" \
	"$(INTDIR)\get_full_hostname.obj" \
	"$(INTDIR)\log.obj" \
	"$(INTDIR)\log_transaction.obj" \
	"$(INTDIR)\my_arch.obj" \
	"$(INTDIR)\my_hostname.obj" \
	"$(INTDIR)\my_subsystem.obj" \
	"$(INTDIR)\string_list.obj" \
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

ALL : "$(OUTDIR)\condor_cpp_util.lib" "$(OUTDIR)\condor_cpp_util.bsc"

!ELSE 

ALL : "$(OUTDIR)\condor_cpp_util.lib" "$(OUTDIR)\condor_cpp_util.bsc"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\ad_printmask.obj"
	-@erase "$(INTDIR)\ad_printmask.sbr"
	-@erase "$(INTDIR)\classad_hashtable.obj"
	-@erase "$(INTDIR)\classad_hashtable.sbr"
	-@erase "$(INTDIR)\classad_log.obj"
	-@erase "$(INTDIR)\classad_log.sbr"
	-@erase "$(INTDIR)\condor_common.obj"
	-@erase "$(INTDIR)\condor_common.pch"
	-@erase "$(INTDIR)\condor_common.sbr"
	-@erase "$(INTDIR)\condor_config.obj"
	-@erase "$(INTDIR)\condor_config.sbr"
	-@erase "$(INTDIR)\condor_event.obj"
	-@erase "$(INTDIR)\condor_event.sbr"
	-@erase "$(INTDIR)\condor_q.obj"
	-@erase "$(INTDIR)\condor_q.sbr"
	-@erase "$(INTDIR)\condor_query.obj"
	-@erase "$(INTDIR)\condor_query.sbr"
	-@erase "$(INTDIR)\condor_state.obj"
	-@erase "$(INTDIR)\condor_state.sbr"
	-@erase "$(INTDIR)\condor_version.obj"
	-@erase "$(INTDIR)\condor_version.sbr"
	-@erase "$(INTDIR)\config.obj"
	-@erase "$(INTDIR)\config.sbr"
	-@erase "$(INTDIR)\daemon_types.obj"
	-@erase "$(INTDIR)\daemon_types.sbr"
	-@erase "$(INTDIR)\disk.obj"
	-@erase "$(INTDIR)\disk.sbr"
	-@erase "$(INTDIR)\environ.obj"
	-@erase "$(INTDIR)\environ.sbr"
	-@erase "$(INTDIR)\file_lock.obj"
	-@erase "$(INTDIR)\file_lock.sbr"
	-@erase "$(INTDIR)\format_time.obj"
	-@erase "$(INTDIR)\format_time.sbr"
	-@erase "$(INTDIR)\generic_query.obj"
	-@erase "$(INTDIR)\generic_query.sbr"
	-@erase "$(INTDIR)\get_daemon_addr.obj"
	-@erase "$(INTDIR)\get_daemon_addr.sbr"
	-@erase "$(INTDIR)\get_full_hostname.obj"
	-@erase "$(INTDIR)\get_full_hostname.sbr"
	-@erase "$(INTDIR)\log.obj"
	-@erase "$(INTDIR)\log.sbr"
	-@erase "$(INTDIR)\log_transaction.obj"
	-@erase "$(INTDIR)\log_transaction.sbr"
	-@erase "$(INTDIR)\my_arch.obj"
	-@erase "$(INTDIR)\my_arch.sbr"
	-@erase "$(INTDIR)\my_hostname.obj"
	-@erase "$(INTDIR)\my_hostname.sbr"
	-@erase "$(INTDIR)\my_subsystem.obj"
	-@erase "$(INTDIR)\my_subsystem.sbr"
	-@erase "$(INTDIR)\string_list.obj"
	-@erase "$(INTDIR)\string_list.sbr"
	-@erase "$(INTDIR)\up_down.obj"
	-@erase "$(INTDIR)\up_down.sbr"
	-@erase "$(INTDIR)\usagemon.obj"
	-@erase "$(INTDIR)\usagemon.sbr"
	-@erase "$(INTDIR)\user_log.obj"
	-@erase "$(INTDIR)\user_log.sbr"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(OUTDIR)\condor_cpp_util.bsc"
	-@erase "$(OUTDIR)\condor_cpp_util.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MTd /W3 /GX /Z7 /Od /I "..\src\h" /I "..\src\condor_includes"\
 /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG" /FR"$(INTDIR)\\"\
 /Fp"$(INTDIR)\condor_common.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\"\
 /Fd"$(INTDIR)\\" /FD /TP /c 
CPP_OBJS="../src/condor_c++_util/"
CPP_SBRS="../src/condor_c++_util/"

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
	"$(INTDIR)\ad_printmask.sbr" \
	"$(INTDIR)\classad_hashtable.sbr" \
	"$(INTDIR)\classad_log.sbr" \
	"$(INTDIR)\condor_common.sbr" \
	"$(INTDIR)\condor_config.sbr" \
	"$(INTDIR)\condor_event.sbr" \
	"$(INTDIR)\condor_q.sbr" \
	"$(INTDIR)\condor_query.sbr" \
	"$(INTDIR)\condor_state.sbr" \
	"$(INTDIR)\condor_version.sbr" \
	"$(INTDIR)\config.sbr" \
	"$(INTDIR)\daemon_types.sbr" \
	"$(INTDIR)\disk.sbr" \
	"$(INTDIR)\environ.sbr" \
	"$(INTDIR)\file_lock.sbr" \
	"$(INTDIR)\format_time.sbr" \
	"$(INTDIR)\generic_query.sbr" \
	"$(INTDIR)\get_daemon_addr.sbr" \
	"$(INTDIR)\get_full_hostname.sbr" \
	"$(INTDIR)\log.sbr" \
	"$(INTDIR)\log_transaction.sbr" \
	"$(INTDIR)\my_arch.sbr" \
	"$(INTDIR)\my_hostname.sbr" \
	"$(INTDIR)\my_subsystem.sbr" \
	"$(INTDIR)\string_list.sbr" \
	"$(INTDIR)\up_down.sbr" \
	"$(INTDIR)\usagemon.sbr" \
	"$(INTDIR)\user_log.sbr"

"$(OUTDIR)\condor_cpp_util.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\condor_cpp_util.lib" 
LIB32_OBJS= \
	"$(INTDIR)\ad_printmask.obj" \
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
	"$(INTDIR)\daemon_types.obj" \
	"$(INTDIR)\disk.obj" \
	"$(INTDIR)\environ.obj" \
	"$(INTDIR)\file_lock.obj" \
	"$(INTDIR)\format_time.obj" \
	"$(INTDIR)\generic_query.obj" \
	"$(INTDIR)\get_daemon_addr.obj" \
	"$(INTDIR)\get_full_hostname.obj" \
	"$(INTDIR)\log.obj" \
	"$(INTDIR)\log_transaction.obj" \
	"$(INTDIR)\my_arch.obj" \
	"$(INTDIR)\my_hostname.obj" \
	"$(INTDIR)\my_subsystem.obj" \
	"$(INTDIR)\string_list.obj" \
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
SOURCE="..\src\condor_c++_util\ad_printmask.C"
DEP_CPP_AD_PR=\
	"..\src\condor_c++_util\ad_printmask.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\escapes.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	{$(INCLUDE)}"condor_ast.h"\
	{$(INCLUDE)}"condor_astbase.h"\
	{$(INCLUDE)}"condor_attrlist.h"\
	{$(INCLUDE)}"condor_classad.h"\
	{$(INCLUDE)}"condor_commands.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"condor_exprtype.h"\
	{$(INCLUDE)}"list.h"\
	{$(INCLUDE)}"proc.h"\
	{$(INCLUDE)}"stream.h"\
	

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"


"$(INTDIR)\ad_printmask.obj" : $(SOURCE) $(DEP_CPP_AD_PR) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"


"$(INTDIR)\ad_printmask.obj"	"$(INTDIR)\ad_printmask.sbr" : $(SOURCE)\
 $(DEP_CPP_AD_PR) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\classad_hashtable.C"
DEP_CPP_CLASS=\
	"..\src\h\file_lock.h"\
	{$(INCLUDE)}"classad_hashtable.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"HashTable.h"\
	

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"


"$(INTDIR)\classad_hashtable.obj" : $(SOURCE) $(DEP_CPP_CLASS) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"


"$(INTDIR)\classad_hashtable.obj"	"$(INTDIR)\classad_hashtable.sbr" : $(SOURCE)\
 $(DEP_CPP_CLASS) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\classad_log.C"
DEP_CPP_CLASSA=\
	"..\src\condor_c++_util\classad_log.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	{$(INCLUDE)}"classad_hashtable.h"\
	{$(INCLUDE)}"condor_ast.h"\
	{$(INCLUDE)}"condor_astbase.h"\
	{$(INCLUDE)}"condor_attrlist.h"\
	{$(INCLUDE)}"condor_classad.h"\
	{$(INCLUDE)}"condor_commands.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"condor_exprtype.h"\
	{$(INCLUDE)}"HashTable.h"\
	{$(INCLUDE)}"log.h"\
	{$(INCLUDE)}"log_transaction.h"\
	{$(INCLUDE)}"proc.h"\
	{$(INCLUDE)}"stream.h"\
	

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"


"$(INTDIR)\classad_log.obj" : $(SOURCE) $(DEP_CPP_CLASSA) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"


"$(INTDIR)\classad_log.obj"	"$(INTDIR)\classad_log.sbr" : $(SOURCE)\
 $(DEP_CPP_CLASSA) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\condor_common.C"
DEP_CPP_CONDO=\
	"..\src\h\file_lock.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"condor_file_lock.h"\
	{$(INCLUDE)}"condor_fix_assert.h"\
	{$(INCLUDE)}"condor_header_features.h"\
	{$(INCLUDE)}"condor_macros.h"\
	{$(INCLUDE)}"condor_sys_nt.h"\
	{$(INCLUDE)}"condor_system.h"\
	{$(INCLUDE)}"fake_flock.h"\
	

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"

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

CPP_SWITCHES=/nologo /MTd /W3 /Gi- /GX /Z7 /Od /I "..\src\h" /I\
 "..\src\condor_includes" /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG"\
 /FR"$(INTDIR)\\" /Fp"$(INTDIR)\condor_common.pch" /Yc"condor_common.h"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 

"$(INTDIR)\condor_common.obj"	"$(INTDIR)\condor_common.sbr"\
	"$(INTDIR)\condor_common.pch" : $(SOURCE) $(DEP_CPP_CONDO) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE="..\src\condor_c++_util\condor_config.C"
DEP_CPP_CONDOR=\
	"..\src\condor_c++_util\condor_version.h"\
	"..\src\condor_c++_util\my_arch.h"\
	"..\src\condor_c++_util\my_hostname.h"\
	"..\src\condor_c++_util\string_list.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_string.h"\
	"..\src\h\clib.h"\
	"..\src\h\condor_sys.h"\
	"..\src\h\debug.h"\
	"..\src\h\file_lock.h"\
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
	{$(INCLUDE)}"list.h"\
	{$(INCLUDE)}"proc.h"\
	{$(INCLUDE)}"stream.h"\
	

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"


"$(INTDIR)\condor_config.obj" : $(SOURCE) $(DEP_CPP_CONDOR) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"


"$(INTDIR)\condor_config.obj"	"$(INTDIR)\condor_config.sbr" : $(SOURCE)\
 $(DEP_CPP_CONDOR) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\condor_event.C"
DEP_CPP_CONDOR_=\
	"..\src\condor_c++_util\condor_event.h"\
	"..\src\condor_c++_util\user_log.c++.h"\
	"..\src\h\file_lock.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"


"$(INTDIR)\condor_event.obj" : $(SOURCE) $(DEP_CPP_CONDOR_) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"


"$(INTDIR)\condor_event.obj"	"$(INTDIR)\condor_event.sbr" : $(SOURCE)\
 $(DEP_CPP_CONDOR_) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\condor_q.C"
DEP_CPP_CONDOR_Q=\
	"..\src\condor_c++_util\condor_q.h"\
	"..\src\condor_c++_util\format_time.h"\
	"..\src\condor_c++_util\generic_query.h"\
	"..\src\condor_c++_util\query_result_type.h"\
	"..\src\condor_c++_util\simplelist.h"\
	"..\src\condor_includes\condor_adtypes.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_qmgr.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	{$(INCLUDE)}"condor_ast.h"\
	{$(INCLUDE)}"condor_astbase.h"\
	{$(INCLUDE)}"condor_attrlist.h"\
	{$(INCLUDE)}"condor_classad.h"\
	{$(INCLUDE)}"condor_commands.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"condor_exprtype.h"\
	{$(INCLUDE)}"list.h"\
	{$(INCLUDE)}"proc.h"\
	{$(INCLUDE)}"stream.h"\
	

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"


"$(INTDIR)\condor_q.obj" : $(SOURCE) $(DEP_CPP_CONDOR_Q) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"


"$(INTDIR)\condor_q.obj"	"$(INTDIR)\condor_q.sbr" : $(SOURCE)\
 $(DEP_CPP_CONDOR_Q) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\condor_query.C"
DEP_CPP_CONDOR_QU=\
	"..\src\condor_c++_util\condor_query.h"\
	"..\src\condor_c++_util\generic_query.h"\
	"..\src\condor_c++_util\query_result_type.h"\
	"..\src\condor_c++_util\simplelist.h"\
	"..\src\condor_includes\condor_adtypes.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_collector.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_parser.h"\
	"..\src\h\file_lock.h"\
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
	{$(INCLUDE)}"list.h"\
	{$(INCLUDE)}"proc.h"\
	{$(INCLUDE)}"reli_sock.h"\
	{$(INCLUDE)}"safe_sock.h"\
	{$(INCLUDE)}"sock.h"\
	{$(INCLUDE)}"sockCache.h"\
	{$(INCLUDE)}"stream.h"\
	

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"


"$(INTDIR)\condor_query.obj" : $(SOURCE) $(DEP_CPP_CONDOR_QU) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"


"$(INTDIR)\condor_query.obj"	"$(INTDIR)\condor_query.sbr" : $(SOURCE)\
 $(DEP_CPP_CONDOR_QU) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\condor_state.C"
DEP_CPP_CONDOR_S=\
	"..\src\condor_includes\condor_state.h"\
	"..\src\h\file_lock.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"


"$(INTDIR)\condor_state.obj" : $(SOURCE) $(DEP_CPP_CONDOR_S) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"


"$(INTDIR)\condor_state.obj"	"$(INTDIR)\condor_state.sbr" : $(SOURCE)\
 $(DEP_CPP_CONDOR_S) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
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
 /FR"$(INTDIR)\\" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 

"$(INTDIR)\condor_version.obj"	"$(INTDIR)\condor_version.sbr" : $(SOURCE)\
 "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE="..\src\condor_c++_util\config.C"
DEP_CPP_CONFI=\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_string.h"\
	"..\src\h\file_lock.h"\
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
	{$(INCLUDE)}"proc.h"\
	{$(INCLUDE)}"stream.h"\
	

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"


"$(INTDIR)\config.obj" : $(SOURCE) $(DEP_CPP_CONFI) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"


"$(INTDIR)\config.obj"	"$(INTDIR)\config.sbr" : $(SOURCE) $(DEP_CPP_CONFI)\
 "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\daemon_types.C"
DEP_CPP_DAEMO=\
	"..\src\condor_c++_util\daemon_types.h"\
	"..\src\h\file_lock.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"


"$(INTDIR)\daemon_types.obj" : $(SOURCE) $(DEP_CPP_DAEMO) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"


"$(INTDIR)\daemon_types.obj"	"$(INTDIR)\daemon_types.sbr" : $(SOURCE)\
 $(DEP_CPP_DAEMO) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\disk.C"
DEP_CPP_DISK_=\
	"..\src\condor_c++_util\condor_disk.h"\
	"..\src\condor_c++_util\condor_updown.h"\
	"..\src\h\debug.h"\
	"..\src\h\file_lock.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"


"$(INTDIR)\disk.obj" : $(SOURCE) $(DEP_CPP_DISK_) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"


"$(INTDIR)\disk.obj"	"$(INTDIR)\disk.sbr" : $(SOURCE) $(DEP_CPP_DISK_)\
 "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\environ.C"
DEP_CPP_ENVIR=\
	"..\src\condor_c++_util\environ.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\h\file_lock.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"


"$(INTDIR)\environ.obj" : $(SOURCE) $(DEP_CPP_ENVIR) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"


"$(INTDIR)\environ.obj"	"$(INTDIR)\environ.sbr" : $(SOURCE) $(DEP_CPP_ENVIR)\
 "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\file_lock.C"
DEP_CPP_FILE_=\
	"..\src\h\file_lock.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"


"$(INTDIR)\file_lock.obj" : $(SOURCE) $(DEP_CPP_FILE_) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"


"$(INTDIR)\file_lock.obj"	"$(INTDIR)\file_lock.sbr" : $(SOURCE)\
 $(DEP_CPP_FILE_) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\format_time.C"
DEP_CPP_FORMA=\
	"..\src\h\file_lock.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"


"$(INTDIR)\format_time.obj" : $(SOURCE) $(DEP_CPP_FORMA) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"


"$(INTDIR)\format_time.obj"	"$(INTDIR)\format_time.sbr" : $(SOURCE)\
 $(DEP_CPP_FORMA) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\generic_query.C"
DEP_CPP_GENER=\
	"..\src\condor_c++_util\generic_query.h"\
	"..\src\condor_c++_util\query_result_type.h"\
	"..\src\condor_c++_util\simplelist.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_parser.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	{$(INCLUDE)}"condor_ast.h"\
	{$(INCLUDE)}"condor_astbase.h"\
	{$(INCLUDE)}"condor_attrlist.h"\
	{$(INCLUDE)}"condor_classad.h"\
	{$(INCLUDE)}"condor_commands.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"condor_exprtype.h"\
	{$(INCLUDE)}"list.h"\
	{$(INCLUDE)}"proc.h"\
	{$(INCLUDE)}"stream.h"\
	

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"


"$(INTDIR)\generic_query.obj" : $(SOURCE) $(DEP_CPP_GENER) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"


"$(INTDIR)\generic_query.obj"	"$(INTDIR)\generic_query.sbr" : $(SOURCE)\
 $(DEP_CPP_GENER) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\get_daemon_addr.C"
DEP_CPP_GET_D=\
	"..\src\condor_c++_util\condor_query.h"\
	"..\src\condor_c++_util\daemon_types.h"\
	"..\src\condor_c++_util\generic_query.h"\
	"..\src\condor_c++_util\get_full_hostname.h"\
	"..\src\condor_c++_util\my_hostname.h"\
	"..\src\condor_c++_util\query_result_type.h"\
	"..\src\condor_c++_util\simplelist.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_collector.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\h\file_lock.h"\
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
	{$(INCLUDE)}"list.h"\
	{$(INCLUDE)}"proc.h"\
	{$(INCLUDE)}"stream.h"\
	

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"


"$(INTDIR)\get_daemon_addr.obj" : $(SOURCE) $(DEP_CPP_GET_D) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"


"$(INTDIR)\get_daemon_addr.obj"	"$(INTDIR)\get_daemon_addr.sbr" : $(SOURCE)\
 $(DEP_CPP_GET_D) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\get_full_hostname.C"
DEP_CPP_GET_F=\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\h\file_lock.h"\
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
	{$(INCLUDE)}"proc.h"\
	{$(INCLUDE)}"stream.h"\
	

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"


"$(INTDIR)\get_full_hostname.obj" : $(SOURCE) $(DEP_CPP_GET_F) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"


"$(INTDIR)\get_full_hostname.obj"	"$(INTDIR)\get_full_hostname.sbr" : $(SOURCE)\
 $(DEP_CPP_GET_F) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\log.C"
DEP_CPP_LOG_C=\
	"..\src\h\file_lock.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"log.h"\
	

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"


"$(INTDIR)\log.obj" : $(SOURCE) $(DEP_CPP_LOG_C) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"


"$(INTDIR)\log.obj"	"$(INTDIR)\log.sbr" : $(SOURCE) $(DEP_CPP_LOG_C)\
 "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\log_transaction.C"
DEP_CPP_LOG_T=\
	"..\src\h\file_lock.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"log.h"\
	{$(INCLUDE)}"log_transaction.h"\
	

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"


"$(INTDIR)\log_transaction.obj" : $(SOURCE) $(DEP_CPP_LOG_T) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"


"$(INTDIR)\log_transaction.obj"	"$(INTDIR)\log_transaction.sbr" : $(SOURCE)\
 $(DEP_CPP_LOG_T) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\my_arch.C"
DEP_CPP_MY_AR=\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\match_prefix.h"\
	"..\src\h\file_lock.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"


"$(INTDIR)\my_arch.obj" : $(SOURCE) $(DEP_CPP_MY_AR) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"


"$(INTDIR)\my_arch.obj"	"$(INTDIR)\my_arch.sbr" : $(SOURCE) $(DEP_CPP_MY_AR)\
 "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\my_hostname.C"
DEP_CPP_MY_HO=\
	"..\src\condor_c++_util\get_full_hostname.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\h\file_lock.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"


"$(INTDIR)\my_hostname.obj" : $(SOURCE) $(DEP_CPP_MY_HO) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"


"$(INTDIR)\my_hostname.obj"	"$(INTDIR)\my_hostname.sbr" : $(SOURCE)\
 $(DEP_CPP_MY_HO) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
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
 /FR"$(INTDIR)\\" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 

"$(INTDIR)\my_subsystem.obj"	"$(INTDIR)\my_subsystem.sbr" : $(SOURCE)\
 "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE="..\src\condor_c++_util\string_list.C"
DEP_CPP_STRIN=\
	"..\src\condor_c++_util\string_list.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\h\file_lock.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"list.h"\
	

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"


"$(INTDIR)\string_list.obj" : $(SOURCE) $(DEP_CPP_STRIN) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"


"$(INTDIR)\string_list.obj"	"$(INTDIR)\string_list.sbr" : $(SOURCE)\
 $(DEP_CPP_STRIN) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\up_down.C"
DEP_CPP_UP_DO=\
	"..\src\condor_c++_util\condor_updown.h"\
	"..\src\h\file_lock.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"


"$(INTDIR)\up_down.obj" : $(SOURCE) $(DEP_CPP_UP_DO) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"


"$(INTDIR)\up_down.obj"	"$(INTDIR)\up_down.sbr" : $(SOURCE) $(DEP_CPP_UP_DO)\
 "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\usagemon.C"
DEP_CPP_USAGE=\
	"..\src\condor_c++_util\usagemon.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\h\file_lock.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"


"$(INTDIR)\usagemon.obj" : $(SOURCE) $(DEP_CPP_USAGE) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"


"$(INTDIR)\usagemon.obj"	"$(INTDIR)\usagemon.sbr" : $(SOURCE) $(DEP_CPP_USAGE)\
 "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\condor_c++_util\user_log.C"
DEP_CPP_USER_=\
	"..\src\condor_c++_util\condor_event.h"\
	"..\src\condor_c++_util\user_log.c++.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\h\file_lock.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"


"$(INTDIR)\user_log.obj" : $(SOURCE) $(DEP_CPP_USER_) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"


"$(INTDIR)\user_log.obj"	"$(INTDIR)\user_log.sbr" : $(SOURCE) $(DEP_CPP_USER_)\
 "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 


!ENDIF 

