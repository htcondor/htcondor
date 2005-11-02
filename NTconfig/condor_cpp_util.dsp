# Microsoft Developer Studio Project File - Name="condor_cpp_util" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=condor_cpp_util - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "condor_cpp_util.mak".
!MESSAGE 
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

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\Debug"
# PROP Intermediate_Dir "..\Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /Z7 /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /D "WIN32" /D "_DEBUG" /Fp"..\Debug\condor_common.pch" /Yu"condor_common.h" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "condor_cpp_util___Win32_Release"
# PROP BASE Intermediate_Dir "condor_cpp_util___Win32_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\Release"
# PROP Intermediate_Dir "..\Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /GX /Z7 /Od /D "WIN32" /D "_DEBUG" /Fp"..\src\condor_c++_util/condor_common.pch" /Yu"condor_common.h" /FD /TP /c
# SUBTRACT BASE CPP /Fr
# ADD CPP /nologo /MT /W3 /GX /Z7 /O1 /D "WIN32" /D "NDEBUG" /Fp"..\Release\condor_common.pch" /Yu"condor_common.h" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "condor_cpp_util - Win32 Debug"
# Name "condor_cpp_util - Win32 Release"
# Begin Source File

SOURCE="..\src\condor_c++_util\access.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\access.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\ad_printmask.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\ad_printmask.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\build_job_env.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\build_job_env.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\check_events.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\check_events.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\check_log_files.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\classad_collection.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\classad_collection.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\classad_command_util.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\classad_command_util.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\classad_hashtable.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\classad_hashtable.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\classad_helpers.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\classad_helpers.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\classad_log.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\classad_log.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\classad_merge.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\classad_merge.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\classad_namedlist.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\command_strings.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\command_strings.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_attributes.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_common.C"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

# PROP Intermediate_Dir "..\Debug"
# ADD CPP /Gi- /Yc"condor_common.h"

!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Release"

# PROP Intermediate_Dir "..\Release"
# ADD CPP /Gi- /Yc"condor_common.h"

!ENDIF 

# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_config.C"
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_config.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_cron.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_cronjob.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_cronmgr.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_client\condor_daemon_client.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_distribution.h
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_environ.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_event.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_event.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_id.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_id.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_md.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_parameters.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_q.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_q.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_query.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_query.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_state.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_ver_info.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_ver_info.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_version.C"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\CondorError.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\CondorError.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\config.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\cron.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\cronjob.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\cronjob_classad.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\cronmgr.C"
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_client\daemon.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_client\daemon.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_client\daemon_list.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_client\daemon_list.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_client\daemon_types.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_client\daemon_types.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_client\dc_collector.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_client\dc_collector.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_client\dc_schedd.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_client\dc_schedd.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_client\dc_shadow.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_client\dc_shadow.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_client\dc_startd.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_client\dc_startd.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_client\dc_starter.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_client\dc_starter.h
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\directory.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\directory.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\distribution.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\domain_tools.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\domain_tools.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\dynuser.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\dynuser.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\email_cpp.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\enum_utils.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\enum_utils.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\env.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\env.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\environ.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\environ.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\error_utils.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\extArray.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\extra_param_info.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\extra_param_info.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\file_lock.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\file_transfer.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\file_transfer.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\format_time.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\format_time.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\gahp_common.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\gahp_common.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\generic_query.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\generic_query.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\get_daemon_name.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\get_daemon_name.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\get_full_hostname.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\get_mysubsystem.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\get_mysubsystem.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\HashTable.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\HashTable.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\historysnapshot.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\historysnapshot.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\iso_dates.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\iso_dates.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\java_config.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\java_config.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\job_lease.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\job_lease.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\jobqueuesnapshot.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\jobqueuesnapshot.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\KeyCache.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\killfamily.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\killfamily.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\linebuffer.C"
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\linebuffer.h
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\list.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\log.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\log.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\log_transaction.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\log_transaction.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\lsa_mgr.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\lsa_mgr.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\metric_units.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\metric_units.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\misc_utils.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\my_distribution.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\my_dynuser.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\my_hostname.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\my_subsystem.C"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\my_username.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\my_username.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\MyString.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\MyString.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\ntsysinfo.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\ntsysinfo.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\perm.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\perm.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\pgsqldatabase.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\pgsqldatabase.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\print_wrapped_text.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\print_wrapped_text.h"
# End Source File
# Begin Source File

SOURCE=..\src\h\proc.h
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\proc_id.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\query_result_type.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\Queue.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\read_multiple_logs.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\read_multiple_logs.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\Set.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\setenv.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\setenv.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\sig_name.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\sig_name.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\simplelist.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\sqlquery.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\sqlquery.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\stat_wrapper.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\stat_wrapper.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\status_string.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\status_string.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\store_cred.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\store_cred.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\string_list.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\string_list.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\stringSpace.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\stringSpace.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\strnewp.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\tmp_dir.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\tmp_dir.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\token_cache.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\token_cache.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\translation_utils.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\translation_utils.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\uids.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\usagemon.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\user_job_policy.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\user_job_policy.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\user_log.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\user_log.c++.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\utc_time.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\utc_time.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\which.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\which.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\windows_firewall.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\windows_firewall.h"
# End Source File
# End Target
# End Project
