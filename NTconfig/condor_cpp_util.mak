# Microsoft Developer Studio Generated NMAKE File, Based on condor_cpp_util.dsp
!IF "$(CFG)" == ""
CFG=condor_cpp_util - Win32 Release
!MESSAGE No configuration specified. Defaulting to condor_cpp_util - Win32 Release.
!ENDIF 

!IF "$(CFG)" != "condor_cpp_util - Win32 Debug" && "$(CFG)" != "condor_cpp_util - Win32 Release"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_cpp_util.mak" CFG="condor_cpp_util - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_cpp_util - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "condor_cpp_util - Win32 Release" (based on "Win32 (x86) Static Library")
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

!IF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

OUTDIR=.\..\Debug
INTDIR=.\..\Debug
# Begin Custom Macros
OutDir=.\..\Debug
# End Custom Macros

ALL : "$(OUTDIR)\condor_cpp_util.lib"


CLEAN :
	-@erase "$(INTDIR)\access.obj"
	-@erase "$(INTDIR)\ad_printmask.obj"
	-@erase "$(INTDIR)\classad_collection.obj"
	-@erase "$(INTDIR)\classad_hashtable.obj"
	-@erase "$(INTDIR)\classad_log.obj"
	-@erase "$(INTDIR)\condor_config.obj"
	-@erase "$(INTDIR)\condor_event.obj"
	-@erase "$(INTDIR)\condor_q.obj"
	-@erase "$(INTDIR)\condor_query.obj"
	-@erase "$(INTDIR)\condor_state.obj"
	-@erase "$(INTDIR)\condor_ver_info.obj"
	-@erase "$(INTDIR)\condor_version.obj"
	-@erase "$(INTDIR)\config.obj"
	-@erase "$(INTDIR)\daemon.obj"
	-@erase "$(INTDIR)\daemon_types.obj"
	-@erase "$(INTDIR)\directory.obj"
	-@erase "$(INTDIR)\dynuser.obj"
	-@erase "$(INTDIR)\email_cpp.obj"
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
	-@erase "$(INTDIR)\metric_units.obj"
	-@erase "$(INTDIR)\misc_utils.obj"
	-@erase "$(INTDIR)\my_hostname.obj"
	-@erase "$(INTDIR)\my_subsystem.obj"
	-@erase "$(INTDIR)\my_username.obj"
	-@erase "$(INTDIR)\ntsysinfo.obj"
	-@erase "$(INTDIR)\perm.obj"
	-@erase "$(INTDIR)\string_list.obj"
	-@erase "$(INTDIR)\strnewp.obj"
	-@erase "$(INTDIR)\uids.obj"
	-@erase "$(INTDIR)\usagemon.obj"
	-@erase "$(INTDIR)\user_log.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\condor_cpp_util.lib"
	-@erase "..\Debug\condor_common.obj"
	-@erase "..\Debug\condor_common.pch"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MDd /W3 /Gm /Gi /GX /ZI /Od /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG" /Fp"$(INTDIR)\condor_common.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 
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
	"..\Debug\condor_common.obj" \
	"$(INTDIR)\condor_config.obj" \
	"$(INTDIR)\condor_event.obj" \
	"$(INTDIR)\condor_q.obj" \
	"$(INTDIR)\condor_query.obj" \
	"$(INTDIR)\condor_state.obj" \
	"$(INTDIR)\condor_ver_info.obj" \
	"$(INTDIR)\condor_version.obj" \
	"$(INTDIR)\config.obj" \
	"$(INTDIR)\daemon.obj" \
	"$(INTDIR)\daemon_types.obj" \
	"$(INTDIR)\directory.obj" \
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
	"$(INTDIR)\metric_units.obj" \
	"$(INTDIR)\misc_utils.obj" \
	"$(INTDIR)\my_hostname.obj" \
	"$(INTDIR)\my_subsystem.obj" \
	"$(INTDIR)\my_username.obj" \
	"$(INTDIR)\ntsysinfo.obj" \
	"$(INTDIR)\perm.obj" \
	"$(INTDIR)\string_list.obj" \
	"$(INTDIR)\strnewp.obj" \
	"$(INTDIR)\uids.obj" \
	"$(INTDIR)\usagemon.obj" \
	"$(INTDIR)\user_log.obj" \
	"$(INTDIR)\email_cpp.obj"

"$(OUTDIR)\condor_cpp_util.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Release"

OUTDIR=.\../Release
INTDIR=.\../Release
# Begin Custom Macros
OutDir=.\../Release
# End Custom Macros

ALL : "$(OUTDIR)\condor_cpp_util.lib"


CLEAN :
	-@erase "$(INTDIR)\access.obj"
	-@erase "$(INTDIR)\ad_printmask.obj"
	-@erase "$(INTDIR)\classad_collection.obj"
	-@erase "$(INTDIR)\classad_hashtable.obj"
	-@erase "$(INTDIR)\classad_log.obj"
	-@erase "$(INTDIR)\condor_config.obj"
	-@erase "$(INTDIR)\condor_event.obj"
	-@erase "$(INTDIR)\condor_q.obj"
	-@erase "$(INTDIR)\condor_query.obj"
	-@erase "$(INTDIR)\condor_state.obj"
	-@erase "$(INTDIR)\condor_ver_info.obj"
	-@erase "$(INTDIR)\condor_version.obj"
	-@erase "$(INTDIR)\config.obj"
	-@erase "$(INTDIR)\daemon.obj"
	-@erase "$(INTDIR)\daemon_types.obj"
	-@erase "$(INTDIR)\directory.obj"
	-@erase "$(INTDIR)\dynuser.obj"
	-@erase "$(INTDIR)\email_cpp.obj"
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
	-@erase "$(INTDIR)\metric_units.obj"
	-@erase "$(INTDIR)\misc_utils.obj"
	-@erase "$(INTDIR)\my_hostname.obj"
	-@erase "$(INTDIR)\my_subsystem.obj"
	-@erase "$(INTDIR)\my_username.obj"
	-@erase "$(INTDIR)\ntsysinfo.obj"
	-@erase "$(INTDIR)\perm.obj"
	-@erase "$(INTDIR)\string_list.obj"
	-@erase "$(INTDIR)\strnewp.obj"
	-@erase "$(INTDIR)\uids.obj"
	-@erase "$(INTDIR)\usagemon.obj"
	-@erase "$(INTDIR)\user_log.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\condor_cpp_util.lib"
	-@erase "..\Release\condor_common.obj"
	-@erase "..\Release\condor_common.pch"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MD /W3 /GX /Z7 /O1 /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /D "WIN32" /D "NDEBUG" /Fp"$(INTDIR)\condor_common.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 
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
	"..\Release\condor_common.obj" \
	"$(INTDIR)\condor_config.obj" \
	"$(INTDIR)\condor_event.obj" \
	"$(INTDIR)\condor_q.obj" \
	"$(INTDIR)\condor_query.obj" \
	"$(INTDIR)\condor_state.obj" \
	"$(INTDIR)\condor_ver_info.obj" \
	"$(INTDIR)\condor_version.obj" \
	"$(INTDIR)\config.obj" \
	"$(INTDIR)\daemon.obj" \
	"$(INTDIR)\daemon_types.obj" \
	"$(INTDIR)\directory.obj" \
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
	"$(INTDIR)\metric_units.obj" \
	"$(INTDIR)\misc_utils.obj" \
	"$(INTDIR)\my_hostname.obj" \
	"$(INTDIR)\my_subsystem.obj" \
	"$(INTDIR)\my_username.obj" \
	"$(INTDIR)\ntsysinfo.obj" \
	"$(INTDIR)\perm.obj" \
	"$(INTDIR)\string_list.obj" \
	"$(INTDIR)\strnewp.obj" \
	"$(INTDIR)\uids.obj" \
	"$(INTDIR)\usagemon.obj" \
	"$(INTDIR)\user_log.obj" \
	"$(INTDIR)\email_cpp.obj"

"$(OUTDIR)\condor_cpp_util.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
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
!IF EXISTS("condor_cpp_util.dep")
!INCLUDE "condor_cpp_util.dep"
!ELSE 
!MESSAGE Warning: cannot find "condor_cpp_util.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "condor_cpp_util - Win32 Debug" || "$(CFG)" == "condor_cpp_util - Win32 Release"
SOURCE="..\src\condor_c++_util\access.C"

"$(INTDIR)\access.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\ad_printmask.C"

"$(INTDIR)\ad_printmask.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\classad_collection.C"

"$(INTDIR)\classad_collection.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\classad_hashtable.C"

"$(INTDIR)\classad_hashtable.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\classad_log.C"

"$(INTDIR)\classad_log.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\condor_common.C"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

CPP_SWITCHES=/nologo /MDd /W3 /Gm /Gi- /GX /ZI /Od /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG" /Fp"..\Debug\condor_common.pch" /Yc"condor_common.h" /Fo"..\Debug/" /Fd"..\Debug/" /FD /TP /c 

"..\Debug\condor_common.obj"	"..\Debug\condor_common.pch" : $(SOURCE)
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Release"

CPP_SWITCHES=/nologo /MD /W3 /Gi- /GX /Z7 /O1 /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /D "WIN32" /D "NDEBUG" /Fp"..\Release\condor_common.pch" /Yc"condor_common.h" /Fo"../Release/" /Fd"../Release/" /FD /TP /c 

"..\Release\condor_common.obj"	"..\Release\condor_common.pch" : $(SOURCE)
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE="..\src\condor_c++_util\condor_config.C"

"$(INTDIR)\condor_config.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\condor_event.C"

"$(INTDIR)\condor_event.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\condor_q.C"

"$(INTDIR)\condor_q.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\condor_query.C"

"$(INTDIR)\condor_query.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\condor_state.C"

"$(INTDIR)\condor_state.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\condor_ver_info.C"

"$(INTDIR)\condor_ver_info.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\condor_version.C"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

CPP_SWITCHES=/nologo /MDd /W3 /Gm /Gi /GX /ZI /Od /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 

"$(INTDIR)\condor_version.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Release"

CPP_SWITCHES=/nologo /MD /W3 /GX /Z7 /O1 /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /D "WIN32" /D "NDEBUG" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 

"$(INTDIR)\condor_version.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE="..\src\condor_c++_util\config.C"

"$(INTDIR)\config.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\daemon.C"

"$(INTDIR)\daemon.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\daemon_types.C"

"$(INTDIR)\daemon_types.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\directory.C"

"$(INTDIR)\directory.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\dynuser.C"

"$(INTDIR)\dynuser.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\email_cpp.C"

"$(INTDIR)\email_cpp.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\environ.C"

"$(INTDIR)\environ.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\file_lock.C"

"$(INTDIR)\file_lock.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\file_transfer.C"

"$(INTDIR)\file_transfer.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\format_time.C"

"$(INTDIR)\format_time.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\generic_query.C"

"$(INTDIR)\generic_query.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\get_daemon_addr.C"

"$(INTDIR)\get_daemon_addr.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\get_full_hostname.C"

"$(INTDIR)\get_full_hostname.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\killfamily.C"

"$(INTDIR)\killfamily.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\log.C"

"$(INTDIR)\log.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\log_transaction.C"

"$(INTDIR)\log_transaction.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\metric_units.C"

"$(INTDIR)\metric_units.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\misc_utils.C"

"$(INTDIR)\misc_utils.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\my_hostname.C"

"$(INTDIR)\my_hostname.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\my_subsystem.C"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

CPP_SWITCHES=/nologo /MDd /W3 /Gm /Gi /GX /ZI /Od /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 

"$(INTDIR)\my_subsystem.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Release"

CPP_SWITCHES=/nologo /MD /W3 /GX /Z7 /O1 /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /D "WIN32" /D "NDEBUG" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 

"$(INTDIR)\my_subsystem.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE="..\src\condor_c++_util\my_username.C"

"$(INTDIR)\my_username.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\ntsysinfo.C"

"$(INTDIR)\ntsysinfo.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\perm.C"

"$(INTDIR)\perm.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\string_list.C"

"$(INTDIR)\string_list.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\strnewp.C"

"$(INTDIR)\strnewp.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\uids.C"

"$(INTDIR)\uids.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\usagemon.C"

"$(INTDIR)\usagemon.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\user_log.C"

"$(INTDIR)\user_log.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)



!ENDIF 

