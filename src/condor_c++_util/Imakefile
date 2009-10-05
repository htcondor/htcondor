#if HAVE_EXT_CLASSADS
#define New_classads_includes	-I$(NEW_CLASSADS_INC) -I../classad_analysis
#define New_classads_obj	credential.o X509credential.o classad_oldnew.o newclassad_stream.o
#define New_classads_dc_obj	dc_credd.o dc_lease_manager.o dc_lease_manager_lease.o
#define New_classads_lib	-L$(NEW_CLASSADS_LIB) -lclassad_ns
#define New_classads_newold classad_newold.o
#else
#define New_classads_includes
#define New_classads_obj
#define New_classads_dc_obj
#define New_classads_lib
#define New_classads_newold
#endif

#if HAVE_DLOPEN
PLUGIN_OBJ = ClassAdLogPluginManager.o LoadPlugins.o
#endif

#if HAVE_CC_SHARED_FLAG
LIBCONDORAPI = libcondorapi.a libcondorapi.so
TEST_LIBCONDORAPI = test_libcondorapi test_libcondorapi_shared
#else
LIBCONDORAPI = libcondorapi.a
TEST_LIBCONDORAPI = test_libcondorapi
#endif

all_target( param_info_init.c cplus_lib.a liburl.a $(LIBCONDORAPI) $(TEST_LIBCONDORAPI) )

param_info_init.c: param_info.in param_info_c_generator.pl
	chmod u+x param_info_c_generator.pl
	./param_info_c_generator.pl

testbin_target(test_libcondorapi,755)
testbin_target(test_condor.log,644)

#if HAVE_CC_SHARED_FLAG
testbin_target(test_libcondorapi_shared,755)
testbin_target(libcondorapi.so,755)
#endif

C_PLUS_FLAGS =	\
		$(STD_C_PLUS_FLAGS) \
		New_classads_includes \
		$(QUILL_INC) \
		$(CONFIGURE_PCRE_CFLAGS) \
		$(TT_INC) -I../condor_tt

CFLAGS = $(STD_C_FLAGS) New_classads_includes
LIB = $(STD_LIBS) $(CONFIGURE_PCRE_LDFLAGS)

/* This is a seperate job_queue.log parser and a prober for changes object which
	is used by quill and the job router. Eventually the job_queue.log
	readers/writers between the schedd and this should be unified into one
	codebase. */
JOB_QUEUE_LOG_PARSER_OBJ = classadlogentry.o classadlogparser.o prober.o \
				JobLogReader.o

URL_OBJ = url_condor.o cbstp_url.o file_url.o http_url.o filter_url.o \
	mailto_url.o cfilter_url.o ftp_url.o include_urls.o mkargv.o MapFile.o

API_OBJ = generic_query.o condor_query.o condor_q.o ad_printmask.o \
	condor_event.o \
	write_user_log.o \
	write_user_log_state.o \
	user_log_header.o \
	read_user_log.o \
	read_user_log_state.o \
	strnewp.o stat_wrapper.o stat_wrapper_internal.o \
	read_multiple_logs.o CondorError.o check_events.o tmp_dir.o

#if HAVE_EXT_OPENSSL
DH_OBJ = condor_dh.o
BASE64_OBJ = condor_base64.o
#endif

/* Machines that do not have the seteuid call need this particular object file
	to allow proper linkage in libcondorapi.a */
#if !HAVE_SETEUID
SETEUID_OBJ = ../condor_util_lib/seteuid.o
#else
SETEUID_OBJ = 
#endif

HIBERNATION_OBJS = hibernation_manager.o hibernator.o network_adapter.o \
	udp_waker.o waker.o
#if IS_LINUX
HIBERNATION_OBJS += hibernator.linux.o \
	network_adapter.unix.o network_adapter.linux.o
#endif

#if !HAVE_SETEGID
SETEGID_OBJ = ../condor_util_lib/setegid.o
#else
SETEGID_OBJ = 
#endif

#ifdef WANT_QUILL
#if HAVE_ORACLE
ORACLE_OBJS = oracledatabase.o
#else
ORACLE_OBJS = 
#endif

QUILL_OBJS = jobqueuesnapshot.o historysnapshot.o pgsqldatabase.o sqlquery.o $(ORACLE_OBJS) dbms_utils.o
#else
QUILL_OBJS =
#endif

#ifdef WANT_QUILL
TT_OBJS = pgsqldatabase.o file_sql.o file_transfer_db.o 
#else
TT_OBJS = file_sql.o
#endif

/* Bring in the old classad library because the user log parsing code
 * (which we form the condor api archive with) now can use xml logging and
 * classad representations of the events.
 * This brings in cedar, but we define stub functions in libcondorapi_stubs.cpp
 * to remedy the situation. If you're just reading log files, then you don't
 * need that functionality anyway. */
CLASSAD_OBJ = \
	../classad.old/ast.o \
	../classad.old/astbase.o \
	../classad.old/attrlist.o \
	../classad.old/buildtable.o \
	../classad.old/classad.o \
	../classad.old/environment.o \
	../classad.old/evaluateOperators.o \
	../classad.old/operators.o \
	../classad.old/parser.o \
	../classad.old/scanner.o \
	../classad.old/value.o \
	../classad.old/xml_classads.o \
	../classad.old/registration.o \
	../condor_util_lib/condor_snutils.o \
	stringSpace.o \
	string_list.o \
	MyString.o


/*
 * Construct a libcondorapi.a that includes enough stuff to read and write
 * log files. The libcondorapi_stubs.o object contains stubs for cedar, the
 * condor query object, and a few various other simple implementations for
 * functions we could not bring in from another place.
 */

CONLIBAPI_OBJ = \
	condor_event.o \
	file_sql.o \
	misc_utils.o \
	user_log_header.o \
	write_user_log.o \
	write_user_log_state.o \
	read_user_log.o \
	read_user_log_state.o \
	iso_dates.o \
	file_lock.o \
	format_time.o \
	utc_time.o \
	stat_wrapper.o \
	stat_wrapper_internal.o \
	../condor_util_lib/dprintf_common.o \
	../condor_util_lib/dprintf_config.o \
	../condor_util_lib/dprintf.o \
	../condor_util_lib/sig_install.o \
	../condor_util_lib/basename.o \
	../condor_util_lib/mkargv.o \
	../condor_util_lib/except.o \
	../condor_util_lib/strupr.o \
	../condor_util_lib/lock_file.NON_POSIX.o \
	../condor_util_lib/rotate_file.o \
	../condor_util_lib/string_funcs.o \
	strnewp.o \
	condor_environ.o \
	../condor_util_lib/setsyscalls.o \
	passwd_cache.o \
	uids.o \
	../condor_util_lib/chomp.o \
	subsystem_info.o \
	my_subsystem.o \
	distribution.o \
	my_distribution.o \
	$(CLASSAD_OBJ) \
	../condor_util_lib/get_random_num.o \
	\
	libcondorapi_stubs.o \
	$(SETEUID_OBJ) \
	$(SETEGID_OBJ) \
	condor_open.o \
	classad_merge.o \
	condor_attributes.o \
	simple_arg.o

DAEMON_CLIENT_OBJ = daemon.o daemon_types.o dc_shadow.o dc_startd.o \
	dc_schedd.o dc_collector.o dc_starter.o daemon_list.o \
	New_classads_dc_obj dc_message.o dc_master.o dc_transferd.o \
	dc_transfer_queue.o

OBJ = state_machine_driver.o event_handler.o name_tab.o selector.o \
	alarm.o user_job_policy.o classad_support.o \
	directory.o stat_info.o stat_wrapper.o stat_wrapper_internal.o \
	my_hostname.o file_lock.o sig_name.o \
	email_cpp.o env.o \
	string_list.o net_string_list.o condor_event.o $(URL_OBJ) \
	get_daemon_name.o \
	write_user_log.o \
	write_user_log_state.o \
	user_log_header.o \
	read_user_log.o read_user_log_state.o \
	config.o condor_config.o \
	stringSpace.o condor_state.o condor_adtypes.o \
	simple_arg.o \
	$(API_OBJ) $(DAEMON_CLIENT_OBJ) ccb_client.o \
	classad_hashtable.o classad_log.o log_transaction.o log.o \
	classad_collection.o usagemon.o print_wrapped_text.o \
	classad_helpers.o classad_merge.o classad_namedlist.o classadHistory.o \
	ClassAdReevaluator.o \
	linebuffer.o cron.o cronmgr.o cronjob.o cronjob_classad.o \
	distribution.o \
	debug_timer.o \
	condor_attributes.o condor_environ.o \
	get_full_hostname.o format_time.o renice_self.o condor_version.o \
	limit.o my_subsystem.o access.o subsystem_info.o \
	strnewp.o memory_file.o my_username.o misc_utils.o file_transfer.o \
	uids.o passwd_cache.o \
	metric_units.o condor_ver_info.o \
	condor_md.o \
	proc_id.o HashTable.o MyString.o \
	KeyCache.o dc_stub.o which.o iso_dates.o java_config.o \
	Regex.o my_distribution.o error_utils.o $(DH_OBJ) $(BASE64_OBJ) \
	translation_utils.o command_strings.o enum_utils.o \
	param_info.o param_info_hash.o \
	condor_parameters.o extra_param_info.o classad_command_util.o \
	domain_tools.o CondorError.o utc_time.o status_string.o \
	time_offset.o condor_crontab.o date_util.o condor_user_policy.o \
	$(JOB_QUEUE_LOG_PARSER_OBJ) \
	$(QUILL_OBJS) \
	New_classads_obj setenv.o condor_id.o build_job_env.o job_lease.o \
	gahp_common.o \
	condor_arglist.o store_cred.o \
	exponential_backoff.o \
	my_popen.o condor_open.o transfer_request.o condor_ftp.o \
	proc_family_interface.o proc_family_proxy.o proc_family_direct.o \
	procd_config.o \
	killfamily.o $(TT_OBJS) file_xml.o  vm_univ_utils.o condor_url.o \
	$(PLUGIN_OBJ) \
	forkwork.o filename_tools_cpp.o condor_getcwd.o \
	set_user_priv_from_ad.o hook_utils.o classad_visa.o \
	$(HIBERNATION_OBJS) \
	ConcurrencyLimitUtils.o \
	dc_service.o \
	condor_sinful.o \
	link.o \
	simple_arg.o \
	condor_threads.o \
	New_classads_newold \
	historyFileFinder.o \
	socket_proxy.o condor_timeslice.o

library_target(cplus_lib.a,$(OBJ))
library_target(liburl.a,$(URL_OBJ))

library_target(libcondorapi.a,$(CONLIBAPI_OBJ))
libcondorapi.a: Makefile

#if HAVE_CC_SHARED_FLAG
shared_library_target(libcondorapi.so,$(CONLIBAPI_OBJ))
libcondorapi.so: Makefile
#endif

all:: JobLogMirror.o

c_plus_target(test_mapfile,test_mapfile.o,$(LIB))
c_plus_target(test_locks,test_locks.o,$(LIB))
c_plus_target(test_iso_dates,test_iso_dates.o,$(LIB))
c_plus_target(test_user_job_policy,test_user_job_policy.o,$(LIB))
c_plus_target(test_classad_support,test_classad_support.o,$(LIB))
c_plus_target(test_classad_merge,test_classad_merge.o,$(LIB))
c_plus_target(test_mystring,test_mystring.o,$(LIB))
c_plus_target(test_condor_arglist,test_condor_arglist.o,$(LIB))
c_plus_target(test_condor_crontab,test_condor_crontab.o,$(LIB))
c_plus_target(test_spawn,test_spawn.o,$(LIB))
c_plus_target(test_tree,test_tree.o,$(LIB))
c_plus_target(test_ClassAdReevaluator,ClassAdReevaluatorTest.o,$(LIB))


/* this needs to be built more like a user would use the library, so it */
/* can't have all of our usual cflags... */
test_libcondorapi.o: test_libcondorapi.cpp
	$(CPlusPlus) -I../h -I../condor_includes -c test_libcondorapi.cpp

c_plus_nowrap_target(test_libcondorapi,test_libcondorapi.o,libcondorapi.a)

#if HAVE_CC_SHARED_FLAG
c_plus_nowrap_target(test_libcondorapi_shared,test_libcondorapi.o,libcondorapi.so)
#endif

c_plus_target(test_check_events,test_check_events.o,$(LIB) New_classads_lib)
c_plus_target(test_multi_log,test_multi_log.o,$(LIB) New_classads_lib)
c_plus_target(test_stat_wrapper,test_stat_wrapper.o,$(LIB))
c_plus_target(test_write_term,test_write_term.o,$(LIB))
c_plus_target(test_prioritysimplelist,test_prioritysimplelist.o,)
c_plus_target(test_binarysearch,test_binarysearch.o,)
c_plus_target(test_network_adapter,test_network_adapter.o,$(LIB))
c_plus_target(test_hibernation,test_hibernation.o,$(LIB))


c_plus_target(cat_url,cat_url.o,liburl.a)
c_plus_target(access.t,access.t.o,$(LIB))
c_plus_target(Queue.t,Queue.t.o,$(LIB))
c_plus_target(killfamily.t,killfamily.t.o,$(LIB))



c_plus_target(my_hostname.t,my_hostname.t.o,$(LIB))



html_target( documentation )

all-tests: access.t killfamily.t test_locks my_hostname.t

/* User log testing */
USERLOG_TEST_NAMES = reader reader_state writer
USERLOG_TESTS = $(addprefix test_log_, $(USERLOG_TEST_NAMES) ) 
USERLOG_LIBAPI_TESTS = $(addsuffix _api, $(USERLOG_TESTS) )

userlog-tests: all $(USERLOG_TESTS) $(USERLOG_LIBAPI_TESTS)

/* Build the log reader test with ENABLE_STATE_DUMP turned on */
c_plus_target(test_log_reader,test_log_reader.o,$(LIB))
c_plus_target(test_log_reader_state,test_log_reader_state.o,$(LIB))
c_plus_target(test_log_writer,test_log_writer.o,$(LIB))
test_log_reader.o : test_log_reader.cpp
	$(CPlusPlus) $(C_PLUS_FLAGS) -DENABLE_STATE_DUMP -c $< -o $@

/* This seems to confuse the gmake on AIX, and I really only need
   the extra dependancies for my interactive builds on Linux */
#if IS_LINUX
test_log_reader: all $(LIB) Makefile $(LIBCONDORAPI)
test_log_reader_state: all $(LIB) Makefile $(LIBCONDORAPI)
test_log_writer: all $(LIB) Makefile $(LIBCONDORAPI)
#endif

testbin_target(test_log_reader,755) 
testbin_target(test_log_reader_state,755) 
testbin_target(test_log_writer,755) 

green-tests: all test_network_adapter test_hibernation

%_api : %.o $(LIBCONDORAPI) Makefile
	$(CPlusPlus) $< -L. -Wl,-Bstatic -lcondorapi -Wl,-Bdynamic -ldl -o $@


IMPORT_LINKS = ../../config/import_links
import_objs(../condor_util_lib,mkargv.o)
import_objs(../condor_daemon_client,$(DAEMON_CLIENT_OBJ))
import_objs(../ccb,ccb_client.o)

public_library(libcondorapi.a,lib)
libcondorapi-release:: $(RELEASE_DIR)/lib/libcondorapi.a

#if HAVE_CC_SHARED_FLAG
public_shared_library(libcondorapi.so,lib)
libcondorapi-release:: $(RELEASE_DIR)/lib/libcondorapi.so
#endif

public_copy_target(user_log.README,include,user_log.README,TEXT_MODE)
libcondorapi-release:: $(RELEASE_DIR)/include/user_log.README

public_copy_target(user_log.c++.h,include,user_log.c++.h,TEXT_MODE)
libcondorapi-release:: $(RELEASE_DIR)/include/user_log.c++.h

public_copy_target(condor_event.h,include,condor_event.h,TEXT_MODE)
libcondorapi-release:: $(RELEASE_DIR)/include/condor_event.h

public_copy_target(../condor_includes/file_lock.h,include,file_lock.h,TEXT_MODE)
libcondorapi-release:: $(RELEASE_DIR)/include/condor_event.h

public_copy_target(condor_holdcodes.h,include,condor_holdcodes.h,TEXT_MODE)
public_copy_target(../condor_includes/condor_constants.h,include,condor_constants.h,TEXT_MODE)

public_copy_target(read_user_log.h,include,read_user_log.h,TEXT_MODE)
libcondorapi-release:: $(RELEASE_DIR)/include/read_user_log.h

public_copy_target(write_user_log.h,include,write_user_log.h,TEXT_MODE)
libcondorapi-release:: $(RELEASE_DIR)/include/write_user_log.h

public_copy_target(iso_dates.h,include,iso_dates.h,TEXT_MODE)
libcondorapi-release:: $(RELEASE_DIR)/include/iso_dates.h

public_copy_target(../condor_includes/condor_classad.h,include,condor_classad.h,TEXT_MODE)
libcondorapi-release:: $(RELEASE_DIR)/include/condor_classad.h

public_copy_target(../condor_includes/condor_exprtype.h,include,condor_exprtype.h,TEXT_MODE)
libcondorapi-release:: $(RELEASE_DIR)/include/condor_exprtype.h

public_copy_target(../condor_includes/condor_astbase.h,include,condor_astbase.h,TEXT_MODE)
libcondorapi-release:: $(RELEASE_DIR)/include/condor_astbase.h

public_copy_target(../condor_includes/condor_ast.h,include,condor_ast.h,TEXT_MODE)
libcondorapi-release:: $(RELEASE_DIR)/include/condor_ast.h

public_copy_target(../condor_includes/condor_attrlist.h,include,condor_attrlist.h,TEXT_MODE)
libcondorapi-release:: $(RELEASE_DIR)/include/condor_attrlist.h

public_copy_target(../condor_includes/condor_parser.h,include,condor_parser.h,TEXT_MODE)
libcondorapi-release:: $(RELEASE_DIR)/include/condor_parser.h

public_copy_target(MyString.h,include,MyString.h,TEXT_MODE)
libcondorapi-release:: $(RELEASE_DIR)/include/MyString.h

public_copy_target(../condor_includes/condor_header_features.h,include,condor_header_features.h,TEXT_MODE)
libcondorapi-release:: $(RELEASE_DIR)/include/condor_header_features.h

condor_version.o: condor_version.cpp
	$(CPlusPlus) $(C_PLUS_FLAGS) -c -DPLATFORM=$(PLATFORM) condor_version.cpp

testbin:: all

help:
	@echo "Known targets:"
	@echo "  all:           build all main targets"
	@echo "  release:       all + populate ../release_dir"
	@echo "  testbin:"
	@echo "  userlog-tests: all + userlog tests"
	@echo "  green-tests:   all + green computing tests"
