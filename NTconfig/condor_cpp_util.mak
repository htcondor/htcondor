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
	-@erase "$(INTDIR)\build_job_env.obj"
	-@erase "$(INTDIR)\check_events.obj"
	-@erase "$(INTDIR)\classad_collection.obj"
	-@erase "$(INTDIR)\classad_command_util.obj"
	-@erase "$(INTDIR)\classad_hashtable.obj"
	-@erase "$(INTDIR)\classad_helpers.obj"
	-@erase "$(INTDIR)\classad_log.obj"
	-@erase "$(INTDIR)\classad_merge.obj"
	-@erase "$(INTDIR)\classad_namedlist.obj"
	-@erase "$(INTDIR)\command_strings.obj"
	-@erase "$(INTDIR)\condor_arglist.obj"
	-@erase "$(INTDIR)\condor_attributes.obj"
	-@erase "$(INTDIR)\condor_config.obj"
	-@erase "$(INTDIR)\condor_environ.obj"
	-@erase "$(INTDIR)\condor_event.obj"
	-@erase "$(INTDIR)\condor_id.obj"
	-@erase "$(INTDIR)\condor_md.obj"
	-@erase "$(INTDIR)\condor_parameters.obj"
	-@erase "$(INTDIR)\condor_q.obj"
	-@erase "$(INTDIR)\condor_query.obj"
	-@erase "$(INTDIR)\condor_state.obj"
	-@erase "$(INTDIR)\condor_ver_info.obj"
	-@erase "$(INTDIR)\condor_version.obj"
	-@erase "$(INTDIR)\CondorError.obj"
	-@erase "$(INTDIR)\config.obj"
	-@erase "$(INTDIR)\cron.obj"
	-@erase "$(INTDIR)\cronjob.obj"
	-@erase "$(INTDIR)\cronjob_classad.obj"
	-@erase "$(INTDIR)\cronmgr.obj"
	-@erase "$(INTDIR)\daemon.obj"
	-@erase "$(INTDIR)\daemon_list.obj"
	-@erase "$(INTDIR)\daemon_types.obj"
	-@erase "$(INTDIR)\dc_collector.obj"
	-@erase "$(INTDIR)\dc_schedd.obj"
	-@erase "$(INTDIR)\dc_shadow.obj"
	-@erase "$(INTDIR)\dc_startd.obj"
	-@erase "$(INTDIR)\dc_starter.obj"
	-@erase "$(INTDIR)\directory.obj"
	-@erase "$(INTDIR)\distribution.obj"
	-@erase "$(INTDIR)\domain_tools.obj"
	-@erase "$(INTDIR)\dynuser.obj"
	-@erase "$(INTDIR)\email_cpp.obj"
	-@erase "$(INTDIR)\enum_utils.obj"
	-@erase "$(INTDIR)\env.obj"
	-@erase "$(INTDIR)\error_utils.obj"
	-@erase "$(INTDIR)\extra_param_info.obj"
	-@erase "$(INTDIR)\file_lock.obj"
	-@erase "$(INTDIR)\file_transfer.obj"
	-@erase "$(INTDIR)\format_time.obj"
	-@erase "$(INTDIR)\gahp_common.obj"
	-@erase "$(INTDIR)\generic_query.obj"
	-@erase "$(INTDIR)\get_daemon_name.obj"
	-@erase "$(INTDIR)\get_full_hostname.obj"
	-@erase "$(INTDIR)\get_mysubsystem.obj"
	-@erase "$(INTDIR)\HashTable.obj"
	-@erase "$(INTDIR)\historysnapshot.obj"
	-@erase "$(INTDIR)\iso_dates.obj"
	-@erase "$(INTDIR)\java_config.obj"
	-@erase "$(INTDIR)\job_lease.obj"
	-@erase "$(INTDIR)\jobqueuesnapshot.obj"
	-@erase "$(INTDIR)\KeyCache.obj"
	-@erase "$(INTDIR)\killfamily.obj"
	-@erase "$(INTDIR)\linebuffer.obj"
	-@erase "$(INTDIR)\log.obj"
	-@erase "$(INTDIR)\log_transaction.obj"
	-@erase "$(INTDIR)\lsa_mgr.obj"
	-@erase "$(INTDIR)\metric_units.obj"
	-@erase "$(INTDIR)\misc_utils.obj"
	-@erase "$(INTDIR)\my_distribution.obj"
	-@erase "$(INTDIR)\my_dynuser.obj"
	-@erase "$(INTDIR)\my_hostname.obj"
	-@erase "$(INTDIR)\my_subsystem.obj"
	-@erase "$(INTDIR)\my_username.obj"
	-@erase "$(INTDIR)\MyString.obj"
	-@erase "$(INTDIR)\ntsysinfo.obj"
	-@erase "$(INTDIR)\perm.obj"
	-@erase "$(INTDIR)\pgsqldatabase.obj"
	-@erase "$(INTDIR)\print_wrapped_text.obj"
	-@erase "$(INTDIR)\proc_id.obj"
	-@erase "$(INTDIR)\read_multiple_logs.obj"
	-@erase "$(INTDIR)\setenv.obj"
	-@erase "$(INTDIR)\sig_name.obj"
	-@erase "$(INTDIR)\sqlquery.obj"
	-@erase "$(INTDIR)\stat_wrapper.obj"
	-@erase "$(INTDIR)\status_string.obj"
	-@erase "$(INTDIR)\store_cred.obj"
	-@erase "$(INTDIR)\string_list.obj"
	-@erase "$(INTDIR)\stringSpace.obj"
	-@erase "$(INTDIR)\strnewp.obj"
	-@erase "$(INTDIR)\tmp_dir.obj"
	-@erase "$(INTDIR)\token_cache.obj"
	-@erase "$(INTDIR)\translation_utils.obj"
	-@erase "$(INTDIR)\uids.obj"
	-@erase "$(INTDIR)\usagemon.obj"
	-@erase "$(INTDIR)\user_job_policy.obj"
	-@erase "$(INTDIR)\user_log.obj"
	-@erase "$(INTDIR)\utc_time.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(INTDIR)\which.obj"
	-@erase "$(INTDIR)\windows_firewall.obj"
	-@erase "$(OUTDIR)\condor_cpp_util.lib"
	-@erase "..\Debug\condor_common.obj"
	-@erase "..\Debug\condor_common.pch"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /D "WIN32" /D "_DEBUG" /Fp"$(INTDIR)\condor_common.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

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

RSC=rc.exe
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_cpp_util.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\condor_cpp_util.lib" 
LIB32_OBJS= \
	"$(INTDIR)\access.obj" \
	"$(INTDIR)\ad_printmask.obj" \
	"$(INTDIR)\build_job_env.obj" \
	"$(INTDIR)\check_events.obj" \
	"$(INTDIR)\classad_collection.obj" \
	"$(INTDIR)\classad_command_util.obj" \
	"$(INTDIR)\classad_hashtable.obj" \
	"$(INTDIR)\classad_helpers.obj" \
	"$(INTDIR)\classad_log.obj" \
	"$(INTDIR)\classad_merge.obj" \
	"$(INTDIR)\classad_namedlist.obj" \
	"$(INTDIR)\command_strings.obj" \
	"$(INTDIR)\condor_arglist.obj" \
	"$(INTDIR)\condor_attributes.obj" \
	"..\Debug\condor_common.obj" \
	"$(INTDIR)\condor_config.obj" \
	"$(INTDIR)\condor_environ.obj" \
	"$(INTDIR)\condor_event.obj" \
	"$(INTDIR)\condor_id.obj" \
	"$(INTDIR)\condor_md.obj" \
	"$(INTDIR)\condor_parameters.obj" \
	"$(INTDIR)\condor_q.obj" \
	"$(INTDIR)\condor_query.obj" \
	"$(INTDIR)\condor_state.obj" \
	"$(INTDIR)\condor_ver_info.obj" \
	"$(INTDIR)\condor_version.obj" \
	"$(INTDIR)\CondorError.obj" \
	"$(INTDIR)\config.obj" \
	"$(INTDIR)\cron.obj" \
	"$(INTDIR)\cronjob.obj" \
	"$(INTDIR)\cronjob_classad.obj" \
	"$(INTDIR)\cronmgr.obj" \
	"$(INTDIR)\daemon.obj" \
	"$(INTDIR)\daemon_list.obj" \
	"$(INTDIR)\daemon_types.obj" \
	"$(INTDIR)\dc_collector.obj" \
	"$(INTDIR)\dc_schedd.obj" \
	"$(INTDIR)\dc_shadow.obj" \
	"$(INTDIR)\dc_startd.obj" \
	"$(INTDIR)\dc_starter.obj" \
	"$(INTDIR)\directory.obj" \
	"$(INTDIR)\distribution.obj" \
	"$(INTDIR)\domain_tools.obj" \
	"$(INTDIR)\dynuser.obj" \
	"$(INTDIR)\email_cpp.obj" \
	"$(INTDIR)\enum_utils.obj" \
	"$(INTDIR)\env.obj" \
	"$(INTDIR)\error_utils.obj" \
	"$(INTDIR)\extra_param_info.obj" \
	"$(INTDIR)\file_lock.obj" \
	"$(INTDIR)\file_transfer.obj" \
	"$(INTDIR)\format_time.obj" \
	"$(INTDIR)\gahp_common.obj" \
	"$(INTDIR)\generic_query.obj" \
	"$(INTDIR)\get_daemon_name.obj" \
	"$(INTDIR)\get_full_hostname.obj" \
	"$(INTDIR)\get_mysubsystem.obj" \
	"$(INTDIR)\HashTable.obj" \
	"$(INTDIR)\historysnapshot.obj" \
	"$(INTDIR)\iso_dates.obj" \
	"$(INTDIR)\java_config.obj" \
	"$(INTDIR)\job_lease.obj" \
	"$(INTDIR)\jobqueuesnapshot.obj" \
	"$(INTDIR)\KeyCache.obj" \
	"$(INTDIR)\killfamily.obj" \
	"$(INTDIR)\linebuffer.obj" \
	"$(INTDIR)\log.obj" \
	"$(INTDIR)\log_transaction.obj" \
	"$(INTDIR)\lsa_mgr.obj" \
	"$(INTDIR)\metric_units.obj" \
	"$(INTDIR)\misc_utils.obj" \
	"$(INTDIR)\my_distribution.obj" \
	"$(INTDIR)\my_dynuser.obj" \
	"$(INTDIR)\my_hostname.obj" \
	"$(INTDIR)\my_subsystem.obj" \
	"$(INTDIR)\my_username.obj" \
	"$(INTDIR)\MyString.obj" \
	"$(INTDIR)\ntsysinfo.obj" \
	"$(INTDIR)\perm.obj" \
	"$(INTDIR)\pgsqldatabase.obj" \
	"$(INTDIR)\print_wrapped_text.obj" \
	"$(INTDIR)\proc_id.obj" \
	"$(INTDIR)\read_multiple_logs.obj" \
	"$(INTDIR)\setenv.obj" \
	"$(INTDIR)\sig_name.obj" \
	"$(INTDIR)\sqlquery.obj" \
	"$(INTDIR)\stat_wrapper.obj" \
	"$(INTDIR)\status_string.obj" \
	"$(INTDIR)\store_cred.obj" \
	"$(INTDIR)\string_list.obj" \
	"$(INTDIR)\stringSpace.obj" \
	"$(INTDIR)\strnewp.obj" \
	"$(INTDIR)\tmp_dir.obj" \
	"$(INTDIR)\token_cache.obj" \
	"$(INTDIR)\translation_utils.obj" \
	"$(INTDIR)\uids.obj" \
	"$(INTDIR)\usagemon.obj" \
	"$(INTDIR)\user_job_policy.obj" \
	"$(INTDIR)\user_log.obj" \
	"$(INTDIR)\utc_time.obj" \
	"$(INTDIR)\which.obj" \
	"$(INTDIR)\windows_firewall.obj"

"$(OUTDIR)\condor_cpp_util.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Release"

OUTDIR=.\..\Release
INTDIR=.\..\Release
# Begin Custom Macros
OutDir=.\..\Release
# End Custom Macros

ALL : "$(OUTDIR)\condor_cpp_util.lib"


CLEAN :
	-@erase "$(INTDIR)\access.obj"
	-@erase "$(INTDIR)\ad_printmask.obj"
	-@erase "$(INTDIR)\build_job_env.obj"
	-@erase "$(INTDIR)\check_events.obj"
	-@erase "$(INTDIR)\classad_collection.obj"
	-@erase "$(INTDIR)\classad_command_util.obj"
	-@erase "$(INTDIR)\classad_hashtable.obj"
	-@erase "$(INTDIR)\classad_helpers.obj"
	-@erase "$(INTDIR)\classad_log.obj"
	-@erase "$(INTDIR)\classad_merge.obj"
	-@erase "$(INTDIR)\classad_namedlist.obj"
	-@erase "$(INTDIR)\command_strings.obj"
	-@erase "$(INTDIR)\condor_arglist.obj"
	-@erase "$(INTDIR)\condor_attributes.obj"
	-@erase "$(INTDIR)\condor_config.obj"
	-@erase "$(INTDIR)\condor_environ.obj"
	-@erase "$(INTDIR)\condor_event.obj"
	-@erase "$(INTDIR)\condor_id.obj"
	-@erase "$(INTDIR)\condor_md.obj"
	-@erase "$(INTDIR)\condor_parameters.obj"
	-@erase "$(INTDIR)\condor_q.obj"
	-@erase "$(INTDIR)\condor_query.obj"
	-@erase "$(INTDIR)\condor_state.obj"
	-@erase "$(INTDIR)\condor_ver_info.obj"
	-@erase "$(INTDIR)\condor_version.obj"
	-@erase "$(INTDIR)\CondorError.obj"
	-@erase "$(INTDIR)\config.obj"
	-@erase "$(INTDIR)\cron.obj"
	-@erase "$(INTDIR)\cronjob.obj"
	-@erase "$(INTDIR)\cronjob_classad.obj"
	-@erase "$(INTDIR)\cronmgr.obj"
	-@erase "$(INTDIR)\daemon.obj"
	-@erase "$(INTDIR)\daemon_list.obj"
	-@erase "$(INTDIR)\daemon_types.obj"
	-@erase "$(INTDIR)\dc_collector.obj"
	-@erase "$(INTDIR)\dc_schedd.obj"
	-@erase "$(INTDIR)\dc_shadow.obj"
	-@erase "$(INTDIR)\dc_startd.obj"
	-@erase "$(INTDIR)\dc_starter.obj"
	-@erase "$(INTDIR)\directory.obj"
	-@erase "$(INTDIR)\distribution.obj"
	-@erase "$(INTDIR)\domain_tools.obj"
	-@erase "$(INTDIR)\dynuser.obj"
	-@erase "$(INTDIR)\email_cpp.obj"
	-@erase "$(INTDIR)\enum_utils.obj"
	-@erase "$(INTDIR)\env.obj"
	-@erase "$(INTDIR)\error_utils.obj"
	-@erase "$(INTDIR)\extra_param_info.obj"
	-@erase "$(INTDIR)\file_lock.obj"
	-@erase "$(INTDIR)\file_transfer.obj"
	-@erase "$(INTDIR)\format_time.obj"
	-@erase "$(INTDIR)\gahp_common.obj"
	-@erase "$(INTDIR)\generic_query.obj"
	-@erase "$(INTDIR)\get_daemon_name.obj"
	-@erase "$(INTDIR)\get_full_hostname.obj"
	-@erase "$(INTDIR)\get_mysubsystem.obj"
	-@erase "$(INTDIR)\HashTable.obj"
	-@erase "$(INTDIR)\historysnapshot.obj"
	-@erase "$(INTDIR)\iso_dates.obj"
	-@erase "$(INTDIR)\java_config.obj"
	-@erase "$(INTDIR)\job_lease.obj"
	-@erase "$(INTDIR)\jobqueuesnapshot.obj"
	-@erase "$(INTDIR)\KeyCache.obj"
	-@erase "$(INTDIR)\killfamily.obj"
	-@erase "$(INTDIR)\linebuffer.obj"
	-@erase "$(INTDIR)\log.obj"
	-@erase "$(INTDIR)\log_transaction.obj"
	-@erase "$(INTDIR)\lsa_mgr.obj"
	-@erase "$(INTDIR)\metric_units.obj"
	-@erase "$(INTDIR)\misc_utils.obj"
	-@erase "$(INTDIR)\my_distribution.obj"
	-@erase "$(INTDIR)\my_dynuser.obj"
	-@erase "$(INTDIR)\my_hostname.obj"
	-@erase "$(INTDIR)\my_subsystem.obj"
	-@erase "$(INTDIR)\my_username.obj"
	-@erase "$(INTDIR)\MyString.obj"
	-@erase "$(INTDIR)\ntsysinfo.obj"
	-@erase "$(INTDIR)\perm.obj"
	-@erase "$(INTDIR)\pgsqldatabase.obj"
	-@erase "$(INTDIR)\print_wrapped_text.obj"
	-@erase "$(INTDIR)\proc_id.obj"
	-@erase "$(INTDIR)\read_multiple_logs.obj"
	-@erase "$(INTDIR)\setenv.obj"
	-@erase "$(INTDIR)\sig_name.obj"
	-@erase "$(INTDIR)\sqlquery.obj"
	-@erase "$(INTDIR)\stat_wrapper.obj"
	-@erase "$(INTDIR)\status_string.obj"
	-@erase "$(INTDIR)\store_cred.obj"
	-@erase "$(INTDIR)\string_list.obj"
	-@erase "$(INTDIR)\stringSpace.obj"
	-@erase "$(INTDIR)\strnewp.obj"
	-@erase "$(INTDIR)\tmp_dir.obj"
	-@erase "$(INTDIR)\token_cache.obj"
	-@erase "$(INTDIR)\translation_utils.obj"
	-@erase "$(INTDIR)\uids.obj"
	-@erase "$(INTDIR)\usagemon.obj"
	-@erase "$(INTDIR)\user_job_policy.obj"
	-@erase "$(INTDIR)\user_log.obj"
	-@erase "$(INTDIR)\utc_time.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\which.obj"
	-@erase "$(INTDIR)\windows_firewall.obj"
	-@erase "$(OUTDIR)\condor_cpp_util.lib"
	-@erase "..\Release\condor_common.obj"
	-@erase "..\Release\condor_common.pch"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MT /W3 /GX /Z7 /O1 /D "WIN32" /D "NDEBUG" /Fp"$(INTDIR)\condor_common.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

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

RSC=rc.exe
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_cpp_util.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\condor_cpp_util.lib" 
LIB32_OBJS= \
	"$(INTDIR)\access.obj" \
	"$(INTDIR)\ad_printmask.obj" \
	"$(INTDIR)\build_job_env.obj" \
	"$(INTDIR)\check_events.obj" \
	"$(INTDIR)\classad_collection.obj" \
	"$(INTDIR)\classad_command_util.obj" \
	"$(INTDIR)\classad_hashtable.obj" \
	"$(INTDIR)\classad_helpers.obj" \
	"$(INTDIR)\classad_log.obj" \
	"$(INTDIR)\classad_merge.obj" \
	"$(INTDIR)\classad_namedlist.obj" \
	"$(INTDIR)\command_strings.obj" \
	"$(INTDIR)\condor_arglist.obj" \
	"$(INTDIR)\condor_attributes.obj" \
	"..\Release\condor_common.obj" \
	"$(INTDIR)\condor_config.obj" \
	"$(INTDIR)\condor_environ.obj" \
	"$(INTDIR)\condor_event.obj" \
	"$(INTDIR)\condor_id.obj" \
	"$(INTDIR)\condor_md.obj" \
	"$(INTDIR)\condor_parameters.obj" \
	"$(INTDIR)\condor_q.obj" \
	"$(INTDIR)\condor_query.obj" \
	"$(INTDIR)\condor_state.obj" \
	"$(INTDIR)\condor_ver_info.obj" \
	"$(INTDIR)\condor_version.obj" \
	"$(INTDIR)\CondorError.obj" \
	"$(INTDIR)\config.obj" \
	"$(INTDIR)\cron.obj" \
	"$(INTDIR)\cronjob.obj" \
	"$(INTDIR)\cronjob_classad.obj" \
	"$(INTDIR)\cronmgr.obj" \
	"$(INTDIR)\daemon.obj" \
	"$(INTDIR)\daemon_list.obj" \
	"$(INTDIR)\daemon_types.obj" \
	"$(INTDIR)\dc_collector.obj" \
	"$(INTDIR)\dc_schedd.obj" \
	"$(INTDIR)\dc_shadow.obj" \
	"$(INTDIR)\dc_startd.obj" \
	"$(INTDIR)\dc_starter.obj" \
	"$(INTDIR)\directory.obj" \
	"$(INTDIR)\distribution.obj" \
	"$(INTDIR)\domain_tools.obj" \
	"$(INTDIR)\dynuser.obj" \
	"$(INTDIR)\email_cpp.obj" \
	"$(INTDIR)\enum_utils.obj" \
	"$(INTDIR)\env.obj" \
	"$(INTDIR)\error_utils.obj" \
	"$(INTDIR)\extra_param_info.obj" \
	"$(INTDIR)\file_lock.obj" \
	"$(INTDIR)\file_transfer.obj" \
	"$(INTDIR)\format_time.obj" \
	"$(INTDIR)\gahp_common.obj" \
	"$(INTDIR)\generic_query.obj" \
	"$(INTDIR)\get_daemon_name.obj" \
	"$(INTDIR)\get_full_hostname.obj" \
	"$(INTDIR)\get_mysubsystem.obj" \
	"$(INTDIR)\HashTable.obj" \
	"$(INTDIR)\historysnapshot.obj" \
	"$(INTDIR)\iso_dates.obj" \
	"$(INTDIR)\java_config.obj" \
	"$(INTDIR)\job_lease.obj" \
	"$(INTDIR)\jobqueuesnapshot.obj" \
	"$(INTDIR)\KeyCache.obj" \
	"$(INTDIR)\killfamily.obj" \
	"$(INTDIR)\linebuffer.obj" \
	"$(INTDIR)\log.obj" \
	"$(INTDIR)\log_transaction.obj" \
	"$(INTDIR)\lsa_mgr.obj" \
	"$(INTDIR)\metric_units.obj" \
	"$(INTDIR)\misc_utils.obj" \
	"$(INTDIR)\my_distribution.obj" \
	"$(INTDIR)\my_dynuser.obj" \
	"$(INTDIR)\my_hostname.obj" \
	"$(INTDIR)\my_subsystem.obj" \
	"$(INTDIR)\my_username.obj" \
	"$(INTDIR)\MyString.obj" \
	"$(INTDIR)\ntsysinfo.obj" \
	"$(INTDIR)\perm.obj" \
	"$(INTDIR)\pgsqldatabase.obj" \
	"$(INTDIR)\print_wrapped_text.obj" \
	"$(INTDIR)\proc_id.obj" \
	"$(INTDIR)\read_multiple_logs.obj" \
	"$(INTDIR)\setenv.obj" \
	"$(INTDIR)\sig_name.obj" \
	"$(INTDIR)\sqlquery.obj" \
	"$(INTDIR)\stat_wrapper.obj" \
	"$(INTDIR)\status_string.obj" \
	"$(INTDIR)\store_cred.obj" \
	"$(INTDIR)\string_list.obj" \
	"$(INTDIR)\stringSpace.obj" \
	"$(INTDIR)\strnewp.obj" \
	"$(INTDIR)\tmp_dir.obj" \
	"$(INTDIR)\token_cache.obj" \
	"$(INTDIR)\translation_utils.obj" \
	"$(INTDIR)\uids.obj" \
	"$(INTDIR)\usagemon.obj" \
	"$(INTDIR)\user_job_policy.obj" \
	"$(INTDIR)\user_log.obj" \
	"$(INTDIR)\utc_time.obj" \
	"$(INTDIR)\which.obj" \
	"$(INTDIR)\windows_firewall.obj"

"$(OUTDIR)\condor_cpp_util.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ENDIF 


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


SOURCE="..\src\condor_c++_util\build_job_env.C"

"$(INTDIR)\build_job_env.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\check_events.C"

"$(INTDIR)\check_events.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\classad_collection.C"

"$(INTDIR)\classad_collection.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\classad_command_util.C"

"$(INTDIR)\classad_command_util.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\classad_hashtable.C"

"$(INTDIR)\classad_hashtable.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\classad_helpers.C"

"$(INTDIR)\classad_helpers.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\classad_log.C"

"$(INTDIR)\classad_log.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\classad_merge.C"

"$(INTDIR)\classad_merge.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\classad_namedlist.C"

"$(INTDIR)\classad_namedlist.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\command_strings.C"

"$(INTDIR)\command_strings.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\condor_arglist.C"

"$(INTDIR)\condor_arglist.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\condor_attributes.C"

"$(INTDIR)\condor_attributes.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\condor_common.C"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /Gm /Gi- /GX /ZI /Od /D "WIN32" /D "_DEBUG" /Fp"..\Debug\condor_common.pch" /Yc"condor_common.h" /Fo"..\Debug/" /Fd"..\Debug/" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

"..\Debug\condor_common.obj"	"..\Debug\condor_common.pch" : $(SOURCE)
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /Gi- /GX /Z7 /O1 /D "WIN32" /D "NDEBUG" /Fp"..\Release\condor_common.pch" /Yc"condor_common.h" /Fo"..\Release/" /Fd"..\Release/" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

"..\Release\condor_common.obj"	"..\Release\condor_common.pch" : $(SOURCE)
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE="..\src\condor_c++_util\condor_config.C"

"$(INTDIR)\condor_config.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\condor_environ.C"

"$(INTDIR)\condor_environ.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\condor_event.C"

"$(INTDIR)\condor_event.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\condor_id.C"

"$(INTDIR)\condor_id.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\condor_md.C"

"$(INTDIR)\condor_md.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\condor_parameters.C"

"$(INTDIR)\condor_parameters.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
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

CPP_SWITCHES=/nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /D "WIN32" /D "_DEBUG" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

"$(INTDIR)\condor_version.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /Z7 /O1 /D "WIN32" /D "NDEBUG" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

"$(INTDIR)\condor_version.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE="..\src\condor_c++_util\CondorError.C"

"$(INTDIR)\CondorError.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\config.C"

"$(INTDIR)\config.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\cron.C"

"$(INTDIR)\cron.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\cronjob.C"

"$(INTDIR)\cronjob.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\cronjob_classad.C"

"$(INTDIR)\cronjob_classad.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\cronmgr.C"

"$(INTDIR)\cronmgr.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_daemon_client\daemon.C

"$(INTDIR)\daemon.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_daemon_client\daemon_list.C

"$(INTDIR)\daemon_list.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_daemon_client\daemon_types.C

"$(INTDIR)\daemon_types.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_daemon_client\dc_collector.C

"$(INTDIR)\dc_collector.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_daemon_client\dc_schedd.C

"$(INTDIR)\dc_schedd.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_daemon_client\dc_shadow.C

"$(INTDIR)\dc_shadow.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_daemon_client\dc_startd.C

"$(INTDIR)\dc_startd.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_daemon_client\dc_starter.C

"$(INTDIR)\dc_starter.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\directory.C"

"$(INTDIR)\directory.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\distribution.C"

"$(INTDIR)\distribution.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\domain_tools.C"

"$(INTDIR)\domain_tools.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\dynuser.C"

"$(INTDIR)\dynuser.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\email_cpp.C"

"$(INTDIR)\email_cpp.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\enum_utils.C"

"$(INTDIR)\enum_utils.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\env.C"

"$(INTDIR)\env.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\error_utils.C"

"$(INTDIR)\error_utils.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\extra_param_info.C"

"$(INTDIR)\extra_param_info.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
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


SOURCE="..\src\condor_c++_util\gahp_common.C"

"$(INTDIR)\gahp_common.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\generic_query.C"

"$(INTDIR)\generic_query.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\get_daemon_name.C"

"$(INTDIR)\get_daemon_name.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\get_full_hostname.C"

"$(INTDIR)\get_full_hostname.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\get_mysubsystem.C"

"$(INTDIR)\get_mysubsystem.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\HashTable.C"

"$(INTDIR)\HashTable.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\historysnapshot.C"

"$(INTDIR)\historysnapshot.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\iso_dates.C"

"$(INTDIR)\iso_dates.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\java_config.C"

"$(INTDIR)\java_config.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\job_lease.C"

"$(INTDIR)\job_lease.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\jobqueuesnapshot.C"

"$(INTDIR)\jobqueuesnapshot.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\KeyCache.C"

"$(INTDIR)\KeyCache.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\killfamily.C"

"$(INTDIR)\killfamily.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\linebuffer.C"

"$(INTDIR)\linebuffer.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\log.C"

"$(INTDIR)\log.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\log_transaction.C"

"$(INTDIR)\log_transaction.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\lsa_mgr.C"

"$(INTDIR)\lsa_mgr.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\metric_units.C"

"$(INTDIR)\metric_units.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\misc_utils.C"

"$(INTDIR)\misc_utils.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\my_distribution.C"

"$(INTDIR)\my_distribution.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\my_dynuser.C"

"$(INTDIR)\my_dynuser.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\my_hostname.C"

"$(INTDIR)\my_hostname.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\my_subsystem.C"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /D "WIN32" /D "_DEBUG" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

"$(INTDIR)\my_subsystem.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /Z7 /O1 /D "WIN32" /D "NDEBUG" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

"$(INTDIR)\my_subsystem.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE="..\src\condor_c++_util\my_username.C"

"$(INTDIR)\my_username.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\MyString.C"

"$(INTDIR)\MyString.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\ntsysinfo.C"

"$(INTDIR)\ntsysinfo.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\perm.C"

"$(INTDIR)\perm.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\pgsqldatabase.C"

"$(INTDIR)\pgsqldatabase.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\print_wrapped_text.C"

"$(INTDIR)\print_wrapped_text.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\proc_id.C"

"$(INTDIR)\proc_id.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\read_multiple_logs.C"

"$(INTDIR)\read_multiple_logs.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\setenv.C"

"$(INTDIR)\setenv.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\sig_name.C"

"$(INTDIR)\sig_name.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\sqlquery.C"

"$(INTDIR)\sqlquery.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\stat_wrapper.C"

"$(INTDIR)\stat_wrapper.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\status_string.C"

"$(INTDIR)\status_string.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\store_cred.C"

"$(INTDIR)\store_cred.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\string_list.C"

"$(INTDIR)\string_list.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\stringSpace.C"

"$(INTDIR)\stringSpace.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\strnewp.C"

"$(INTDIR)\strnewp.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\tmp_dir.C"

"$(INTDIR)\tmp_dir.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\token_cache.C"

"$(INTDIR)\token_cache.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\translation_utils.C"

"$(INTDIR)\translation_utils.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\uids.C"

"$(INTDIR)\uids.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\usagemon.C"

"$(INTDIR)\usagemon.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\user_job_policy.C"

"$(INTDIR)\user_job_policy.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\user_log.C"

"$(INTDIR)\user_log.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\utc_time.C"

"$(INTDIR)\utc_time.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\which.C"

"$(INTDIR)\which.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\windows_firewall.C"

"$(INTDIR)\windows_firewall.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)



!ENDIF 

