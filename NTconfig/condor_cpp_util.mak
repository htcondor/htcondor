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
	-@erase "$(INTDIR)\classad_visa.obj"
	-@erase "$(INTDIR)\command_strings.obj"
	-@erase "$(INTDIR)\ConcurrencyLimitUtils.obj"
	-@erase "$(INTDIR)\condor_arglist.obj"
	-@erase "$(INTDIR)\condor_attributes.obj"
	-@erase "$(INTDIR)\condor_config.obj"
	-@erase "$(INTDIR)\condor_crontab.obj"
	-@erase "$(INTDIR)\condor_environ.obj"
	-@erase "$(INTDIR)\condor_event.obj"
	-@erase "$(INTDIR)\condor_ftp.obj"
	-@erase "$(INTDIR)\condor_getcwd.obj"
	-@erase "$(INTDIR)\condor_id.obj"
	-@erase "$(INTDIR)\condor_md.obj"
	-@erase "$(INTDIR)\condor_open.obj"
	-@erase "$(INTDIR)\condor_parameters.obj"
	-@erase "$(INTDIR)\condor_q.obj"
	-@erase "$(INTDIR)\condor_query.obj"
	-@erase "$(INTDIR)\condor_state.obj"
	-@erase "$(INTDIR)\condor_url.obj"
	-@erase "$(INTDIR)\condor_user_policy.obj"
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
	-@erase "$(INTDIR)\date_util.obj"
	-@erase "$(INTDIR)\dbms_utils.obj"
	-@erase "$(INTDIR)\dc_collector.obj"
	-@erase "$(INTDIR)\dc_message.obj"
	-@erase "$(INTDIR)\dc_schedd.obj"
	-@erase "$(INTDIR)\dc_shadow.obj"
	-@erase "$(INTDIR)\dc_startd.obj"
	-@erase "$(INTDIR)\dc_starter.obj"
	-@erase "$(INTDIR)\dc_transfer_queue.obj"
	-@erase "$(INTDIR)\dc_transferd.obj"
	-@erase "$(INTDIR)\directory.obj"
	-@erase "$(INTDIR)\directory.WINDOWS.obj"
	-@erase "$(INTDIR)\distribution.obj"
	-@erase "$(INTDIR)\domain_tools.obj"
	-@erase "$(INTDIR)\dynuser.obj"
	-@erase "$(INTDIR)\email.obj"
	-@erase "$(INTDIR)\enum_utils.obj"
	-@erase "$(INTDIR)\env.obj"
	-@erase "$(INTDIR)\error_utils.obj"
	-@erase "$(INTDIR)\extra_param_info.obj"
	-@erase "$(INTDIR)\file_lock.obj"
	-@erase "$(INTDIR)\file_sql.obj"
	-@erase "$(INTDIR)\file_transfer.obj"
	-@erase "$(INTDIR)\file_transfer_db.obj"
	-@erase "$(INTDIR)\file_xml.obj"
	-@erase "$(INTDIR)\filename_tools_cpp.obj"
	-@erase "$(INTDIR)\forkwork.obj"
	-@erase "$(INTDIR)\format_time.obj"
	-@erase "$(INTDIR)\gahp_common.obj"
	-@erase "$(INTDIR)\generic_query.obj"
	-@erase "$(INTDIR)\get_daemon_name.obj"
	-@erase "$(INTDIR)\get_full_hostname.obj"
	-@erase "$(INTDIR)\HashTable.obj"
	-@erase "$(INTDIR)\hibernation_manager.obj"
	-@erase "$(INTDIR)\hibernator.obj"
	-@erase "$(INTDIR)\hibernator.WINDOWS.obj"
	-@erase "$(INTDIR)\historysnapshot.obj"
	-@erase "$(INTDIR)\hook_utils.obj"
	-@erase "$(INTDIR)\iso_dates.obj"
	-@erase "$(INTDIR)\java_config.obj"
	-@erase "$(INTDIR)\job_lease.obj"
	-@erase "$(INTDIR)\jobqueuesnapshot.obj"
	-@erase "$(INTDIR)\KeyCache.obj"
	-@erase "$(INTDIR)\killfamily.obj"
	-@erase "$(INTDIR)\linebuffer.obj"
	-@erase "$(INTDIR)\load_dll.obj"
	-@erase "$(INTDIR)\log.obj"
	-@erase "$(INTDIR)\log_transaction.obj"
	-@erase "$(INTDIR)\lsa_mgr.obj"
	-@erase "$(INTDIR)\MapFile.obj"
	-@erase "$(INTDIR)\metric_units.obj"
	-@erase "$(INTDIR)\misc_utils.obj"
	-@erase "$(INTDIR)\my_distribution.obj"
	-@erase "$(INTDIR)\my_dynuser.obj"
	-@erase "$(INTDIR)\my_hostname.obj"
	-@erase "$(INTDIR)\my_popen.obj"
	-@erase "$(INTDIR)\my_subsystem.obj"
	-@erase "$(INTDIR)\my_username.obj"
	-@erase "$(INTDIR)\MyString.obj"
	-@erase "$(INTDIR)\net_string_list.obj"
	-@erase "$(INTDIR)\network_adapter.obj"
	-@erase "$(INTDIR)\network_adapter.WINDOWS.obj"
	-@erase "$(INTDIR)\ntsysinfo.obj"
	-@erase "$(INTDIR)\perm.obj"
	-@erase "$(INTDIR)\pgsqldatabase.obj"
	-@erase "$(INTDIR)\print_wrapped_text.obj"
	-@erase "$(INTDIR)\proc_family_direct.obj"
	-@erase "$(INTDIR)\proc_family_interface.obj"
	-@erase "$(INTDIR)\proc_family_proxy.obj"
	-@erase "$(INTDIR)\proc_id.obj"
	-@erase "$(INTDIR)\procd_config.obj"
	-@erase "$(INTDIR)\process_control.WINDOWS.obj"
	-@erase "$(INTDIR)\profile.WINDOWS.obj"
	-@erase "$(INTDIR)\read_multiple_logs.obj"
	-@erase "$(INTDIR)\read_user_log.obj"
	-@erase "$(INTDIR)\read_user_log_state.obj"
	-@erase "$(INTDIR)\Regex.obj"
	-@erase "$(INTDIR)\remote_close.WINDOWS.obj"
	-@erase "$(INTDIR)\security.WINDOWS.obj"
	-@erase "$(INTDIR)\selector.obj"
	-@erase "$(INTDIR)\set_user_priv_from_ad.obj"
	-@erase "$(INTDIR)\setenv.obj"
	-@erase "$(INTDIR)\setup_api_dll.obj"
	-@erase "$(INTDIR)\sig_name.obj"
	-@erase "$(INTDIR)\sqlquery.obj"
	-@erase "$(INTDIR)\stat_info.obj"
	-@erase "$(INTDIR)\stat_wrapper.obj"
	-@erase "$(INTDIR)\stat_wrapper_internal.obj"
	-@erase "$(INTDIR)\status_string.obj"
	-@erase "$(INTDIR)\store_cred.obj"
	-@erase "$(INTDIR)\string_conversion.WINDOWS.obj"
	-@erase "$(INTDIR)\string_list.obj"
	-@erase "$(INTDIR)\stringSpace.obj"
	-@erase "$(INTDIR)\strnewp.obj"
	-@erase "$(INTDIR)\subsystem_info.obj"
	-@erase "$(INTDIR)\system_info.WINDOWS.obj"
	-@erase "$(INTDIR)\time_offset.obj"
	-@erase "$(INTDIR)\tmp_dir.obj"
	-@erase "$(INTDIR)\token_cache.obj"
	-@erase "$(INTDIR)\transfer_request.obj"
	-@erase "$(INTDIR)\translation_utils.obj"
	-@erase "$(INTDIR)\udp_waker.obj"
	-@erase "$(INTDIR)\uids.obj"
	-@erase "$(INTDIR)\usagemon.obj"
	-@erase "$(INTDIR)\user_job_policy.obj"
	-@erase "$(INTDIR)\user_log.obj"
	-@erase "$(INTDIR)\user_log_header.obj"
	-@erase "$(INTDIR)\utc_time.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(INTDIR)\vm_univ_utils.obj"
	-@erase "$(INTDIR)\waker.obj"
	-@erase "$(INTDIR)\which.obj"
	-@erase "$(INTDIR)\windows_firewall.obj"
	-@erase "$(OUTDIR)\condor_cpp_util.lib"
	-@erase "..\Debug\condor_common.obj"
	-@erase "..\Debug\condor_common.pch"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /I "..\src\condor_tt" /D "WIN32" /D "_DEBUG" /Fp"$(INTDIR)\condor_common.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_DEFINES) $(CONDOR_CPPARGS) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

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
	"$(INTDIR)\classad_visa.obj" \
	"$(INTDIR)\command_strings.obj" \
	"$(INTDIR)\ConcurrencyLimitUtils.obj" \
	"$(INTDIR)\condor_arglist.obj" \
	"$(INTDIR)\condor_attributes.obj" \
	"..\Debug\condor_common.obj" \
	"$(INTDIR)\condor_config.obj" \
	"$(INTDIR)\condor_crontab.obj" \
	"$(INTDIR)\condor_environ.obj" \
	"$(INTDIR)\condor_event.obj" \
	"$(INTDIR)\condor_ftp.obj" \
	"$(INTDIR)\condor_getcwd.obj" \
	"$(INTDIR)\condor_id.obj" \
	"$(INTDIR)\condor_md.obj" \
	"$(INTDIR)\condor_open.obj" \
	"$(INTDIR)\condor_parameters.obj" \
	"$(INTDIR)\condor_q.obj" \
	"$(INTDIR)\condor_query.obj" \
	"$(INTDIR)\condor_state.obj" \
	"$(INTDIR)\condor_url.obj" \
	"$(INTDIR)\condor_user_policy.obj" \
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
	"$(INTDIR)\date_util.obj" \
	"$(INTDIR)\dbms_utils.obj" \
	"$(INTDIR)\dc_collector.obj" \
	"$(INTDIR)\dc_message.obj" \
	"$(INTDIR)\dc_schedd.obj" \
	"$(INTDIR)\dc_shadow.obj" \
	"$(INTDIR)\dc_startd.obj" \
	"$(INTDIR)\dc_starter.obj" \
	"$(INTDIR)\dc_transfer_queue.obj" \
	"$(INTDIR)\dc_transferd.obj" \
	"$(INTDIR)\directory.obj" \
	"$(INTDIR)\directory.WINDOWS.obj" \
	"$(INTDIR)\distribution.obj" \
	"$(INTDIR)\domain_tools.obj" \
	"$(INTDIR)\dynuser.obj" \
	"$(INTDIR)\email.obj" \
	"$(INTDIR)\enum_utils.obj" \
	"$(INTDIR)\env.obj" \
	"$(INTDIR)\error_utils.obj" \
	"$(INTDIR)\extra_param_info.obj" \
	"$(INTDIR)\file_lock.obj" \
	"$(INTDIR)\file_sql.obj" \
	"$(INTDIR)\file_transfer.obj" \
	"$(INTDIR)\file_transfer_db.obj" \
	"$(INTDIR)\file_xml.obj" \
	"$(INTDIR)\filename_tools_cpp.obj" \
	"$(INTDIR)\forkwork.obj" \
	"$(INTDIR)\format_time.obj" \
	"$(INTDIR)\gahp_common.obj" \
	"$(INTDIR)\generic_query.obj" \
	"$(INTDIR)\get_daemon_name.obj" \
	"$(INTDIR)\get_full_hostname.obj" \
	"$(INTDIR)\HashTable.obj" \
	"$(INTDIR)\hibernation_manager.obj" \
	"$(INTDIR)\hibernator.obj" \
	"$(INTDIR)\hibernator.WINDOWS.obj" \
	"$(INTDIR)\historysnapshot.obj" \
	"$(INTDIR)\hook_utils.obj" \
	"$(INTDIR)\iso_dates.obj" \
	"$(INTDIR)\java_config.obj" \
	"$(INTDIR)\job_lease.obj" \
	"$(INTDIR)\jobqueuesnapshot.obj" \
	"$(INTDIR)\KeyCache.obj" \
	"$(INTDIR)\killfamily.obj" \
	"$(INTDIR)\linebuffer.obj" \
	"$(INTDIR)\load_dll.obj" \
	"$(INTDIR)\log.obj" \
	"$(INTDIR)\log_transaction.obj" \
	"$(INTDIR)\lsa_mgr.obj" \
	"$(INTDIR)\MapFile.obj" \
	"$(INTDIR)\metric_units.obj" \
	"$(INTDIR)\misc_utils.obj" \
	"$(INTDIR)\my_distribution.obj" \
	"$(INTDIR)\my_dynuser.obj" \
	"$(INTDIR)\my_hostname.obj" \
	"$(INTDIR)\my_popen.obj" \
	"$(INTDIR)\my_subsystem.obj" \
	"$(INTDIR)\my_username.obj" \
	"$(INTDIR)\MyString.obj" \
	"$(INTDIR)\net_string_list.obj" \
	"$(INTDIR)\network_adapter.obj" \
	"$(INTDIR)\network_adapter.WINDOWS.obj" \
	"$(INTDIR)\ntsysinfo.obj" \
	"$(INTDIR)\perm.obj" \
	"$(INTDIR)\pgsqldatabase.obj" \
	"$(INTDIR)\print_wrapped_text.obj" \
	"$(INTDIR)\proc_family_direct.obj" \
	"$(INTDIR)\proc_family_interface.obj" \
	"$(INTDIR)\proc_family_proxy.obj" \
	"$(INTDIR)\proc_id.obj" \
	"$(INTDIR)\procd_config.obj" \
	"$(INTDIR)\process_control.WINDOWS.obj" \
	"$(INTDIR)\profile.WINDOWS.obj" \
	"$(INTDIR)\read_multiple_logs.obj" \
	"$(INTDIR)\read_user_log.obj" \
	"$(INTDIR)\read_user_log_state.obj" \
	"$(INTDIR)\Regex.obj" \
	"$(INTDIR)\remote_close.WINDOWS.obj" \
	"$(INTDIR)\security.WINDOWS.obj" \
	"$(INTDIR)\selector.obj" \
	"$(INTDIR)\set_user_priv_from_ad.obj" \
	"$(INTDIR)\setenv.obj" \
	"$(INTDIR)\setup_api_dll.obj" \
	"$(INTDIR)\sig_name.obj" \
	"$(INTDIR)\sqlquery.obj" \
	"$(INTDIR)\stat_info.obj" \
	"$(INTDIR)\stat_wrapper.obj" \
	"$(INTDIR)\stat_wrapper_internal.obj" \
	"$(INTDIR)\status_string.obj" \
	"$(INTDIR)\store_cred.obj" \
	"$(INTDIR)\string_conversion.WINDOWS.obj" \
	"$(INTDIR)\string_list.obj" \
	"$(INTDIR)\stringSpace.obj" \
	"$(INTDIR)\strnewp.obj" \
	"$(INTDIR)\subsystem_info.obj" \
	"$(INTDIR)\system_info.WINDOWS.obj" \
	"$(INTDIR)\time_offset.obj" \
	"$(INTDIR)\tmp_dir.obj" \
	"$(INTDIR)\token_cache.obj" \
	"$(INTDIR)\transfer_request.obj" \
	"$(INTDIR)\translation_utils.obj" \
	"$(INTDIR)\udp_waker.obj" \
	"$(INTDIR)\uids.obj" \
	"$(INTDIR)\usagemon.obj" \
	"$(INTDIR)\user_job_policy.obj" \
	"$(INTDIR)\user_log.obj" \
	"$(INTDIR)\user_log_header.obj" \
	"$(INTDIR)\utc_time.obj" \
	"$(INTDIR)\vm_univ_utils.obj" \
	"$(INTDIR)\waker.obj" \
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
	-@erase "$(INTDIR)\classad_visa.obj"
	-@erase "$(INTDIR)\command_strings.obj"
	-@erase "$(INTDIR)\ConcurrencyLimitUtils.obj"
	-@erase "$(INTDIR)\condor_arglist.obj"
	-@erase "$(INTDIR)\condor_attributes.obj"
	-@erase "$(INTDIR)\condor_config.obj"
	-@erase "$(INTDIR)\condor_crontab.obj"
	-@erase "$(INTDIR)\condor_environ.obj"
	-@erase "$(INTDIR)\condor_event.obj"
	-@erase "$(INTDIR)\condor_ftp.obj"
	-@erase "$(INTDIR)\condor_getcwd.obj"
	-@erase "$(INTDIR)\condor_id.obj"
	-@erase "$(INTDIR)\condor_md.obj"
	-@erase "$(INTDIR)\condor_open.obj"
	-@erase "$(INTDIR)\condor_parameters.obj"
	-@erase "$(INTDIR)\condor_q.obj"
	-@erase "$(INTDIR)\condor_query.obj"
	-@erase "$(INTDIR)\condor_state.obj"
	-@erase "$(INTDIR)\condor_url.obj"
	-@erase "$(INTDIR)\condor_user_policy.obj"
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
	-@erase "$(INTDIR)\date_util.obj"
	-@erase "$(INTDIR)\dbms_utils.obj"
	-@erase "$(INTDIR)\dc_collector.obj"
	-@erase "$(INTDIR)\dc_message.obj"
	-@erase "$(INTDIR)\dc_schedd.obj"
	-@erase "$(INTDIR)\dc_shadow.obj"
	-@erase "$(INTDIR)\dc_startd.obj"
	-@erase "$(INTDIR)\dc_starter.obj"
	-@erase "$(INTDIR)\dc_transfer_queue.obj"
	-@erase "$(INTDIR)\dc_transferd.obj"
	-@erase "$(INTDIR)\directory.obj"
	-@erase "$(INTDIR)\directory.WINDOWS.obj"
	-@erase "$(INTDIR)\distribution.obj"
	-@erase "$(INTDIR)\domain_tools.obj"
	-@erase "$(INTDIR)\dynuser.obj"
	-@erase "$(INTDIR)\email.obj"
	-@erase "$(INTDIR)\enum_utils.obj"
	-@erase "$(INTDIR)\env.obj"
	-@erase "$(INTDIR)\error_utils.obj"
	-@erase "$(INTDIR)\extra_param_info.obj"
	-@erase "$(INTDIR)\file_lock.obj"
	-@erase "$(INTDIR)\file_sql.obj"
	-@erase "$(INTDIR)\file_transfer.obj"
	-@erase "$(INTDIR)\file_transfer_db.obj"
	-@erase "$(INTDIR)\file_xml.obj"
	-@erase "$(INTDIR)\filename_tools_cpp.obj"
	-@erase "$(INTDIR)\forkwork.obj"
	-@erase "$(INTDIR)\format_time.obj"
	-@erase "$(INTDIR)\gahp_common.obj"
	-@erase "$(INTDIR)\generic_query.obj"
	-@erase "$(INTDIR)\get_daemon_name.obj"
	-@erase "$(INTDIR)\get_full_hostname.obj"
	-@erase "$(INTDIR)\HashTable.obj"
	-@erase "$(INTDIR)\hibernation_manager.obj"
	-@erase "$(INTDIR)\hibernator.obj"
	-@erase "$(INTDIR)\hibernator.WINDOWS.obj"
	-@erase "$(INTDIR)\historysnapshot.obj"
	-@erase "$(INTDIR)\hook_utils.obj"
	-@erase "$(INTDIR)\iso_dates.obj"
	-@erase "$(INTDIR)\java_config.obj"
	-@erase "$(INTDIR)\job_lease.obj"
	-@erase "$(INTDIR)\jobqueuesnapshot.obj"
	-@erase "$(INTDIR)\KeyCache.obj"
	-@erase "$(INTDIR)\killfamily.obj"
	-@erase "$(INTDIR)\linebuffer.obj"
	-@erase "$(INTDIR)\load_dll.obj"
	-@erase "$(INTDIR)\log.obj"
	-@erase "$(INTDIR)\log_transaction.obj"
	-@erase "$(INTDIR)\lsa_mgr.obj"
	-@erase "$(INTDIR)\MapFile.obj"
	-@erase "$(INTDIR)\metric_units.obj"
	-@erase "$(INTDIR)\misc_utils.obj"
	-@erase "$(INTDIR)\my_distribution.obj"
	-@erase "$(INTDIR)\my_dynuser.obj"
	-@erase "$(INTDIR)\my_hostname.obj"
	-@erase "$(INTDIR)\my_popen.obj"
	-@erase "$(INTDIR)\my_subsystem.obj"
	-@erase "$(INTDIR)\my_username.obj"
	-@erase "$(INTDIR)\MyString.obj"
	-@erase "$(INTDIR)\net_string_list.obj"
	-@erase "$(INTDIR)\network_adapter.obj"
	-@erase "$(INTDIR)\network_adapter.WINDOWS.obj"
	-@erase "$(INTDIR)\ntsysinfo.obj"
	-@erase "$(INTDIR)\perm.obj"
	-@erase "$(INTDIR)\pgsqldatabase.obj"
	-@erase "$(INTDIR)\print_wrapped_text.obj"
	-@erase "$(INTDIR)\proc_family_direct.obj"
	-@erase "$(INTDIR)\proc_family_interface.obj"
	-@erase "$(INTDIR)\proc_family_proxy.obj"
	-@erase "$(INTDIR)\proc_id.obj"
	-@erase "$(INTDIR)\procd_config.obj"
	-@erase "$(INTDIR)\process_control.WINDOWS.obj"
	-@erase "$(INTDIR)\profile.WINDOWS.obj"
	-@erase "$(INTDIR)\read_multiple_logs.obj"
	-@erase "$(INTDIR)\read_user_log.obj"
	-@erase "$(INTDIR)\read_user_log_state.obj"
	-@erase "$(INTDIR)\Regex.obj"
	-@erase "$(INTDIR)\remote_close.WINDOWS.obj"
	-@erase "$(INTDIR)\security.WINDOWS.obj"
	-@erase "$(INTDIR)\selector.obj"
	-@erase "$(INTDIR)\set_user_priv_from_ad.obj"
	-@erase "$(INTDIR)\setenv.obj"
	-@erase "$(INTDIR)\setup_api_dll.obj"
	-@erase "$(INTDIR)\sig_name.obj"
	-@erase "$(INTDIR)\sqlquery.obj"
	-@erase "$(INTDIR)\stat_info.obj"
	-@erase "$(INTDIR)\stat_wrapper.obj"
	-@erase "$(INTDIR)\stat_wrapper_internal.obj"
	-@erase "$(INTDIR)\status_string.obj"
	-@erase "$(INTDIR)\store_cred.obj"
	-@erase "$(INTDIR)\string_conversion.WINDOWS.obj"
	-@erase "$(INTDIR)\string_list.obj"
	-@erase "$(INTDIR)\stringSpace.obj"
	-@erase "$(INTDIR)\strnewp.obj"
	-@erase "$(INTDIR)\subsystem_info.obj"
	-@erase "$(INTDIR)\system_info.WINDOWS.obj"
	-@erase "$(INTDIR)\time_offset.obj"
	-@erase "$(INTDIR)\tmp_dir.obj"
	-@erase "$(INTDIR)\token_cache.obj"
	-@erase "$(INTDIR)\transfer_request.obj"
	-@erase "$(INTDIR)\translation_utils.obj"
	-@erase "$(INTDIR)\udp_waker.obj"
	-@erase "$(INTDIR)\uids.obj"
	-@erase "$(INTDIR)\usagemon.obj"
	-@erase "$(INTDIR)\user_job_policy.obj"
	-@erase "$(INTDIR)\user_log.obj"
	-@erase "$(INTDIR)\user_log_header.obj"
	-@erase "$(INTDIR)\utc_time.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vm_univ_utils.obj"
	-@erase "$(INTDIR)\waker.obj"
	-@erase "$(INTDIR)\which.obj"
	-@erase "$(INTDIR)\windows_firewall.obj"
	-@erase "$(OUTDIR)\condor_cpp_util.lib"
	-@erase "..\Release\condor_common.obj"
	-@erase "..\Release\condor_common.pch"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MT /W3 /GX /Z7 /O1 /I "..\src\condor_tt" /D "WIN32" /D "NDEBUG" /Fp"$(INTDIR)\condor_common.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_DEFINES) $(CONDOR_CPPARGS) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

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
	"$(INTDIR)\classad_visa.obj" \
	"$(INTDIR)\command_strings.obj" \
	"$(INTDIR)\ConcurrencyLimitUtils.obj" \
	"$(INTDIR)\condor_arglist.obj" \
	"$(INTDIR)\condor_attributes.obj" \
	"..\Release\condor_common.obj" \
	"$(INTDIR)\condor_config.obj" \
	"$(INTDIR)\condor_crontab.obj" \
	"$(INTDIR)\condor_environ.obj" \
	"$(INTDIR)\condor_event.obj" \
	"$(INTDIR)\condor_ftp.obj" \
	"$(INTDIR)\condor_getcwd.obj" \
	"$(INTDIR)\condor_id.obj" \
	"$(INTDIR)\condor_md.obj" \
	"$(INTDIR)\condor_open.obj" \
	"$(INTDIR)\condor_parameters.obj" \
	"$(INTDIR)\condor_q.obj" \
	"$(INTDIR)\condor_query.obj" \
	"$(INTDIR)\condor_state.obj" \
	"$(INTDIR)\condor_url.obj" \
	"$(INTDIR)\condor_user_policy.obj" \
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
	"$(INTDIR)\date_util.obj" \
	"$(INTDIR)\dbms_utils.obj" \
	"$(INTDIR)\dc_collector.obj" \
	"$(INTDIR)\dc_message.obj" \
	"$(INTDIR)\dc_schedd.obj" \
	"$(INTDIR)\dc_shadow.obj" \
	"$(INTDIR)\dc_startd.obj" \
	"$(INTDIR)\dc_starter.obj" \
	"$(INTDIR)\dc_transfer_queue.obj" \
	"$(INTDIR)\dc_transferd.obj" \
	"$(INTDIR)\directory.obj" \
	"$(INTDIR)\directory.WINDOWS.obj" \
	"$(INTDIR)\distribution.obj" \
	"$(INTDIR)\domain_tools.obj" \
	"$(INTDIR)\dynuser.obj" \
	"$(INTDIR)\email.obj" \
	"$(INTDIR)\enum_utils.obj" \
	"$(INTDIR)\env.obj" \
	"$(INTDIR)\error_utils.obj" \
	"$(INTDIR)\extra_param_info.obj" \
	"$(INTDIR)\file_lock.obj" \
	"$(INTDIR)\file_sql.obj" \
	"$(INTDIR)\file_transfer.obj" \
	"$(INTDIR)\file_transfer_db.obj" \
	"$(INTDIR)\file_xml.obj" \
	"$(INTDIR)\filename_tools_cpp.obj" \
	"$(INTDIR)\forkwork.obj" \
	"$(INTDIR)\format_time.obj" \
	"$(INTDIR)\gahp_common.obj" \
	"$(INTDIR)\generic_query.obj" \
	"$(INTDIR)\get_daemon_name.obj" \
	"$(INTDIR)\get_full_hostname.obj" \
	"$(INTDIR)\HashTable.obj" \
	"$(INTDIR)\hibernation_manager.obj" \
	"$(INTDIR)\hibernator.obj" \
	"$(INTDIR)\hibernator.WINDOWS.obj" \
	"$(INTDIR)\historysnapshot.obj" \
	"$(INTDIR)\hook_utils.obj" \
	"$(INTDIR)\iso_dates.obj" \
	"$(INTDIR)\java_config.obj" \
	"$(INTDIR)\job_lease.obj" \
	"$(INTDIR)\jobqueuesnapshot.obj" \
	"$(INTDIR)\KeyCache.obj" \
	"$(INTDIR)\killfamily.obj" \
	"$(INTDIR)\linebuffer.obj" \
	"$(INTDIR)\load_dll.obj" \
	"$(INTDIR)\log.obj" \
	"$(INTDIR)\log_transaction.obj" \
	"$(INTDIR)\lsa_mgr.obj" \
	"$(INTDIR)\MapFile.obj" \
	"$(INTDIR)\metric_units.obj" \
	"$(INTDIR)\misc_utils.obj" \
	"$(INTDIR)\my_distribution.obj" \
	"$(INTDIR)\my_dynuser.obj" \
	"$(INTDIR)\my_hostname.obj" \
	"$(INTDIR)\my_popen.obj" \
	"$(INTDIR)\my_subsystem.obj" \
	"$(INTDIR)\my_username.obj" \
	"$(INTDIR)\MyString.obj" \
	"$(INTDIR)\net_string_list.obj" \
	"$(INTDIR)\network_adapter.obj" \
	"$(INTDIR)\network_adapter.WINDOWS.obj" \
	"$(INTDIR)\ntsysinfo.obj" \
	"$(INTDIR)\perm.obj" \
	"$(INTDIR)\pgsqldatabase.obj" \
	"$(INTDIR)\print_wrapped_text.obj" \
	"$(INTDIR)\proc_family_direct.obj" \
	"$(INTDIR)\proc_family_interface.obj" \
	"$(INTDIR)\proc_family_proxy.obj" \
	"$(INTDIR)\proc_id.obj" \
	"$(INTDIR)\procd_config.obj" \
	"$(INTDIR)\process_control.WINDOWS.obj" \
	"$(INTDIR)\profile.WINDOWS.obj" \
	"$(INTDIR)\read_multiple_logs.obj" \
	"$(INTDIR)\read_user_log.obj" \
	"$(INTDIR)\read_user_log_state.obj" \
	"$(INTDIR)\Regex.obj" \
	"$(INTDIR)\remote_close.WINDOWS.obj" \
	"$(INTDIR)\security.WINDOWS.obj" \
	"$(INTDIR)\selector.obj" \
	"$(INTDIR)\set_user_priv_from_ad.obj" \
	"$(INTDIR)\setenv.obj" \
	"$(INTDIR)\setup_api_dll.obj" \
	"$(INTDIR)\sig_name.obj" \
	"$(INTDIR)\sqlquery.obj" \
	"$(INTDIR)\stat_info.obj" \
	"$(INTDIR)\stat_wrapper.obj" \
	"$(INTDIR)\stat_wrapper_internal.obj" \
	"$(INTDIR)\status_string.obj" \
	"$(INTDIR)\store_cred.obj" \
	"$(INTDIR)\string_conversion.WINDOWS.obj" \
	"$(INTDIR)\string_list.obj" \
	"$(INTDIR)\stringSpace.obj" \
	"$(INTDIR)\strnewp.obj" \
	"$(INTDIR)\subsystem_info.obj" \
	"$(INTDIR)\system_info.WINDOWS.obj" \
	"$(INTDIR)\time_offset.obj" \
	"$(INTDIR)\tmp_dir.obj" \
	"$(INTDIR)\token_cache.obj" \
	"$(INTDIR)\transfer_request.obj" \
	"$(INTDIR)\translation_utils.obj" \
	"$(INTDIR)\udp_waker.obj" \
	"$(INTDIR)\uids.obj" \
	"$(INTDIR)\usagemon.obj" \
	"$(INTDIR)\user_job_policy.obj" \
	"$(INTDIR)\user_log.obj" \
	"$(INTDIR)\user_log_header.obj" \
	"$(INTDIR)\utc_time.obj" \
	"$(INTDIR)\vm_univ_utils.obj" \
	"$(INTDIR)\waker.obj" \
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
SOURCE="..\src\condor_c++_util\access.cpp"

"$(INTDIR)\access.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\ad_printmask.cpp"

"$(INTDIR)\ad_printmask.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\build_job_env.cpp"

"$(INTDIR)\build_job_env.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\check_events.cpp"

"$(INTDIR)\check_events.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\classad_collection.cpp"

"$(INTDIR)\classad_collection.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\classad_command_util.cpp"

"$(INTDIR)\classad_command_util.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\classad_hashtable.cpp"

"$(INTDIR)\classad_hashtable.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\classad_helpers.cpp"

"$(INTDIR)\classad_helpers.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\classad_log.cpp"

"$(INTDIR)\classad_log.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\classad_merge.cpp"

"$(INTDIR)\classad_merge.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\classad_namedlist.cpp"

"$(INTDIR)\classad_namedlist.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\classad_visa.cpp"

"$(INTDIR)\classad_visa.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\command_strings.cpp"

"$(INTDIR)\command_strings.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\ConcurrencyLimitUtils.cpp"

"$(INTDIR)\ConcurrencyLimitUtils.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\condor_arglist.cpp"

"$(INTDIR)\condor_arglist.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\condor_attributes.cpp"

"$(INTDIR)\condor_attributes.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\condor_common.cpp"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /Gm /Gi- /GX /ZI /Od /I "..\src\condor_tt" /D "WIN32" /D "_DEBUG" /Fp"..\Debug\condor_common.pch" /Yc"condor_common.h" /Fo"..\Debug/" /Fd"..\Debug/" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_DEFINES) $(CONDOR_CPPARGS) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

"..\Debug\condor_common.obj"	"..\Debug\condor_common.pch" : $(SOURCE)
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /Gi- /GX /Z7 /O1 /I "..\src\condor_tt" /D "WIN32" /D "NDEBUG" /Fp"..\Release\condor_common.pch" /Yc"condor_common.h" /Fo"..\Release/" /Fd"..\Release/" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_DEFINES) $(CONDOR_CPPARGS) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

"..\Release\condor_common.obj"	"..\Release\condor_common.pch" : $(SOURCE)
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE="..\src\condor_c++_util\condor_config.cpp"

"$(INTDIR)\condor_config.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\condor_crontab.cpp"

"$(INTDIR)\condor_crontab.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\condor_environ.cpp"

"$(INTDIR)\condor_environ.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\condor_event.cpp"

"$(INTDIR)\condor_event.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\condor_ftp.cpp"

"$(INTDIR)\condor_ftp.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\condor_getcwd.cpp"

"$(INTDIR)\condor_getcwd.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\condor_id.cpp"

"$(INTDIR)\condor_id.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\condor_md.cpp"

"$(INTDIR)\condor_md.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\condor_open.cpp"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /I "..\src\condor_tt" /D "WIN32" /D "_DEBUG" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_DEFINES) $(CONDOR_CPPARGS) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

"$(INTDIR)\condor_open.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /Z7 /O1 /I "..\src\condor_tt" /D "WIN32" /D "NDEBUG" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_DEFINES) $(CONDOR_CPPARGS) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

"$(INTDIR)\condor_open.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE="..\src\condor_c++_util\condor_parameters.cpp"

"$(INTDIR)\condor_parameters.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\condor_q.cpp"

"$(INTDIR)\condor_q.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\condor_query.cpp"

"$(INTDIR)\condor_query.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\condor_state.cpp"

"$(INTDIR)\condor_state.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\condor_url.cpp"

"$(INTDIR)\condor_url.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\condor_user_policy.cpp"

"$(INTDIR)\condor_user_policy.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\condor_ver_info.cpp"

"$(INTDIR)\condor_ver_info.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\condor_version.cpp"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /I "..\src\condor_tt" /D "WIN32" /D "_DEBUG" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_DEFINES) $(CONDOR_CPPARGS) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

"$(INTDIR)\condor_version.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /Z7 /O1 /I "..\src\condor_tt" /D "WIN32" /D "NDEBUG" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_DEFINES) $(CONDOR_CPPARGS) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

"$(INTDIR)\condor_version.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE="..\src\condor_c++_util\CondorError.cpp"

"$(INTDIR)\CondorError.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\config.cpp"

"$(INTDIR)\config.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\cron.cpp"

"$(INTDIR)\cron.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\cronjob.cpp"

"$(INTDIR)\cronjob.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\cronjob_classad.cpp"

"$(INTDIR)\cronjob_classad.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\cronmgr.cpp"

"$(INTDIR)\cronmgr.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_daemon_client\daemon.cpp

"$(INTDIR)\daemon.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_daemon_client\daemon_list.cpp

"$(INTDIR)\daemon_list.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_daemon_client\daemon_types.cpp

"$(INTDIR)\daemon_types.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\date_util.cpp"

"$(INTDIR)\date_util.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\dbms_utils.cpp"

"$(INTDIR)\dbms_utils.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_daemon_client\dc_collector.cpp

"$(INTDIR)\dc_collector.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_daemon_client\dc_message.cpp

"$(INTDIR)\dc_message.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_daemon_client\dc_schedd.cpp

"$(INTDIR)\dc_schedd.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_daemon_client\dc_shadow.cpp

"$(INTDIR)\dc_shadow.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_daemon_client\dc_startd.cpp

"$(INTDIR)\dc_startd.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_daemon_client\dc_starter.cpp

"$(INTDIR)\dc_starter.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_daemon_client\dc_transfer_queue.cpp

"$(INTDIR)\dc_transfer_queue.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_daemon_client\dc_transferd.cpp

"$(INTDIR)\dc_transferd.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\directory.cpp"

"$(INTDIR)\directory.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\directory.WINDOWS.cpp"

"$(INTDIR)\directory.WINDOWS.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\distribution.cpp"

"$(INTDIR)\distribution.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\domain_tools.cpp"

"$(INTDIR)\domain_tools.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\dynuser.cpp"

"$(INTDIR)\dynuser.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\email.cpp"

"$(INTDIR)\email.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\enum_utils.cpp"

"$(INTDIR)\enum_utils.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\env.cpp"

"$(INTDIR)\env.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\error_utils.cpp"

"$(INTDIR)\error_utils.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\extra_param_info.cpp"

"$(INTDIR)\extra_param_info.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\file_lock.cpp"

"$(INTDIR)\file_lock.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\file_sql.cpp"

"$(INTDIR)\file_sql.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\file_transfer.cpp"

"$(INTDIR)\file_transfer.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\file_transfer_db.cpp"

"$(INTDIR)\file_transfer_db.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\file_xml.cpp"

"$(INTDIR)\file_xml.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\filename_tools_cpp.cpp"

"$(INTDIR)\filename_tools_cpp.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\forkwork.cpp"

"$(INTDIR)\forkwork.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\format_time.cpp"

"$(INTDIR)\format_time.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\gahp_common.cpp"

"$(INTDIR)\gahp_common.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\generic_query.cpp"

"$(INTDIR)\generic_query.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\get_daemon_name.cpp"

"$(INTDIR)\get_daemon_name.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\get_full_hostname.cpp"

"$(INTDIR)\get_full_hostname.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\HashTable.cpp"

"$(INTDIR)\HashTable.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\hibernation_manager.cpp"

"$(INTDIR)\hibernation_manager.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\hibernator.cpp"

"$(INTDIR)\hibernator.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\hibernator.WINDOWS.cpp"

"$(INTDIR)\hibernator.WINDOWS.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\historysnapshot.cpp"

"$(INTDIR)\historysnapshot.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\hook_utils.cpp"

"$(INTDIR)\hook_utils.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\iso_dates.cpp"

"$(INTDIR)\iso_dates.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\java_config.cpp"

"$(INTDIR)\java_config.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\job_lease.cpp"

"$(INTDIR)\job_lease.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\jobqueuesnapshot.cpp"

"$(INTDIR)\jobqueuesnapshot.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\KeyCache.cpp"

"$(INTDIR)\KeyCache.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\killfamily.cpp"

"$(INTDIR)\killfamily.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\linebuffer.cpp"

"$(INTDIR)\linebuffer.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\load_dll.cpp"

"$(INTDIR)\load_dll.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\log.cpp"

"$(INTDIR)\log.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\log_transaction.cpp"

"$(INTDIR)\log_transaction.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\lsa_mgr.cpp"

"$(INTDIR)\lsa_mgr.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\MapFile.cpp"

"$(INTDIR)\MapFile.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\metric_units.cpp"

"$(INTDIR)\metric_units.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\misc_utils.cpp"

"$(INTDIR)\misc_utils.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\my_distribution.cpp"

"$(INTDIR)\my_distribution.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\my_dynuser.cpp"

"$(INTDIR)\my_dynuser.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\my_hostname.cpp"

"$(INTDIR)\my_hostname.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\my_popen.cpp"

"$(INTDIR)\my_popen.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\my_subsystem.cpp"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /I "..\src\condor_tt" /D "WIN32" /D "_DEBUG" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_DEFINES) $(CONDOR_CPPARGS) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

"$(INTDIR)\my_subsystem.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /Z7 /O1 /I "..\src\condor_tt" /D "WIN32" /D "NDEBUG" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_DEFINES) $(CONDOR_CPPARGS) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

"$(INTDIR)\my_subsystem.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE="..\src\condor_c++_util\my_username.cpp"

"$(INTDIR)\my_username.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\MyString.cpp"

"$(INTDIR)\MyString.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\net_string_list.cpp"

"$(INTDIR)\net_string_list.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\network_adapter.cpp"

"$(INTDIR)\network_adapter.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\network_adapter.WINDOWS.cpp"

"$(INTDIR)\network_adapter.WINDOWS.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\ntsysinfo.cpp"

"$(INTDIR)\ntsysinfo.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\perm.cpp"

"$(INTDIR)\perm.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\pgsqldatabase.cpp"

"$(INTDIR)\pgsqldatabase.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\print_wrapped_text.cpp"

"$(INTDIR)\print_wrapped_text.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\proc_family_direct.cpp"

"$(INTDIR)\proc_family_direct.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\proc_family_interface.cpp"

"$(INTDIR)\proc_family_interface.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\proc_family_proxy.cpp"

"$(INTDIR)\proc_family_proxy.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\proc_id.cpp"

"$(INTDIR)\proc_id.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\procd_config.cpp"

"$(INTDIR)\procd_config.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\process_control.WINDOWS.cpp"

"$(INTDIR)\process_control.WINDOWS.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\profile.WINDOWS.cpp"

"$(INTDIR)\profile.WINDOWS.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\read_multiple_logs.cpp"

"$(INTDIR)\read_multiple_logs.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\read_user_log.cpp"

"$(INTDIR)\read_user_log.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\read_user_log_state.cpp"

"$(INTDIR)\read_user_log_state.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\Regex.cpp"

"$(INTDIR)\Regex.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\remote_close.WINDOWS.cpp"

"$(INTDIR)\remote_close.WINDOWS.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\security.WINDOWS.cpp"

"$(INTDIR)\security.WINDOWS.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\selector.cpp"

"$(INTDIR)\selector.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\set_user_priv_from_ad.cpp"

"$(INTDIR)\set_user_priv_from_ad.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\setenv.cpp"

"$(INTDIR)\setenv.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\setup_api_dll.cpp"

"$(INTDIR)\setup_api_dll.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\sig_name.cpp"

"$(INTDIR)\sig_name.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\sqlquery.cpp"

"$(INTDIR)\sqlquery.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\stat_info.cpp"

"$(INTDIR)\stat_info.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\stat_wrapper.cpp"

"$(INTDIR)\stat_wrapper.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\stat_wrapper_internal.cpp"

"$(INTDIR)\stat_wrapper_internal.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\status_string.cpp"

"$(INTDIR)\status_string.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\store_cred.cpp"

"$(INTDIR)\store_cred.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\string_conversion.WINDOWS.cpp"

"$(INTDIR)\string_conversion.WINDOWS.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\string_list.cpp"

"$(INTDIR)\string_list.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\stringSpace.cpp"

"$(INTDIR)\stringSpace.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\strnewp.cpp"

"$(INTDIR)\strnewp.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\subsystem_info.cpp"

"$(INTDIR)\subsystem_info.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\system_info.WINDOWS.cpp"

"$(INTDIR)\system_info.WINDOWS.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\time_offset.cpp"

"$(INTDIR)\time_offset.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\tmp_dir.cpp"

"$(INTDIR)\tmp_dir.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\token_cache.cpp"

"$(INTDIR)\token_cache.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\transfer_request.cpp"

"$(INTDIR)\transfer_request.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\translation_utils.cpp"

"$(INTDIR)\translation_utils.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\udp_waker.cpp"

"$(INTDIR)\udp_waker.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\uids.cpp"

"$(INTDIR)\uids.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\usagemon.cpp"

"$(INTDIR)\usagemon.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\user_job_policy.cpp"

"$(INTDIR)\user_job_policy.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\user_log.cpp"

"$(INTDIR)\user_log.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\user_log_header.cpp"

"$(INTDIR)\user_log_header.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\utc_time.cpp"

"$(INTDIR)\utc_time.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\vm_univ_utils.cpp"

"$(INTDIR)\vm_univ_utils.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\waker.cpp"

"$(INTDIR)\waker.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\which.cpp"

"$(INTDIR)\which.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\src\condor_c++_util\windows_firewall.cpp"

"$(INTDIR)\windows_firewall.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)



!ENDIF 

