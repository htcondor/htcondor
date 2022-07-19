 ###############################################################
 # 
 # Copyright 2011 Red Hat, Inc. 
 # 
 # Licensed under the Apache License, Version 2.0 (the "License"); you 
 # may not use this file except in compliance with the License.  You may 
 # obtain a copy of the License at 
 # 
 #    http://www.apache.org/licenses/LICENSE-2.0 
 # 
 # Unless required by applicable law or agreed to in writing, software 
 # distributed under the License is distributed on an "AS IS" BASIS, 
 # WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 # See the License for the specific language governing permissions and 
 # limitations under the License. 
 # 
 ############################################################### 

condor_exe( condor_testingd "testingd.cpp" ${C_LIBEXEC} "${CONDOR_LIBS}" OFF )
condor_exe( condor_sinful "sinful-tool.cpp" ${C_LIBEXEC} "${CONDOR_LIBS}" OFF )
condor_exe( test_user_mapping "test_user_mapping.cpp" ${C_LIBEXEC} "${CONDOR_LIBS}" OFF )

if( NOT BUILD_DATE )
	GET_DATE( BUILD_DATE )
endif()
if(BUILD_DATE)
	set_source_files_properties(condor_version.cpp PROPERTIES COMPILE_DEFINITIONS BUILD_DATE="${BUILD_DATE}")
endif()

# We need this .o statically linked into every exe for the version object
# to parse.  Make this an object library to link everywhere.
add_library(condor_version_obj STATIC condor_version.cpp)

# These are the sources for .o's that go into both libcondor_utils and libcondorapi
# Ideally, as we break dependencies, more of these move into just libcondor_utils
set(CONDOR_API_AND_UTILS_SRC
basename.cpp
chomp.cpp
classad_merge.cpp
classad_merge.h
compat_classad.cpp
compat_classad.h
compat_classad_util.cpp
compat_classad_util.h
condor_arglist.cpp
condor_arglist.h
condor_event.cpp
condor_event.h
condor_snutils.cpp
condor_ver_info.cpp
condor_ver_info.h
condor_version.h
condor_version.cpp
directory_util.cpp
directory_util.h
env.cpp
env.h
except.cpp
file_lock.cpp
filename_tools.cpp
get_random_num.cpp
HashTable.cpp
HashTable.h
iso_dates.cpp
iso_dates.h
MyString.cpp
MyString.h
read_user_log.cpp
read_user_log.h
read_user_log_state.cpp
read_user_log_state.h
setenv.cpp
setenv.h
stat_wrapper.cpp
stat_wrapper.h
stl_string_utils.cpp
stl_string_utils.h
strcasestr.cpp
string_list.cpp
string_list.h
strnewp.cpp
strupr.cpp
subsystem_info.cpp
subsystem_info.h
ToE.cpp
user_log_header.cpp
user_log_header.h
utc_time.cpp
utc_time.h
)

if (NOT WINDOWS)
	list(APPEND CONDOR_API_AND_UTILS_SRC
		dprintf_syslog.cpp
		dprintf_syslog.h
	)
endif(NOT WINDOWS)

if(WINDOWS)
	list(APPEND CONDOR_API_AND_UTILS_SRC
    	directory.WINDOWS.cpp
		directory_util.cpp
		dynuser.WINDOWS.cpp
		get_last_error_string.WINDOWS.cpp
		lock_file.WINDOWS.cpp
		lsa_mgr.cpp
		ntsysinfo.WINDOWS.cpp
		perm.WINDOWS.cpp
		posix.WINDOWS.cpp
		process_control.WINDOWS.cpp
		stat.WINDOWS.cpp
		token_cache.WINDOWS.cpp
		truncate.WINDOWS.cpp
		uids.windows.credd.cpp
	)
endif(WINDOWS)

if(NOT WINDOWS)
	list(APPEND CONDOR_API_AND_UTILS_SRC
		lock_file.unix.cpp
	)
endif(NOT WINDOWS)

# These sources compile to .o's which only go into libcondorapi
set(CONDOR_API_SRC
	libcondorapi_stubs.cpp
	../condor_sysapi/kernel_version.cpp
	../condor_sysapi/reconfig.cpp
	../condor_sysapi/sysapi.h
)

# These are the sources which only go into libcondorutils
set(CONDOR_UTILS_SRC
access.cpp
access.h
adcluster.cpp
adcluster.h
ad_printmask.cpp
ad_printmask.h
ad_transforms.cpp
ad_transforms.h
AWSv4-utils.cpp
AWSv4-utils.h
AWSv4-impl.h
backward_file_reader.cpp
binary_search.h
build_job_env.cpp
build_job_env.h
check_events.cpp
check_events.h
checksum.cpp
checksum.h
classad_collection.cpp
classad_collection.h
classad_command_util.cpp
classad_command_util.h
classad_cron_job.cpp
classad_helpers.cpp
classad_helpers.h
classadHistory.cpp
classadHistory.h
classad_log.cpp
ClassAdLogEntry.cpp
ClassAdLogEntry.h
classad_log.h
ClassAdLogParser.cpp
ClassAdLogParser.h
ClassAdLogPlugin.h
ClassAdLogProber.cpp
ClassAdLogProber.h
ClassAdLogReader.cpp
ClassAdLogReader.h
classad_oldnew.cpp
classad_oldnew.h
classad_usermap.cpp
classad_visa.cpp
classad_visa.h
classy_counted_ptr.h
command_name_tables.h
command_strings.cpp
command_strings.h
compat_classad_list.cpp
compat_classad_list.h
condor_adtypes.cpp
condor_api.h
condor_attributes.cpp
condor_base64.cpp
condor_base64.h
condor_blkng_full_disk_io.cpp
condor_claimid_parser.h
condor_common.cpp
condor_config.cpp
condor_crontab.cpp
condor_crontab.h
condor_environ.cpp
CondorError.cpp
CondorError.h
condor_fsync.cpp
condor_fsync.h
condor_ftp.cpp
condor_getcwd.cpp
condor_getcwd.h
condor_holdcodes.h
condor_id.cpp
condor_id.h
condor_md.cpp
condor_mkstemp.cpp
condor_netaddr.cpp
condor_netdb.cpp
condor_open.h
condor_perms.cpp
condor_pidenvid.cpp
condor_q.cpp
condor_q.h
condor_query.cpp
condor_query.h
condor_regex.cpp
condor_regex.h
condor_scitokens.cpp
condor_scitokens.h
condor_sinful.cpp
condor_sinful.h
condor_sockaddr.cpp
condor_sockfunc.cpp
condor_sockfunc_ex.cpp
condor_state.cpp
condor_threads.cpp
condor_threads.h
condor_timeslice.cpp
condor_timeslice.h
condor_universe.cpp
condor_url.cpp
condor_url.h
condor_user_policy.cpp
condor_user_policy.h
config.cpp
console-utils.cpp
console-utils.h
consumption_policy.cpp
consumption_policy.h
copy_file.cpp
credmon_interface.cpp
credmon_interface.h
cron_job.cpp
cron_job_io.cpp
cron_job_list.cpp
cron_job_mgr.cpp
cron_job_mode.cpp
cron_job_params.cpp
cronos.cpp
cron_param.cpp
dagman_utils.cpp
dagman_utils.h
data_reuse.cpp
data_reuse.h
date_util.cpp
date_util.h
dc_service.cpp
dc_service.h
DelegationInterface.cpp
DelegationInterface.h
d_format_time.cpp
directory.cpp
directory.h
docker-api.cpp
docker-api.h
domain_tools.cpp
domain_tools.h
dprintf_common.cpp
dprintf_config.cpp
dprintf.cpp
dprintf_setup.cpp
dynuser.h
email.cpp
email_cpp.cpp
email_file.cpp
enum_utils.cpp
enum_utils.h
error_utils.cpp
error_utils.h
escapes.cpp
exit_utils.cpp
expr_analyze.cpp
expr_analyze.h
extArray.h
file_modified_trigger.cpp
file_modified_trigger.h
filename_tools_cpp.cpp
filesystem_remap.cpp
filesystem_remap.h
file_transfer.cpp
file_transfer.h
file_transfer_stats.cpp
file_transfer_stats.h
forkwork.cpp
forkwork.h
format_time.cpp
format_time.h
fs_util.cpp
gahp_common.cpp
gahp_common.h
generic_query.cpp
generic_query.h
generic_stats.cpp
generic_stats.h
get_daemon_name.cpp
get_daemon_name.h
get_exec_path.cpp
get_port_range.cpp
get_random_num_secure.cpp
globus_utils.cpp
hashkey.cpp
hashkey.h
hibernation_manager.cpp
hibernation_manager.h
hibernator.cpp
hibernator.h
hibernator.tools.cpp
hibernator.tools.h
historyFileFinder.cpp
historyFileFinder.h
history_queue.cpp
history_queue.h
history_utils.h
hook_utils.cpp
hook_utils.h
internet.cpp
ipv6_addrinfo.cpp
ipv6_hostname.cpp
ipv6_interface.cpp
ipv6_interface.h
java_config.cpp
java_config.h
JobLogMirror.cpp
JobLogMirror.h
job_ad_instance_recording.cpp
job_ad_instance_recording.h
KeyCache.cpp
killfamily.cpp
killfamily.h
limit.h
limit_directory_access.cpp
limit_directory_access.h
linebuffer.cpp
link.cpp
link.h
list.h
LoadPlugins.cpp
LoadPlugins.h
log.cpp
log.h
log_rotate.cpp
log_rotate.h
log_transaction.cpp
log_transaction.h
make_printmask.cpp
manifest.cpp
manifest.h
MapFile.cpp
MapFile.h
mark_thread.cpp
match_prefix.cpp
metric_units.cpp
metric_units.h
misc_utils.cpp
misc_utils.h
mk_cache_links.cpp
mk_cache_links.h
my_async_fread.cpp
my_async_fread.h
my_getopt.cpp
my_hostname.cpp
my_hostname.h
my_popen.cpp
my_popen.h
my_username.cpp
my_username.h
named_classad.cpp
named_classad_list.cpp
NegotiationUtils.cpp
NegotiationUtils.h
net_string_list.cpp
net_string_list.h
network_adapter.cpp
network_adapter.h
nullfile.cpp
overflow.cpp
overflow.h
param_info.cpp
param_info.h
param_info_help.cpp
param_info_tables.h
path_utils.cpp
path_utils.h
perm.h
PluginManager.h
printf_format.cpp
print_wrapped_text.cpp
print_wrapped_text.h
prioritysimplelist.h
proc.cpp
procd_config.cpp
procd_config.h
proc_family_direct.cpp
proc_family_direct.h
proc_family_interface.cpp
proc_family_interface.h
proc_family_proxy.cpp
proc_family_proxy.h
proc_id.cpp
query_result_type.h
ranger.cpp
ranger.h
read_multiple_logs.cpp
read_multiple_logs.h
rotate_file.cpp
secure_file.cpp
secure_file.h
selector.cpp
selector.h
set_user_priv_from_ad.cpp
shortfile.cpp
shortfile.h
sig_name.cpp
sig_name.h
simple_arg.cpp
simple_arg.h
simplelist.h
socket_proxy.cpp
socket_proxy.h
SourceRoute.cpp
SourceRoute.h
spooled_job_files.cpp
spooled_job_files.h
spool_version.cpp
spool_version.h
ssl_version_hack.h
stat_info.cpp
stat_info.h
status_string.cpp
status_string.h
status_types.h
stopwatch.h
store_cred.cpp
store_cred.h
stringSpace.cpp
stringSpace.h
str_isxxx.cpp
str_isxxx.h
submit_glob.cpp
submit_protocol.cpp
submit_protocol.h
submit_utils.cpp
submit_utils.h
systemd_manager.cpp
systemd_manager.h
threads_implementation.h
timed_queue.h
time_offset.cpp
time_offset.h
tmp_dir.cpp
tmp_dir.h
ToE.h
token_cache.h
tokener.cpp
tokener.h
token_utils.cpp
token_utils.h
totals.cpp
totals.h
transfer_request.cpp
translation_utils.cpp
translation_utils.h
tree.h
uids.cpp
udp_waker.cpp
udp_waker.h
usagemon.cpp
usagemon.h
user_job_policy.cpp
user_job_policy.h
user_log.c++.h
userlog_to_classads.cpp
userlog_to_classads.h
vm_univ_utils.cpp
vm_univ_utils.h
wait_for_user_log.cpp
wait_for_user_log.h
waker.cpp
waker.h
which.cpp
which.h
write_user_log.cpp
write_user_log.h
write_user_log_state.cpp
write_user_log_state.h
xform_utils.cpp
xform_utils.h
zkm_base64.cpp
zkm_base64.h
)

if(NOT WINDOWS)
	list(APPEND CONDOR_UTILS_SRC
		access_euid.unix.cpp
		ClassAdLogPluginManager.unix.cpp
		detach.unix.cpp
		fdpass.unix.cpp
		getdtablesize.unix.cpp
		getmnt.unix.cpp
		limit.unix.cpp
		network_adapter.unix.cpp
		network_adapter.unix.h
		passwd_cache.unix.cpp
		passwd_cache.unix.h
		sig_install.unix.cpp
		sleep.unix.cpp
	)
endif(NOT WINDOWS)

if(LINUX)
	list(APPEND CONDOR_UTILS_SRC
		hibernator.linux.cpp
		hibernator.linux.h
		network_adapter.linux.cpp
		network_adapter.linux.h
	)
endif(LINUX)

if(WINDOWS)
	list(APPEND CONDOR_UTILS_SRC
		access_desktop.WINDOWS.cpp
		executable_scripts.WINDOWS.cpp
		firewall.WINDOWS.cpp
		hibernator.WINDOWS.cpp
		load_dll.WINDOWS.cpp
		network_adapter.WINDOWS.cpp
		profile.WINDOWS.cpp
		remote_close.WINDOWS.cpp
		rmdir.WINDOWS.cpp
		security.WINDOWS.cpp
		setup_api_dll.WINDOWS.cpp
		string_conversion.WINDOWS.cpp
		system_info.WINDOWS.cpp
		uuid_lib.WINDOWS.cpp
	)
endif(WINDOWS)


##################################################
add_custom_command (OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/command_name_tables.h
		COMMAND perl
		ARGS ${CMAKE_CURRENT_SOURCE_DIR}/command_table_generator.pl -i ${CMAKE_CURRENT_SOURCE_DIR}/../condor_includes/condor_commands.h -o ${CMAKE_CURRENT_BINARY_DIR}/command_name_tables.h
		DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../condor_includes/condor_commands.h ${CMAKE_CURRENT_SOURCE_DIR}/command_table_generator.pl)

add_custom_command (OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/param_info_tables.h
		COMMAND perl
		ARGS ${CMAKE_CURRENT_SOURCE_DIR}/param_table_generator.pl -i ${CMAKE_CURRENT_SOURCE_DIR}/param_info.in -o ${CMAKE_CURRENT_BINARY_DIR}/param_info_tables.h -p ${PLATFORM}
		DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/param_info.in ${CMAKE_CURRENT_SOURCE_DIR}/param_table_generator.pl)

add_custom_target (utils_genparams ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/param_info_tables.h ${CMAKE_CURRENT_BINARY_DIR}/command_name_tables.h)

if(UNIX)
	set_source_files_properties(param_info.cpp PROPERTIES COMPILE_FLAGS -Wno-unused-parameter)
endif(UNIX)

############ end generate params
# These sources depend on the generated param_info_tables.  Everything else can build in parallel
set_source_files_properties(config.cpp condor_config.cpp param_info.cpp param_info_help.cpp PROPERTIES OBJECT_DEPENDS  ${CMAKE_CURRENT_BINARY_DIR}/param_info_tables.h)

# Only command_strings.cpp depends on the generator above.  Everything else can build in parallel
set_source_files_properties(command_strings.cpp PROPERTIES OBJECT_DEPENDS  ${CMAKE_CURRENT_BINARY_DIR}/command_name_tables.h)


# These are source files that reside in other directories that we want to
# include in libcondor_utils. They used to be in separate libraries.
set( EXTRA_SRCS "${SAFE_OPEN_SRC};${ANALYSIS_UTIL_SRCS};${CCB_UTIL_SRCS};${CEDAR_UTIL_SRCS};${DAEMON_CLIENT_UTIL_SRCS};${DAEMON_CORE_UTIL_SRCS};${PROCAPI_UTIL_SRCS};${PROCD_CLIENT_UTIL_SRCS};${QMGMT_UTIL_SRCS};${SYSAPI_UTIL_SRCS}" )
if (UNIX)
set_source_files_properties(../condor_sysapi/clinpack.cpp PROPERTIES COMPILE_FLAGS -w)
set_source_files_properties(../condor_daemon_core.V6/self_monitor.cpp ../classad_analysis/analysis.cpp ../classad_analysis/interval.cpp generic_stats.cpp generic_query.cpp PROPERTIES COMPILE_FLAGS -Wno-float-equal)
endif(UNIX)

# Turn off the OpenSSL deprecation warnings until we decide on the long-term direction with OpenSSL on Mac.
if (APPLE)
	set_source_files_properties(condor_base64.cpp PROPERTIES COMPILE_FLAGS -Wno-deprecated-declarations)
	set_source_files_properties(condor_dh.cpp PROPERTIES COMPILE_FLAGS -Wno-deprecated-declarations)
	set_source_files_properties(condor_md.cpp PROPERTIES COMPILE_FLAGS -Wno-deprecated-declarations)
	set_source_files_properties(../condor_io/condor_auth_passwd.cpp PROPERTIES COMPILE_FLAGS -Wno-deprecated-declarations)
	set_source_files_properties(../condor_io/condor_auth_ssl.cpp PROPERTIES COMPILE_FLAGS -Wno-deprecated-declarations)
	set_source_files_properties(../condor_io/condor_crypt.cpp PROPERTIES COMPILE_FLAGS -Wno-deprecated-declarations)
	set_source_files_properties(../condor_io/condor_crypt_3des.cpp PROPERTIES COMPILE_FLAGS -Wno-deprecated-declarations)
	set_source_files_properties(../condor_io/condor_crypt_blowfish.cpp PROPERTIES COMPILE_FLAGS -Wno-deprecated-declarations)
endif()

# Build "Object" libraries, which are just a set of .o files which can later to into libraries
add_library(condor_api_and_utils_objects OBJECT ${CONDOR_API_AND_UTILS_SRC})

# The utils need pcre2
# Older cmakes don't allow target_link_libraries to OBJECT libraries
# remove this when we drop Ubuntu 18
include_directories(${PCRE2_INCLUDE})
include_directories(${OPENSSL_INCLUDE})
if (${CMAKE_VERSION} VERSION_LESS 3.12.0) 
	include_directories(${PCRE2_INCLUDE})
else()
	target_link_libraries(condor_api_and_utils_objects INTERFACE pcre2-8)
endif()

add_library(condor_utils_objects OBJECT ${CONDOR_UTILS_SRC})
if (${CMAKE_VERSION} VERSION_LESS 3.12.0) 
	include_directories(${PCRE2_INCLUDE})
else()
	target_link_libraries(condor_utils_objects INTERFACE pcre2-8)
	if (WITH_SCITOKENS)
		target_link_libraries(condor_utils_objects PRIVATE SciTokens-headers)
	endif()
endif()

add_dependencies(condor_utils_objects utils_genparams)
add_library(condor_extra_objects OBJECT ${EXTRA_SRCS})

# The extra libraries also need pcre2
if (${CMAKE_VERSION} VERSION_LESS 3.12.0) 
	include_directories(${PCRE2_INCLUDE})
else()
	target_link_libraries(condor_extra_objects PUBLIC pcre2-8)
	if (WITH_SCITOKENS)
		target_link_libraries(condor_extra_objects PRIVATE SciTokens-headers)
	endif()
endif()

add_library(condor_safe_objects OBJECT ${SAFE_OPEN_SRC})

# linux condor_utils.so from the .o's built above
condor_shared_lib(condor_utils 
	$<TARGET_OBJECTS:condor_api_and_utils_objects>
	$<TARGET_OBJECTS:condor_utils_objects>
	$<TARGET_OBJECTS:condor_extra_objects>
)

if ( ${PACKAGE_VERSION} MATCHES "([0-9]+)[.]([0-9]+)[.]([0-9]+)" )
	set( UTILS_LIB_NAME "condor_utils_${CMAKE_MATCH_1}_${CMAKE_MATCH_2}_${CMAKE_MATCH_3}" )
else()
	message(FATAL_ERROR "Can't determine Condor version!")
endif()
set_target_properties( condor_utils PROPERTIES OUTPUT_NAME "${UTILS_LIB_NAME}" )
if (HAVE_GNU_LD)
	target_link_libraries( condor_utils PRIVATE -Wl,--wrap,exit )
endif()

# for linux also make a static version of condor_utils
#
if (LINUX OR APPLE)
   condor_static_lib(condor_utils_s
		$<TARGET_OBJECTS:condor_api_and_utils_objects>
		$<TARGET_OBJECTS:condor_utils_objects>
		$<TARGET_OBJECTS:condor_extra_objects>
	)
   set_target_properties( condor_utils_s PROPERTIES OUTPUT_NAME "${UTILS_LIB_NAME}" )
   if (HAVE_GNU_LD)
	target_link_libraries( condor_utils_s -Wl,--wrap,exit )
   endif()
endif(LINUX OR APPLE)


# Brain twister:
#
# If you dynamicly load a library which links against libcondor_utils (such as
# a python module), the dprintf inside libcondor_utils will be linked against
# the libc dprintf.  Hilarity ensues as internal condor functions use the libc
# dprintf.
#
# The magic below will cause internal references to dprintf to be changed to
# __wrap_dprintf, which is also provided by condor_utils.  Hence, when
# dynamically loaded, Condor's __wrap_dprintf (identical to Condor's dprintf)
# is used instead of libc's.  Condor's dprintf is still exported, meaning
# linking executables against condor_utils still works.
#
# Voodoo, that's what this is.  Never overload libc functions!
#
if ( LINUX )
  set_target_properties( condor_utils PROPERTIES LINK_FLAGS "-Wl,--wrap=dprintf" )
endif()

if (DLOPEN_GSI_LIBS OR DLOPEN_VOMS_LIBS)
	target_link_libraries(condor_utils PUBLIC ${RT_FOUND} ${CLASSADS_FOUND} ${KRB5_FOUND} ${MUNGE_FOUND} crypto )
else()
	target_link_libraries(condor_utils PUBLIC ${RT_FOUND} ${CLASSADS_FOUND} ${VOMS_FOUND} ${GLOBUS_FOUND} openssl crypto ${KRB5_FOUND} ${MUNGE_FOUND} )
	if (WITH_SCITOKENS)
		target_link_libraries(condor_utils PUBLIC SciTokens)
	endif()

endif()
if (LINUX AND LIBUUID_FOUND)
	target_link_libraries(condor_utils PUBLIC ${LIBUUID_FOUND})
endif()

if ( APPLE )
	target_link_libraries( condor_utils PUBLIC ${IOKIT_FOUND} ${COREFOUNDATION_FOUND} resolv )
	set_target_properties( condor_utils PROPERTIES INSTALL_NAME_DIR ${CMAKE_CURRENT_BINARY_DIR} )
	install( CODE "execute_process(COMMAND ${CMAKE_SOURCE_DIR}/src/condor_scripts/macosx_rewrite_libs \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${C_LIB}/lib${UTILS_LIB_NAME}.dylib)" )
endif()

##################################################
# condorapi & tests

if(WINDOWS)
    set_property( TARGET utils_genparams PROPERTY FOLDER "libraries" )
endif()

add_library(condor_api_objects OBJECT ${CONDOR_API_SRC})
if (${CMAKE_VERSION} VERSION_LESS 3.12.0) 
	include_directories(${PCRE2_INCLUDE})
else()
	target_link_libraries(condor_api_objects pcre2-8)
endif()

condor_static_lib(condorapi 
						$<TARGET_OBJECTS:condor_api_objects>
						$<TARGET_OBJECTS:condor_api_and_utils_objects>
						$<TARGET_OBJECTS:condor_safe_objects>
)
target_link_libraries(condorapi pcre2-8)

#add_dependencies( condorapi condor_utils )

if (HAVE_OLD_SCANDIR)
	add_definitions(-DHAVE_OLD_SCANDIR)
endif()

if (DOCKER_ALLOW_RUN_AS_ROOT)
	set_source_files_properties(docker-api.cpp PROPERTIES COMPILE_DEFINITIONS DOCKER_ALLOW_RUN_AS_ROOT=1)
endif()

target_link_libraries(condorapi classads)
if (LINUX AND LIBUUID_FOUND)
	target_link_libraries(condorapi ${LIBUUID_FOUND})
endif()

if (LINUX OR APPLE)
	condor_shared_lib(condorapi_shared 
						$<TARGET_OBJECTS:condor_api_objects>
						$<TARGET_OBJECTS:condor_api_and_utils_objects>
						$<TARGET_OBJECTS:condor_safe_objects>
	)
	#add_dependencies( condorapi_shared condor_utils )
	target_link_libraries(condorapi_shared ${CLASSADS_FOUND};${CMAKE_DL_LIBS};${RT_FOUND})
	set_target_properties( condorapi_shared PROPERTIES OUTPUT_NAME condorapi )
endif()

install (FILES condor_event.h
		 read_user_log.h
		DESTINATION ${C_INCLUDE})

install ( TARGETS condorapi DESTINATION ${C_LIB} )
if (LINUX OR APPLE)
    install ( TARGETS condorapi_shared DESTINATION ${C_LIB} )
endif()
if (APPLE)
    install( CODE "execute_process(COMMAND ${CMAKE_SOURCE_DIR}/src/condor_scripts/macosx_rewrite_libs \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${C_LIB}/libcondorapi.dylib)" )
endif()

set_source_files_properties(test_log_reader.cpp PROPERTIES DEFINITIONS ENABLE_STATE_DUMP)

condor_exe_test(test_log_reader "test_log_reader.cpp" "${CONDOR_TOOL_LIBS}")
condor_exe_test(test_log_reader_state "test_log_reader_state.cpp" "${CONDOR_TOOL_LIBS}")
condor_exe_test(test_log_writer "test_log_writer.cpp" "${CONDOR_TOOL_LIBS}")
condor_exe_test(test_libcondorapi "test_libcondorapi.cpp" "condorapi")

condor_exe_test(test_sinful "test_sinful.cpp" "${CONDOR_TOOL_LIBS}" )
condor_exe_test(test_macro_expand "test_macro_expand.cpp" "${CONDOR_TOOL_LIBS}" )
