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

MACRO (ADD_DEPENDENCIES_SUFFIX_HACK _SOURCE _TARGET)
# Many, but not all of the tests added with CondorExeTest have a target name that
# ends in exe.  We want all of the generated executable for these to end in exe,
# both on Unix and Windows.  To do this, the CondorExeTest macro removes the .exe
# suffix of the target name (not the generated executable, but the target) only
# on Windows, so CMake will tack on the exe suffix to the generated target, and we
# won't end up with double .exe suffixes.  This creates problems when we want
# to add additional dependencies on those targets, because the name is different
# on each platform.

if (WINDOWS)
	string(REPLACE ".exe" "" _TARGET_WITHOUT_EXE ${_TARGET})
	add_dependencies(${_SOURCE} ${_TARGET_WITHOUT_EXE})
else(WINDOWS)
	add_dependencies(${_SOURCE} ${_TARGET})
endif (WINDOWS)

ENDMACRO(ADD_DEPENDENCIES_SUFFIX_HACK)
############################################################################
if (BUILD_TESTING)

	##########################################################
	# vars and settings
	set(CONDOR_SCRIPTS_DIR ${CONDOR_SOURCE_DIR}/src/condor_scripts)
	include_directories(${CONDOR_SOURCE_DIR}/src/condor_chirp)

	##########################################################
	## begin file goop manipulation specific to the horror that
	## is the "condor testing framework."
	foreach(scriptFile Condor.pm
			   CondorTest.pm
			   CondorPersonal.pm
			   CondorUtils.pm
			   run_test.pl
			   condor_credmon_oauth_dummy)
		configure_file(${CONDOR_SCRIPTS_DIR}/${scriptFile}
				${TEST_TARGET_DIR}/${scriptFile} COPYONLY)
	endforeach(scriptFile)


	set(CHECK_MODULES CondorLog.pm
			JobDoesNotRun.pm
			JobIsEvicted.pm
			ParseDryRunData.pm
			ProcInfo.pm
			SimpleJob.pm)

	if (NOT "${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
	foreach(scriptFile ${CHECK_MODULES})
		add_custom_command ( OUTPUT ${TEST_TARGET_DIR}/Check/${scriptFile}
			DEPENDS ${CONDOR_SOURCE_DIR}/src/condor_tests/Check/${scriptFile}
			COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CONDOR_SOURCE_DIR}/src/condor_tests/Check/${scriptFile} ${TEST_TARGET_DIR}/Check/${scriptFile})
		add_custom_target(test_copy_${scriptFile} ALL DEPENDS ${TEST_TARGET_DIR}/Check/${scriptFile})
	endforeach()

	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/CheckOutputFormats.pm ${TEST_TARGET_DIR}/CheckOutputFormats.pm COPYONLY)

	endif()

	# 1st remove generated files.
	file( GLOB RMV_FIRST ${TEST_TARGET_DIR}/list_* *.desc )
	if (RMV_FIRST)
		file( REMOVE ${RMV_FIRST} )
	endif()

	APPEND_VAR(CONDOR_TEST_HELPER_SCRIPTS "Condor.pm;CondorTest.pm;CondorPersonal.pm;CondorUtils.pm;run_test.pl;condor_credmon_oauth_dummy")

	##########################################################
	# exe test targets
	condor_exe_test(lib_chirpio.exe "lib_chirpio.cpp" "chirp_client")
	condor_exe_test(job_core_chirp_par_executable.exe "job_core_chirp_par.cpp" "chirp_client")
	condor_exe_test(x_read_joblog.exe "x_read_joblog.cpp" condor_utils)
	condor_exe_test(x_write_joblog.exe "x_write_joblog.cpp" condor_utils)
	condor_exe_test(x_write_joblog_events.exe "x_write_joblog_events.cpp" condor_utils)
	condor_exe_test(lib_eventlog_base_executable.exe "lib_eventlog_base.cpp" condor_utils)
	condor_exe_test(job_core_bigenv.exe "job_core_bigenv.c" "")
	if(NOT WINDOWS)
		condor_exe_test(test_for_AwaitableDeadlineReaper.exe "test_for_AwaitableDeadlineReaper.cpp" condor_utils)
	endif(NOT WINDOWS)
	condor_exe_test(x_job_mem_checker.exe "x_job_mem_checker.c" "" )
	condor_exe_test(x_complete_params.exe "x_complete_params.cpp" "" )
	if( WINDOWS )
		add_dependencies(x_complete_params utils_genparams)
	else( WINDOWS )
		add_dependencies(x_complete_params.exe utils_genparams)
	endif( WINDOWS)
	condor_exe_test(x_conditional_params.exe "x_conditional_params.cpp" "${CONDOR_TOOL_LIBS};${CONDOR_WIN_LIBS}" )

	# Not all of our gccs support -Wno-div-by-zero.
	#if (UNIX)
	#	set_source_files_properties(x_dumpcore.cpp PROPERTIES COMPILE_FLAGS -Wno-div-by-zero)
	#endif(UNIX)

	if(WINDOWS)
		# can't build this using cmake because it insists on passing wrong compiler flags, the good thing is that we don't need to build it anyway.
		# condor_exe_test(sleep.exe "sleep.cpp" "")
	endif(WINDOWS)

	if (NOT WINDOWS)
	  condor_exe_test(x_trapsig.exe "x_trapsig.cpp" "" )
	endif(NOT WINDOWS)

	##########################################################
	# platform specific tests
	if(LINUX)

		condor_pl_test( job_ec2_basic "EC2 Query API test" "quick;ctest" CTEST DEPENDS "src/condor_tests/job_ec2_basic.accessKey;src/condor_tests/job_ec2_basic.secretKey;src/condor_tests/job_ec2_basic.userDataFile;${CMAKE_BINARY_DIR}/src/condor_tests/queryAPI-sim")
		add_dependencies(job_ec2_basic queryAPI-sim)
		condor_pl_test( job_hyperthread_check "hyper thread testing test" "quick;ctest" CTEST DEPENDS src/condor_tests/count_cpus)

		condor_pl_test(lib_procapi_pidtracking-byenv "Slow Termination Child Cleanup Test" "quick;ctest" CTEST DEPENDS "src/condor_tests/lib_procapi_pidtracking-byenv.cmd;src/condor_tests/x_pid_tracking.pl")
		#condor_pl_test(job_core_shadow-lessthan-memlimit_van "Make sure the shadow stays below memory limit" "quick;ctest")
	endif()

	# negative matches. test on everything but this
	if(NOT WINDOWS)
		condor_pl_test(unit_test_sinful "unit: Sinful" "quick;ctest" CTEST DEPENDS "src/condor_tests/NetworkTestConfigs.pm;${CMAKE_BINARY_DIR}/src/condor_tests/test_sinful")
		add_dependencies(unit_test_sinful test_sinful)
		condor_pl_test(job_core_killsignal_sched "Scheduler: Verify the specified input file is used" "quick;ctest" CTEST DEPENDS "src/condor_tests/job_core_killsignal_sched.cmd;${CMAKE_BINARY_DIR}/src/condor_tests/x_trapsig.exe")
		add_dependencies(job_core_killsignal_sched x_trapsig.exe)
		condor_pl_test(job_core_rmkillsig_sched "Scheduler: Verify the  remove_kill_sig" "quick;ctest" CTEST DEPENDS "src/condor_tests/job_core_rmkillsig_sched.cmd;${CMAKE_BINARY_DIR}/src/condor_tests/x_trapsig.exe")
		add_dependencies(job_core_rmkillsig_sched x_trapsig.exe)
	endif()

	condor_pl_test( protocol_matching "test: Protocol matching" "quick;ctest" CTEST DEPENDS ${CMAKE_BINARY_DIR}/src/condor_tests/test_protocol_matching)
	add_dependencies(protocol_matching test_protocol_matching)

	condor_pl_test(cmd_condor_off-master "vanilla: condor_on condor_off test" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(job_test_scheddrotation "Scheduler: basic log rotation test" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(job_test_logrotation "basic log rotation test" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
if (NOT WINDOWS)
	condor_pl_test(fetch_work_static_slot "work fetch static smoke test" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_fetchwork_scripts.tar.gz")
	condor_pl_test(fetch_work_pslot "work fetch pslot smoke test" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_fetchwork_scripts.tar.gz")
endif ()

	condor_pl_test(lib_auth_protocol-ssl "SSL authentication test" "quick;ctest" CTEST DEPENDS "src/condor_tests/lib_auth_protocol-ssl.cmd;src/condor_tests/x_sleep.pl")
	#condor_pl_test(lib_auth_protocol-pw "PW authentication test" "quick;ctest")
	#condor_pl_test(job_dagman_node_status_rm "Check status file shows condor_rm" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_node_status_rm.dag;src/condor_tests/job_dagman_node_status_rm_node.pl;src/condor_tests/job_dagman_node_status_rm_node.cmd;src/condor_tests/job_dagman_node_status_rm_node.template")
	condor_pl_test(job_dagman_large_dag "Test large DAG w/ recovery mode" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_large_dag-node.pl;src/condor_tests/create_large_dag.pl")



	# The shared port tests are failing intermittantly on mac.  Rather than spending the time to fix
	# the tests, we are commenting them out for now.  Symptom is that some daemons in the same
	# personal condor cannot use the family session, and apparently present the wrong session
	# to the server side.
if (NOT APPLE)
	condor_pl_test(lib_shared_port_van "Test of shared port feature." "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(lib_shared_port-collector_van "Test of shared port feature with collector usage." "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(lib_shared_port-check-ports_van "Test of shared port feature with respect to open ports." "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
endif ()

	condor_pl_test (cmd_submit_coverage "Test condor_submit file entries" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test (cmd_submit_regress_dry "Regression test for condor_submit keywords" "quick;ctest" CTEST DEPENDS "src/condor_tests/cmd_submit_regress_dry.expect;src/condor_tests/cmd_submit_regress_dry.subs")
	condor_pl_test (cmd_submit_factory_regress_dry "Regression test for condor_submit keywords with late materialization" "quick;ctest" CTEST DEPENDS "src/condor_tests/cmd_submit_regress_dry.expect;src/condor_tests/cmd_submit_regress_dry.subs;${CMAKE_BINARY_DIR}/src/condor_tests/test_job_factory.exe")
	add_dependencies_suffix_hack(cmd_submit_factory_regress_dry test_job_factory.exe)
	condor_pl_test (cmd_submit_massive "Test condor_submit file massive" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	#condor_pl_test (job_test_urlfetch "Test condor_urlfetch" "quick;ctest")
	condor_pl_test (lib_meta_knob  "Test meta knobs" "quick;ctest" CTEST)
	condor_pl_test (lib_include_cmd_config  "Test meta knobs" "quick;ctest" CTEST DEPENDS "src/condor_tests/lib_include_cmd_config1.txt;src/condor_tests/lib_include_cmd_config3.txt;src/condor_tests/lib_include_cmd_config.pl;src/condor_tests/lib_include_cmd_config2.txt;src/condor_tests/lib_include_cmd_config4.txt")


	# an example of a test writen in python rather than in perl
	# condor_pl_test (python_example "Sample python test" "quick;ctest")
	if (WITH_PYTHON_BINDINGS)
		# An example test that depends on python bindings.
		# condor_pl_test (python_bindings_example "Sample python bindings test" "quick;ctest")

		# htcondor.dags test suite
		condor_pl_test (test_htcondor_dags "htcondor-dags test suite" "quick;ctest" CTEST DEPENDS "src/condor_tests/test_htcondor_dags")

		# htcondor.personal test suite
		if (NOT WINDOWS)
			condor_pl_test (test_htcondor_personal "htcondor-personal test suite" "quick;ctest" CTEST DEPENDS "src/condor_tests/test_htcondor_personal")
		endif()

		# ClassAds test suite
		condor_pl_test (test_classad_version_comparison "Test ClassAd version comparison functions" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")

		if (LINUX OR APPLE)
			# Fail.  We can't build this on Windows because of bad target names.
			# At some point, this should be turned into a unit test.
			condor_exe_test(test_filter.exe "test_filter.cpp;../condor_starter.V6.1/filter.cpp" "condor_utils")
			# Fail.  ctest -R always runs PyTest with an Ornithology-only argument,
			# which probably means we can't run this test without the bindings,
			# although it doesn't use them.
			condor_pl_test(test_filter "Test plug-in output ad filtering" "quick;ctest" CTEST DEPENDS "${CMAKE_BINARY_DIR}/src/condor_tests/test_filter.exe;src/condor_tests/ornithology;src/condor_tests/conftest.py;${CMAKE_BINARY_DIR}/src/condor_tests/test_filter.exe")
			add_dependencies_suffix_hack(test_filter test_filter.exe)

			# test_classad_eval doesn't actually depend on ornithology at all,
			# but run_test.pl doesn't know that, and always runs pytest with an
			# ornithology-specific command-line flag
			condor_pl_test(test_classad_eval "Test classad_eval" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			condor_pl_test(test_htcondor_annex_create_constraints "Test annex create constraints" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			# Note that ctest can NOT find this test if it's named
			# test_AwaitableDeadlineReaper.  I can't even.
			condor_pl_test(test_awaitable_deadline_reaper "Test annex create constraints" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py;${CMAKE_BINARY_DIR}/src/condor_tests/test_for_AwaitableDeadlineReaper.exe")
			add_dependencies_suffix_hack(test_awaitable_deadline_reaper test_for_AwaitableDeadlineReaper.exe)
		endif()

		# Ornithology - partial merge - Windows and MacOSX problems
		if(LINUX OR APPLE)
			condor_pl_test(test_run_sleep_job "Run a sleep job to completion" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			condor_pl_test(test_num_holds "Test Hold Aggregates" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			condor_pl_test(test_shadow_hook "Test shadow job hooks and their status msg" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			condor_pl_test(test_hook_status_msg "Test Starter Job Hook Status Msg" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			condor_pl_test(test_xfer_hold_codes "Test proper hold codes after xfer failure" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			condor_pl_test(test_job_env "Test Job Environment" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			condor_pl_test(test_bogus_collector "Test Bogus Collector" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			# condor_pl_test(test_hold_and_release "Submit a job, hold it, release it, run it completion" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			condor_pl_test(test_late_materialization "Test that late materialization options work correctly with each other" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			condor_pl_test(test_late_materialization_bindings "Test that late materialization works correctly via the bindings" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			condor_pl_test(test_lm_more "More-thorough testing of late materialization via the bindings" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			condor_pl_test(test_concurrency_limits "Test that concurrency limits are obeyed" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			condor_pl_test(test_condor_now "Test that condow_now works and never leaks memory" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			condor_pl_test(test_condor_run "Test condor_run outputs submission warnings" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			condor_pl_test(test_condor_now_internals "Test condow_now internals" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			condor_pl_test(test_drain_policies "Test job policy and backfill/draining interactions" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			condor_pl_test(test_dagman_inline_submit "Test the DAGMan inline submit description feature" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			condor_pl_test(test_dagman_submit_description "Test the DAGMan SUBMIT-DESCRIPTION command" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			condor_pl_test(test_dagman_service_nodes "Test the DAGMan service nodes" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			condor_pl_test(test_scheduler_priority "Test that job priority is respected in scheduler universe" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			condor_pl_test(test_parallel_uni "Test parallel uni" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			condor_pl_test(test_negotiator_slot_const "Test NEGOTIATOR_SLOT_CONSTRAINT" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			condor_pl_test(test_io_ft_error_reporting "Verify that input failures are reported if a failure output transfer fails" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")

			condor_pl_test(test_python_bindings_classad "Test that the Python classad bindings behave correctly" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			condor_pl_test(test_python_bindings_version "Test that the Python htcondor bindings have correct version info" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			condor_pl_test(test_python_bindings_collector "Test that the Python htcondor.Collector bindings behave correctly" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			condor_pl_test(test_python_bindings_dagman "Test DAGMan submission from the Python bindings" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			condor_pl_test(test_python_bindings_jobeventlog "tests the JobEvent class" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py;${CMAKE_BINARY_DIR}/src/condor_tests/x_write_joblog.exe")
			add_dependencies_suffix_hack(test_python_bindings_jobeventlog x_write_joblog.exe)

			condor_pl_test(test_auth_protocol_token "Test TOKEN authentication" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")

			condor_pl_test(test_htcondor_cli "Test the htcondor CLI tool" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			condor_pl_test(test_toe_exit_info "Test ToE exit info" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			condor_pl_test(test_json_toe_when "Test JSON ToE's when" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			condor_pl_test(test_allowed_job_duration "test allowed_job_duration" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			condor_pl_test(test_jobsets "Test jobsets" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")

			condor_pl_test(test_htcondor_submit_constructor "Test htcondor.Submit()" "quick;ctest"	CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			condor_pl_test(test_job_submit_method_recording "Test recording of JobSubmitMethod job attribute" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			condor_pl_test(test_condor_history_reads_epochs "Test condor_history ability to read epoch files" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")	
			condor_pl_test(test_container_img_declares_universe "Test declaring container image sets up universe" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			condor_pl_test(test_multifile_curl_plugin_timeout "Test multifile curl plugin correctly does timeout" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			condor_pl_test(test_dagman_futile_nodes "Test DAGMan accurately sets futile nodes" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			condor_pl_test(test_condor_history "Test condor_history tools capabilities" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			condor_pl_test(test_dagman_proper_env "Test ability to set DAGMan proper job environment" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			condor_pl_test(test_dagman_save_files "Test ability for DAGMan to write and load save point files" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			condor_pl_test(test_dagman_futile_nodes_efficiency "Test DAGMan is not inefficiently setting nodes to futile" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			condor_pl_test(test_dagman_check_q_and_exit "Test DAGMan verify running jobs mechanism" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			condor_pl_test(test_submit_requirements "Test submit requirements" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			condor_pl_test(test_dagman_utils_parse_crash "Test DAGMan utils doesn't segfault" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			condor_pl_test(test_protected_url_xfers "test attribute setting for mapped AP protected URL transfers" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			condor_pl_test(test_chirp_to_dagman_log "Test chirp events get written to DAGMan log" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			condor_pl_test(test_dagman_script_debug_file "Test DAGMan ability to capture script standard output streams" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			condor_pl_test(test_startd_cron "Test startd cron one shots write to start log" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			condor_pl_test(test_dagman_script_macros "Test DAGMan script macros" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			condor_pl_test(test_dagman_inherit_attrs "Test DAGMan attribute inheritance" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			condor_pl_test(test_htcondor_cli_sys_status "Test HTCondor CLI system nouns status" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			condor_pl_test(test_dagman_inline_shell_submit "Test DAGMan inline description with shell submit" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")

			# These tests require Python 3.6 or later.
			if (PYTHON3_VERSION_MINOR GREATER_EQUAL 6)
				condor_pl_test(test_custom_machine_resource "Test that a custom machine resource is assigned, limited, and monitored correctly" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py;src/condor_tests/libcmr.py")
				condor_pl_test(test_custom_machine_resources "Test that multiple custom machine resources are assigned, limited, and monitored correctly" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py;src/condor_tests/libcmr.py")
				condor_pl_test(test_custom_machine_resource_instances "Test that multiple instances of custom machine resources are assigned, limited, and monitored correctly" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py;src/condor_tests/libcmr.py")
				condor_pl_test(test_curl_plugin "Test the curl file transfer plugin" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
				condor_pl_test(test_allowed_execute_duration "Test allowed_execute_duration implementation" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
				condor_pl_test(test_htcondor_736 "Test bugs fixed in HTCONDOR-736" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
				condor_pl_test(test_htcondor_809 "Additional file-transfer tests" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
				condor_pl_test(test_activation_metrics "Test activation metrics" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
				# HTCONDOR-1365 changed the behavior of output_destination so
				# that it no longer sends standard output and error to the
				# destination.  HTCONDOR-955 made output_destination respect
				# the `output` and `error` submit file commands' remaps, so
				# it's no longer relevant.  Leaving the test in source for
				# now, pending a resolution of backwards-compability issues.
				# condor_pl_test(test_htcondor_955 "Test output_destination with standard stream names" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
				condor_pl_test(test_htcondor_993 "Test lots of directories with preserve_relative_paths" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
				condor_pl_test(test_xfer_stats_overflow_regression "Test TransferInputStats nested attributes don't overflow" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
				condor_pl_test(test_condor_token_fetch_key "Test `condor_token_fetch -key`" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
				# Sadly, Python doesn't implement a way to get disk space on Windows.
				condor_pl_test(test_reserve_disk "Test RESERVED_DISK underflow" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
				condor_pl_test(test_hpc_annex_require_job "Test HPC_ANNEX_REQUIRE_JOB" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
				condor_pl_test(test_failure_files "Test failure files" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")

				condor_pl_test(test_dataflow_job "Test dataflow jobs (simple)" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
				condor_pl_test(test_dataflow_job_more "Test dataflow jobs (stupid)" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")

				condor_pl_test(test_dont_queue_small_sandboxes "Test not using transfer queue for small transfers" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
				condor_pl_test(test_epoch_attrs "Test our work-around for ElasticSearch not doing joins" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")

				condor_pl_test(test_success_exit_code_xfer "Make sure we still get logs if success_exit_code is set" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")

				if(NOT EMULATED_PLATFORM)
					# The emulated platforms are too slow to run these tests.
					condor_pl_test(test_checkpoint_destination "Test checkpoint_destination" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
					condor_pl_test(test_checkpoint_destination_two "Test checkpoint_destination_two" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
					condor_pl_test(test_checkpoint_preen "Test checkpoint_destination cleanup via preen" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
					condor_pl_test(test_checkpoint_ct "Test checkpoint CommittedTime" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
					condor_pl_test(test_tom_directory_creation "Test transfer_output_remaps creating directories" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
				endif(NOT EMULATED_PLATFORM)

				condor_pl_test(test_default_checkpoint_destination "Test use feature:DefaultCheckpointDestination" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
				condor_pl_test(test_unspecified_checkpoint "Test self-checkpointing jobs which specify their checkpoint" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")

				condor_pl_test(test_plugin_timeout "Test plugin timeout" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
				condor_pl_test(test_plugins_dont_see_directories "Plug-ins shouldn't see directories" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")

				condor_pl_test(test_ft_plugin_testing "Test file transfer plugin testing mode" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
				condor_pl_test(test_ft_plugin_stuff "Test some file-transfer plug-in things" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
				condor_pl_test(test_pcre "Test some PCRE mapfile nonsense" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")

				condor_pl_test(test_submit_itemdata_qargs "Test htcondor2.Submit.itemdata(qargs=qargs)" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			endif()
		endif()

		# These Ornithology tests only work on linux.
		# That's expected for some. Others need investigation
		# to see if we can get them working on other platforms.
		if(LINUX)
			# We don't see the "invalid interpreter" error
			# message on macos.
			condor_pl_test(test_broken_hash_bang "Test Broken Hash Bang" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			# Container universe is only available on linux.
			condor_pl_test(test_container_uni "Test container uni" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py") 
			if( NOT EMULATED_PLATFORM )
				condor_pl_test(test_singularity_sif "Test container uni with sif file" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
			endif( NOT EMULATED_PLATFORM )
			# The manifest code should work on macos, but
			# the tests are failing.
			condor_pl_test(test_manifest "Test manifest functionality" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py") 
			# These tests require Python 3.6 or later.
			if (PYTHON3_VERSION_MINOR GREATER_EQUAL 6)
				condor_pl_test(test_condor_manifest "Test MANIFEST implementation via condor_manifest tool" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py") 
				condor_pl_test(test_job_token_transfer "Test secure token transfer from shadow to starter" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py;src/condor_scripts/condor_credmon_oauth_dummy")
			endif()

			# This test requires the Python scitokens library, which
			# has unresolvable depedencies on CentOS 7.
			if (NOT ${SYSTEM_NAME} MATCHES "centos7")
				if (NOT ${SYSTEM_NAME} MATCHES "Ubuntu.*20")
					condor_pl_test(test_issue_credentials "Test issue_credentials()" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
					condor_pl_test(test_dagman_produce_creds "Test DAGMan creates job credentials" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
					condor_pl_test(test_htcondor_credential "Test the htcondor credential" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")
				endif()
			endif()

			# The AES test depends on strace'ing the condor_shadow binary.  Strace isn't supported under 
			# qemu, which is where we run our ppc and arm tests. 
			# And strace is only available on linux.
			if( NOT EMULATED_PLATFORM )
				condor_pl_test(test_aes_file_transfer "Test AES encrypted file transfer" "quick;ctest" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py") 
			endif( NOT EMULATED_PLATFORM )

		endif()
	endif()

	if(WITH_GANGLIA)
		condor_pl_test(gangliad "Basic test of condor_gangliad." "quick;ctest" CTEST DEPENDS "src/gangliad/ganglia_default_metrics")
	endif()

	condor_pl_test(lib_procapi_pidtracking-snapshot "Fast Termination Child Cleanup Test" "quick;ctest" CTEST DEPENDS "src/condor_tests/lib_procapi_pidtracking-snapshot.cmd;src/condor_tests/x_pid_tracking.pl")

	condor_pl_test(classad_cache-basic "classad-cache smoke test" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	# test gives false positives on darwin and windows
	if (LINUX)
		# The shadow memory test fails under qemu, as qemu itself eats a lot of memory.
		if(NOT EMULATED_PLATFORM)
			condor_pl_test(shadow_mem_usage-basic "report shadow memory use" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_shadow_mem_usage.pl")
			if (${BIT_MODE} MATCHES "32")
				condor_pl_test(shadow_s_mem_usage-basic "report small shadow memory use" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_shadow_mem_usage.pl")
			endif()
		endif(NOT EMULATED_PLATFORM)
	endif(LINUX)

	##########################################################
	# now all the common tests.

	##### generic lib tests
	condor_pl_test(lib_unit_tests "Run condor_utils Unit Tests" "quick;ctest" CTEST DEPENDS "${CMAKE_BINARY_DIR}/src/condor_tests/condor_unit_tests;src/condor_tests/README")
	add_dependencies(lib_unit_tests condor_unit_tests)
	condor_pl_test(classad_unit_test "Run classads Unit Tests" "quick;ctest" CTEST DEPENDS "${CMAKE_BINARY_DIR}/src/condor_tests/classad_unit_tester")
	add_dependencies(classad_unit_test classad_unit_tester)
	condor_pl_test(classad_cache_unit_test "Run classads parse test with and without classad caching" "quick;ctest" CTEST DEPENDS "${CMAKE_BINARY_DIR}/src/condor_tests/_test_classad_parse")
	add_dependencies(classad_unit_test _test_classad_parse)
	condor_pl_test(unit_test_async_fread "Run MyAsyncFileReader Unit Tests" "quick;ctest" CTEST DEPENDS "${CMAKE_BINARY_DIR}/src/condor_tests/async_freader_tests")
	add_dependencies(unit_test_async_fread async_freader_tests)
	#need to copy the underlying exe into condor_tests directory before these tests can be run
	#condor_pl_test(consumption_policy_unit_test "Run Consumption policy unit tests" "quick;ctest")
	#condor_pl_test(ring_buffer_unit_test "Run ring buffer unit tests" "quick;ctest")
	# Not in use till endurance testing done.
	# condor_pl_test(lib_sysapi "Run SysApi Tests" "quick;ctest")
	# condor_pl_test(lib_procapi "Run ProcApi Tests" "quick;ctest")

	##### dagman tests
	condor_pl_test(job_core_chirp_par "Exercise the chirp I/O C library in Parallel Universe" "lib;quick;ctest" CTEST DEPENDS "src/condor_tests/job_core_chirp_par.template;${CMAKE_BINARY_DIR}/src/condor_tests/job_core_chirp_par_executable.exe;src/condor_tests/x_chirpio_mkdata.pl")
	add_dependencies_suffix_hack(job_core_chirp_par job_core_chirp_par_executable.exe)
	condor_pl_test(job_dagman_splice-scaling "Dagman Splice Parse Scaling" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_splice-scaling-splice.cmd;src/condor_tests/job_dagman_splice-scaling-splice.dag")
	condor_pl_test(job_dagman_splice-A "Simple Dagman Splice A" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_splice-A.cmd;src/condor_tests/job_dagman_splice-A.dag")
	condor_pl_test(job_dagman_splice-B "Simple Dagman Splice B" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_splice-B.cmd;src/condor_tests/job_dagman_splice-B-splice2.dag;src/condor_tests/job_dagman_splice-B.dag;src/condor_tests/job_dagman_splice-B-splice1.dag;src/condor_tests/job_dagman_splice-B-splice3.dag")
	condor_pl_test(job_dagman_splice-C "Simple Dagman Splice C" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_splice-C.cmd;src/condor_tests/job_dagman_splice-C.dag;src/condor_tests/job_dagman_splice-C-splice1.dag")
	condor_pl_test(job_dagman_splice-D "Simple Dagman Splice D" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_splice-D.cmd;src/condor_tests/job_dagman_splice-D.dag;src/condor_tests/job_dagman_splice-D-splice1.dag")
	condor_pl_test(job_dagman_splice-E "Simple Dagman Splice E" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_splice-E.cmd;src/condor_tests/job_dagman_splice-E.dag;src/condor_tests/job_dagman_splice-E-splice1.dag")
	condor_pl_test(job_dagman_splice-F "Simple Dagman Splice F" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_splice-F.cmd;src/condor_tests/job_dagman_splice-F.dag;src/condor_tests/job_dagman_splice-F-splice1.dag")
	condor_pl_test(job_dagman_splice-G "Simple Dagman Splice G" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_splice-G.cmd;src/condor_tests/job_dagman_splice-G.dag;src/condor_tests/job_dagman_splice-G-splice1.dag")
	condor_pl_test(job_dagman_splice-H "Simple Dagman Splice H" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_splice-H.cmd;src/condor_tests/job_dagman_splice-H-splice2.dag;src/condor_tests/job_dagman_splice-H.dag;src/condor_tests/job_dagman_splice-H-splice1.dag;src/condor_tests/job_dagman_splice-H-splice3.dag")
	condor_pl_test(job_dagman_splice-I "Simple Dagman Splice I" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_splice-I.cmd;src/condor_tests/job_dagman_splice-I-splice2.dag;src/condor_tests/job_dagman_splice-I.dag;src/condor_tests/job_dagman_splice-I-splice1.dag;src/condor_tests/job_dagman_splice-I-splice3.dag")
	# This test is commented out until we fix Condor.pm to allow multiple dag files to be submitted with condor_submit_dag in TestSubmitDagman
	# condor_pl_test(job_dagman_splice-J "Simple Dagman Splice J" "dagman;quick;ctest")
	condor_pl_test(job_dagman_splice-K "Simple Dagman Splice K" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_splice-K.cmd;src/condor_tests/job_dagman_splice-K.dag;src/condor_tests/job_dagman_splice-K-splice1.dag;src/condor_tests/job_dagman_splice-K-splice2.dag")
	condor_pl_test(job_dagman_splice-L "Simple Dagman Splice L" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_splice-L.cmd;src/condor_tests/job_dagman_splice-L.dag;src/condor_tests/job_dagman_splice-L-subdir1;src/condor_tests/job_dagman_splice-L-subdir2")
	condor_pl_test(job_dagman_splice-M "Simple Dagman Splice M" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_splice-M.cmd;src/condor_tests/job_dagman_splice-M.dag;src/condor_tests/job_dagman_splice-M-subdir1;src/condor_tests/job_dagman_splice-M-subdir2")
	condor_pl_test(job_dagman_splice-N "Simple Dagman Splice N" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_splice-N.cmd;src/condor_tests/job_dagman_splice-N.dag;src/condor_tests/job_dagman_splice-N-subdir1;src/condor_tests/job_dagman_splice-N-subdir2")
	condor_pl_test(job_dagman_splice-O "Simple Dagman Splice O" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_splice-O.cmd;src/condor_tests/job_dagman_splice-O.dag;src/condor_tests/job_dagman_splice-O-subdir1;src/condor_tests/job_dagman_splice-O-subdir2")
	condor_pl_test(job_dagman_splice-P "Simple Dagman Splice P" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_splice-P-0.dag;src/condor_tests/job_dagman_splice-P-1.dag;src/condor_tests/job_dagman_splice-P-2.dag;src/condor_tests/job_dagman_splice-P.cmd;src/condor_scripts/CondorTest.pm;src/condor_scripts/Condor.pm;src/condor_scripts/CondorUtils.pm;src/condor_scripts/CondorPersonal.pm;src/condor_tests/DagmanTest.pm")
	condor_pl_test(job_dagman_splice-Q "Simple Dagman Splice Q" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_splice-Q-0.dag;src/condor_tests/job_dagman_splice-Q-1.dag;src/condor_tests/job_dagman_splice-Q-2.dag;src/condor_tests/job_dagman_splice-Q.cmd;src/condor_scripts/CondorTest.pm;src/condor_scripts/Condor.pm;src/condor_scripts/CondorUtils.pm;src/condor_scripts/CondorPersonal.pm;src/condor_tests/DagmanTest.pm")
	# Turned off -- see gittrac ##4602: Clean up DAGMan VARS parsing.
	#condor_pl_test(job_dagman_splice-R "Simple Dagman Splice R" "dagman;quick;ctest")
	condor_pl_test(job_dagman_pre_skip-A "Simple Dagman Pre Skip A" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_pre_skip-A_A.cmd;src/condor_tests/job_dagman_pre_skip-A_A.sh;src/condor_tests/job_dagman_pre_skip-A_B.cmd;src/condor_tests/job_dagman_pre_skip-A_B.sh;src/condor_tests/job_dagman_pre_skip-A.dag")
	condor_pl_test(job_dagman_pre_skip-B "Simple Dagman Pre Skip B" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_pre_skip-B_A.cmd;src/condor_tests/job_dagman_pre_skip-B_A.sh;src/condor_tests/job_dagman_pre_skip-B.dag;src/condor_tests/job_dagman_pre_skip-B_A-post.sh;src/condor_tests/job_dagman_pre_skip-B_B.cmd")
	condor_pl_test(job_dagman_pre_skip-C "Simple Dagman Pre Skip C" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_pre_skip-C_A.cmd;src/condor_tests/job_dagman_pre_skip-C_A.pl;src/condor_tests/job_dagman_pre_skip-C_B.cmd;src/condor_tests/job_dagman_pre_skip-C.dag")
	condor_pl_test(job_dagman_always_run_post-A "Always run post A" "dagman;quick;ctest" CTEST)
	condor_pl_test(job_dagman_always_run_post-B "Always run post B" "dagman;quick;ctest" CTEST)
	condor_pl_test(job_dagman_always_run_post-C "Always run post C" "dagman;quick;ctest" CTEST)
	condor_pl_test(job_dagman_always_run_post-D "Always run post D" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_always_run_post-D_A.cmd;src/condor_tests/job_dagman_always_run_post-D_A_pre.sh;src/condor_tests/job_dagman_always_run_post-D_A_post.sh;src/condor_tests/job_dagman_always_run_post-D.dag")
	condor_pl_test(job_dagman_always_run_post-E "Always run post E" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_always_run_post-E_A.cmd;src/condor_tests/job_dagman_always_run_post-E_A_pre.sh;src/condor_tests/job_dagman_always_run_post-E_A_post.sh;src/condor_tests/job_dagman_always_run_post-E.dag")
	condor_pl_test(job_dagman_final-A "Simple Dagman Final Node A" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_final-A.dag;src/condor_tests/job_dagman_final-A-nodeB.cmd;src/condor_tests/job_dagman_final-A-node.pl;src/condor_tests/job_dagman_final-A-script.pl;src/condor_tests/job_dagman_final-A-nodeA.cmd;src/condor_tests/job_dagman_final-A-nodeC.cmd")
	condor_pl_test(job_dagman_final-B "Simple Dagman Final Node B" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_final-B.dag;src/condor_tests/job_dagman_final-B-nodeB.cmd;src/condor_tests/job_dagman_final-B-node.pl;src/condor_tests/job_dagman_final-B-script.pl;src/condor_tests/job_dagman_final-B-nodeA.cmd;src/condor_tests/job_dagman_final-B-nodeC.cmd")
	condor_pl_test(job_dagman_final-C "Simple Dagman Final Node C" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_final-C.dag;src/condor_tests/job_dagman_final-C-nodeA.pl;src/condor_tests/job_dagman_final-C-nodeC.cmd;src/condor_tests/job_dagman_final-C-nodeA.cmd;src/condor_tests/job_dagman_final-C-nodeB.cmd;src/condor_tests/job_dagman_final-C-node.pl;src/condor_tests/job_dagman_final-C-script.pl")
	condor_pl_test(job_dagman_final-D "Simple Dagman Final Node D" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_final-D.dag;src/condor_tests/job_dagman_final-D-nodeB.cmd;src/condor_tests/job_dagman_final-D-node.pl;src/condor_tests/job_dagman_final-D-script.pl;src/condor_tests/job_dagman_final-D-nodeA.cmd;src/condor_tests/job_dagman_final-D-nodeC.cmd")
	# turned off for now ticket #4386
	#condor_pl_test(job_dagman_final-E "Simple Dagman Final Node E" "dagman;quick;ctest")
	condor_pl_test(job_dagman_final-F "Simple Dagman Final Node F" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_final-F.config;src/condor_tests/job_dagman_final-F-nodeA.cmd;src/condor_tests/job_dagman_final-F-nodeB2.cmd;src/condor_tests/job_dagman_final-F-nodeC.cmd;src/condor_tests/job_dagman_final-F.dag;src/condor_tests/job_dagman_final-F-nodeB1.cmd;src/condor_tests/job_dagman_final-F-nodeB3.cmd;src/condor_tests/job_dagman_final-F-node.pl;src/condor_tests/job_dagman_final-F-script.pl")
	condor_pl_test(job_dagman_final-G "Simple Dagman Final Node G" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_final-G.dag;src/condor_tests/job_dagman_final-G-nodeA.cmd;src/condor_tests/job_dagman_final-G-node.pl")
	condor_pl_test(job_dagman_final-H "Simple Dagman Final Node H" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_final-H.dag;src/condor_tests/job_dagman_final-H-nodeB1.cmd;src/condor_tests/job_dagman_final-H-nodeC.cmd;src/condor_tests/job_dagman_final-H-nodeA.cmd;src/condor_tests/job_dagman_final-H-nodeB2.cmd;src/condor_tests/job_dagman_final-H-node.pl")
	condor_pl_test(job_dagman_final-I "Simple Dagman Final Node I" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_final-I.dag;src/condor_tests/job_dagman_final-I-nodeB.cmd;src/condor_tests/job_dagman_final-I-nodeD.cmd;src/condor_tests/job_dagman_final-I-nodeA.cmd;src/condor_tests/job_dagman_final-I-nodeC.cmd;src/condor_tests/job_dagman_final-I-node.pl;src/condor_tests/job_dagman_final-I-script.pl")
	#condor_pl_test(job_dagman_final-J "Test DAG with final node with PRE_SKIP" "dagman;quick;ctest")
	condor_pl_test(job_dagman_final-K "Test DAG with final node for which all submit attempts fail" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_final-K.dag;src/condor_tests/job_dagman_final-K-node.sub;src/condor_tests/x_dagman_retry-monitor.pl")
	#condor_pl_test(job_dagman_final-L "Test DAG with final node with multiple procs, all succeed" "dagman;quick;ctest")
	#condor_pl_test(job_dagman_final-M "Test DAG with final node with multiple procs, one fails" "dagman;quick;ctest")
	condor_pl_test(job_dagman_nonexistent_subdag "Test for nonexistent subdag" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_nonexistent_subdag.dag")
	condor_pl_test(job_dagman_latemat "DAGMan + Late Materialization Basic Test" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")

	##### submit foreach variant tests
	condor_pl_test(job_foreach_submit-filepartmacros "submit file usage file part macros" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl;src/condor_tests/x_echostring.pl")
	condor_pl_test(job_foreach_submit-filepartmacros-run "submit file usage file part macros- run submit file" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl;src/condor_tests/x_echostring.pl")
	condor_pl_test(job_foreach_submit-queueitemfrom-1Line "submit file usage queue ITEM from - run submit file" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_print-args.pl")
	condor_pl_test(job_foreach_submit-queueitemfrom-1Line-run "submit file usage queue ITEM from - run submit file" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_print-args.pl")
	condor_pl_test(job_foreach_submit-queueitemfrom-File "submit file usage queue ITEM from - run submit file" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_print-args.pl")
	condor_pl_test(job_foreach_submit-queueitemfrom-File-run "submit file usage queue ITEM from - run submit file" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_print-args.pl")
	condor_pl_test(job_foreach_submit-queueitemfrom-MLine "submit file usage queue ITEM from - run submit file" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_print-args.pl")
	condor_pl_test(job_foreach_submit-queueitemfrom-MLine-run "submit file usage queue ITEM from - run submit file" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_print-args.pl")
	condor_pl_test(job_foreach_submit-queueitemfrom-Stdin "submit file usage queue ITEM from - run submit file" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_print-args.pl")
	condor_pl_test(job_foreach_submit-queueitemfrom-Stdin-run "submit file usage queue ITEM from - run submit file" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_print-args.pl")
	condor_pl_test(job_foreach_submit-queueitemin-commalist "submit file usage queue ITEM in " "quick;ctest" CTEST DEPENDS "src/condor_tests/x_print-args.pl")
	condor_pl_test(job_foreach_submit-queueitemin-commalist-run "submit file usage queue ITEM in - run submit file" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_print-args.pl")
	condor_pl_test(job_foreach_submit-queueitemin "submit file usage queue ITEM in " "quick;ctest" CTEST DEPENDS "src/condor_tests/x_print-args.pl")
	condor_pl_test(job_foreach_submit-queueitemin-run "submit file usage queue ITEM in - run submit file" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_print-args.pl")
	condor_pl_test(job_foreach_submit-queueitemMLineIn "submit file usage queue ITEM in " "quick;ctest" CTEST DEPENDS "src/condor_tests/x_print-args.pl")
	condor_pl_test(job_foreach_submit-queueitemMLineIn-run "submit file usage queue ITEM in - run submit file" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_print-args.pl")
	condor_pl_test(job_foreach_submit-simple-glob "submit file usage match" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(job_foreach_submit-simple-glob-run "submit file usage match - run submit file" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_getsandbox.pl")
	condor_pl_test(job_foreach_submit-queuesliceditemin-commalist "submit file slice test" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_print-args.pl")

	# auth tests
	condor_pl_test(lib_auth_config "Run Authorization Config Tests" "quick;ctest" CTEST DEPENDS "${CMAKE_BINARY_DIR}/src/condor_tests/condor_test_auth")
	add_dependencies_suffix_hack(lib_auth_config condor_test_auth)
	condor_pl_test(lib_auth_protocol-fs "FS authentication test" "quick;ctest" CTEST DEPENDS "src/condor_tests/lib_auth_protocol-fs.cmd;src/condor_tests/x_sleep.pl;src/condor_tests/x_lib_auth_protocol.pl;src/condor_tests/x_param.protocol-fs.template;src/condor_tests/x_secfs_configpostsrc")
	condor_pl_test(lib_auth_protocol-ctb "CTB authentication test" "quick;ctest" CTEST DEPENDS "src/condor_tests/lib_auth_protocol-ctb.cmd;src/condor_tests/x_sleep.pl;src/condor_tests/x_lib_auth_protocol.pl;src/condor_tests/x_param.protocol-ctb.template;src/condor_tests/x_secneg_clR_dmnR_secprepostsrc;src/condor_tests/x_secctb_configpostsrc")
	## Disabled due to bugginess and dubious worth
	#condor_pl_test(lib_auth_protocol-negot "4 way negotiation test - frist 8" "quick;ctest")
	#condor_pl_test(lib_auth_protocol-negot-prt2 "4 way negotiation test - last 8" "quick;ctest")
	#condor_pl_test(lib_auth_protocol-negot-prt3 "4 way negotiation test - last 8" "quick;ctest")
	#condor_pl_test(lib_auth_protocol-negot-prt4 "4 way negotiation test - last 8" "quick;ctest")
	##### generic cmd tests

	condor_pl_test(cmd_status_slot_variants "Condor_status works with static and partitionable slots" "quick;ctest" CTEST DEPENDS "src/condor_tests/CondorCmdStatusWorker2.pm;src/condor_scripts/CondorUtils.pm;src/condor_scripts/Condor.pm;src/condor_scripts/CondorTest.pm;src/condor_scripts/CondorPersonal.pm;src/condor_tests/StatusConfigs;src/condor_tests/StatusAds;src/condor_tests/x_sleep.pl")
	condor_pl_test(cmd_status_shows-any "Condor_status -any works as expected" "quick;ctest" CTEST DEPENDS "src/condor_tests/CondorCmdStatusWorker2.pm;src/condor_scripts/CondorUtils.pm;src/condor_scripts/Condor.pm;src/condor_scripts/CondorTest.pm;src/condor_scripts/CondorPersonal.pm;src/condor_tests/x_sleep.pl")
	condor_pl_test(cmd_status_shows-submitters "Condor_status -submitters works as expected" "quick;ctest" CTEST DEPENDS "src/condor_tests/CondorCmdStatusWorker2.pm;src/condor_scripts/CondorUtils.pm;src/condor_scripts/Condor.pm;src/condor_scripts/CondorTest.pm;src/condor_scripts/CondorPersonal.pm;src/condor_tests/x_sleep.pl")
	condor_pl_test(cmd_status_shows-negotiator "Condor_status -negotiator works as expected" "quick;ctest" CTEST DEPENDS "src/condor_tests/CondorCmdStatusWorker2.pm;src/condor_scripts/CondorUtils.pm;src/condor_scripts/Condor.pm;src/condor_scripts/CondorTest.pm;src/condor_scripts/CondorPersonal.pm;src/condor_tests/x_sleep.pl")
	condor_pl_test(cmd_status_shows-state "Condor_status -state works as expected" "quick;ctest" CTEST DEPENDS "src/condor_tests/CondorCmdStatusWorker2.pm;src/condor_scripts/CondorUtils.pm;src/condor_scripts/Condor.pm;src/condor_scripts/CondorTest.pm;src/condor_scripts/CondorPersonal.pm;src/condor_tests/x_sleep.pl")
	condor_pl_test(cmd_status_shows-format "Condor_status -format works as expected" "quick;ctest" CTEST DEPENDS "src/condor_tests/CondorCmdStatusWorker2.pm;src/condor_scripts/CondorUtils.pm;src/condor_scripts/Condor.pm;src/condor_scripts/CondorTest.pm;src/condor_scripts/CondorPersonal.pm;src/condor_tests/x_sleep.pl")
	condor_pl_test(cmd_status_shows-constraint "Condor_status -constraint works as expected" "quick;ctest" CTEST DEPENDS "src/condor_tests/CondorCmdStatusWorker2.pm;src/condor_scripts/CondorUtils.pm;src/condor_scripts/Condor.pm;src/condor_scripts/CondorTest.pm;src/condor_scripts/CondorPersonal.pm;src/condor_tests/x_sleep.pl")
	condor_pl_test(cmd_status_shows-avail "Condor_status -avail works as expected" "quick;ctest" CTEST DEPENDS "src/condor_tests/CondorCmdStatusWorker2.pm;src/condor_scripts/CondorUtils.pm;src/condor_scripts/Condor.pm;src/condor_scripts/CondorTest.pm;src/condor_scripts/CondorPersonal.pm;src/condor_tests/x_sleep.pl")
	condor_pl_test(cmd_status_shows-claim "Condor_status -claim works as expected" "quick;ctest" CTEST DEPENDS "src/condor_tests/CondorCmdStatusWorker2.pm;src/condor_scripts/CondorUtils.pm;src/condor_scripts/Condor.pm;src/condor_scripts/CondorTest.pm;src/condor_scripts/CondorPersonal.pm;src/condor_tests/x_sleep.pl")
	condor_pl_test(cmd_status_shows-help "Condor_status -help works as expected" "quick;ctest" CTEST)
	condor_pl_test(cmd_status_shows-master "Condor_status -master works as expected" "quick;ctest" CTEST DEPENDS "src/condor_tests/CondorCmdStatusWorker2.pm;src/condor_scripts/CondorUtils.pm;src/condor_scripts/Condor.pm;src/condor_scripts/CondorTest.pm;src/condor_scripts/CondorPersonal.pm;src/condor_tests/x_sleep.pl")
	condor_pl_test(cmd_status_shows-schedd "Condor_status -schedd works as expected" "quick;ctest" CTEST DEPENDS "src/condor_tests/CondorCmdStatusWorker2.pm;src/condor_scripts/CondorUtils.pm;src/condor_scripts/Condor.pm;src/condor_scripts/CondorTest.pm;src/condor_scripts/CondorPersonal.pm;src/condor_tests/x_sleep.pl")
	condor_pl_test(cmd_status_shows-slots "Condor_status works as expected" "quick;ctest" CTEST DEPENDS "src/condor_tests/CondorCmdStatusWorker2.pm;src/condor_scripts/CondorUtils.pm;src/condor_scripts/Condor.pm;src/condor_scripts/CondorTest.pm;src/condor_scripts/CondorPersonal.pm;src/condor_tests/x_sleep.pl")
	condor_pl_test(cmd_status_shows-absent "Condor_status -absent works as expected" "quick;ctest" CTEST DEPENDS "src/condor_tests/CondorCmdStatusWorker2.pm;src/condor_scripts/CondorUtils.pm;src/condor_scripts/Condor.pm;src/condor_scripts/CondorTest.pm;src/condor_scripts/CondorPersonal.pm;src/condor_tests/x_sleep.pl")
	condor_pl_test(cmd_q_shows-dag "Condor_q shows dag" "quick;ctest" CTEST)
	condor_pl_test(cmd_q_shows-hold "Condor_q shows hold" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(cmd_q_shows-run "Condor_q shows runs" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(cmd_q_shows-global "Condor_q shows global" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(cmd_q_shows-name "Condor_q shows name" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(cmd_q_shows-pool "Condor_q shows pool" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(cmd_q_shows-sub "Condor_q shows sub" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(cmd_q_shows-better-analyze "Condor_q shows better-analyze" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl;src/condor_tests/cmd_q_shows-better-analyze.cmd")
	condor_pl_test(cmd_q_shows-xml "Condor_q shows xml" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(cmd_q_shows-cputime "Condor_q shows cputime" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(cmd_q_shows-format "Condor_q shows format" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(cmd_q_formats "Condor_q formats" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(cmd_q_shows-batch "Condor_q shows batch" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(cmd_q_shows-af "Condor_q_shows af" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(cmd_status-af "Condor_status af" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl;src/condor_tests/cmd_status-af_orig_first_test.ads;src/condor_tests/cmd_status-af_orig_second_test.ads")
	condor_pl_test(cmd_q_format_file "Condor_q format_file" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(cmd_transform_ads "Condor_transform_ads" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(cmd_status-transform "Condor_status transform" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(cmd_status-compact "Condor_status compact" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(disable_owner_test "Disable_owner_test" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(max_jobs_per_owner_test "Max_jobs_per_owner_test" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(max_jobs_per_owner_test_group "Max_jobs_per_owner_test -group" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(job_transform_names_test "Job_transform_names_test" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(cmd_submit-batchname "condor_submit -batch-name" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(file_transfer_submit "condor_submit file transfer" "quick;ctest" CTEST)
	condor_pl_test(config_validate_policy_templates "config_validate_policy_templates" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(docker_simulator_test "docker_simulator_test" "quick;ctest" CTEST DEPENDS "src/condor_tests/docker_simulator.pl;src/condor_tests/x_sleep.pl")
	condor_pl_test(map_user_to_groups_metaknob_test "map_user_to_groups_metaknob_test" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(cmd_submit-factory "condor_submit -factory" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(job_xform_factory "submit -factory with transforms" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(job_ad_epoch_recording "Test Job Ads recorded per epoch" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")

	if (LINUX)
		condor_pl_test(cmd_ssh_to_job_van "condor_ssh_to_job" "quick;ctest" CTEST DEPENDS "src/condor_tests/cmd_ssh_to_job_van.pl")
	endif()
	condor_pl_test(cmd_drain "condor_drain" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(defrag "condor_defrag" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")

	if (NOT WINDOWS)
		condor_pl_test(cmd_drain_scavenging "condor_drain -start" "quick;ctest" CTEST DEPENDS "src/condor_tests/CustomMachineResource.pm;src/condor_scripts/CondorTest.pm;src/condor_scripts/Condor.pm;src/condor_scripts/CondorUtils.pm;src/condor_scripts/CondorPersonal.pm")
	endif(NOT WINDOWS)

	condor_pl_test(cmd_drain_scavenging_vacation "condor_drain -start with lazy job" "quick;ctest" CTEST DEPENDS "src/condor_tests/CustomMachineResource.pm;src/condor_scripts/CondorTest.pm;src/condor_scripts/Condor.pm;src/condor_scripts/CondorUtils.pm;src/condor_scripts/CondorPersonal.pm")

	##### lib log tests
	condor_pl_test(lib_eventlog "Basic Event Log Test" "core;quick;ctest" CTEST DEPENDS "${CMAKE_BINARY_DIR}/src/condor_tests/x_read_joblog.exe")
	add_dependencies_suffix_hack(lib_eventlog x_read_joblog.exe)
	condor_pl_test(lib_eventlog-xml "Basic XML Event Log Test" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl;${CMAKE_BINARY_DIR}/src/condor_tests/x_read_joblog.exe;src/condor_tests/x_lib_eventlog.expected")
	add_dependencies_suffix_hack(lib_eventlog-xml x_read_joblog.exe)
	condor_pl_test(lib_eventlog_base "Test re-initialization of eventlog reader does not cause seg faults" "lib;quick;ctest" CTEST DEPENDS "${CMAKE_BINARY_DIR}/src/condor_tests/lib_eventlog_base_executable.exe")
	add_dependencies_suffix_hack(lib_eventlog_base lib_eventlog_base_executable.exe)

	#dprint("TODO: Need to fix lib_eventlog_rotation-defaults test")
	#condor_pl_test(lib_eventlog_rotation-defaults "Event Log Rotation: Default settings" "quick;ctest")
	condor_pl_test(lib_chirp "Exercise the chirp I/O tool" "lib;quick;ctest" CTEST)
	condor_pl_test(lib_chirpio_van "Exercise the chirp I/O library using the C client library" "lib;quick;ctest" CTEST DEPENDS "${CMAKE_BINARY_DIR}/src/condor_tests/lib_chirpio.exe")
	add_dependencies_suffix_hack(lib_chirpio_van lib_chirpio.exe)
	condor_pl_test(lib_eventlog_rotation-no_rotations_1 "Event Log Rotation: no rotations/1" "quick;ctest" CTEST DEPENDS "src/condor_tests/lib_eventlog_rotation.pl;${CMAKE_BINARY_DIR}/src/condor_tests/test_log_writer")
	add_dependencies(lib_eventlog_rotation-no_rotations_1 test_log_writer)
	condor_pl_test(lib_eventlog_rotation-no_rotations_2 "Event Log Rotation: no rotations/2" "quick;ctest" CTEST DEPENDS "src/condor_tests/lib_eventlog_rotation.pl;${CMAKE_BINARY_DIR}/src/condor_tests/test_log_writer")
	add_dependencies(lib_eventlog_rotation-no_rotations_2 test_log_writer)
	condor_pl_test(lib_eventlog_rotation-no_rotations_3 "Event Log Rotation: no rotations/3" "quick;ctest" CTEST DEPENDS "src/condor_tests/lib_eventlog_rotation.pl;${CMAKE_BINARY_DIR}/src/condor_tests/test_log_writer")
	add_dependencies(lib_eventlog_rotation-no_rotations_3 test_log_writer)
	condor_pl_test(lib_eventlog_rotation-w_old_rotations "Event Log Rotation: old rotations" "quick;ctest" CTEST DEPENDS "src/condor_tests/lib_eventlog_rotation.pl;${CMAKE_BINARY_DIR}/src/condor_tests/test_log_writer")
	add_dependencies(lib_eventlog_rotation-w_old_rotations test_log_writer)
	condor_pl_test(lib_eventlog_rotation-w_old_rotations_2_loops "Event Log Rotation: old rotations / 2 loops" "quick;ctest" CTEST DEPENDS "src/condor_tests/lib_eventlog_rotation.pl;${CMAKE_BINARY_DIR}/src/condor_tests/test_log_writer")
	add_dependencies(lib_eventlog_rotation-w_old_rotations_2_loops test_log_writer)
	condor_pl_test(lib_eventlog_rotation-w_2_rotations "Event Log Rotation: 2 rotations" "quick;ctest" CTEST DEPENDS "src/condor_tests/lib_eventlog_rotation.pl;${CMAKE_BINARY_DIR}/src/condor_tests/test_log_writer")
	add_dependencies(lib_eventlog_rotation-w_2_rotations test_log_writer)
	condor_pl_test(lib_eventlog_rotation-w_2_rotations_2_loops "Event Log Rotation: 2 rotations / 2 loops" "quick;ctest" CTEST DEPENDS "src/condor_tests/lib_eventlog_rotation.pl;${CMAKE_BINARY_DIR}/src/condor_tests/test_log_writer")
	add_dependencies(lib_eventlog_rotation-w_2_rotations_2_loops test_log_writer)
	condor_pl_test(lib_eventlog_rotation-w_5_rotations "Event Log Rotation: 5 rotations" "quick;ctest" CTEST DEPENDS "src/condor_tests/lib_eventlog_rotation.pl;${CMAKE_BINARY_DIR}/src/condor_tests/test_log_writer")
	add_dependencies(lib_eventlog_rotation-w_5_rotations test_log_writer)
	condor_pl_test(lib_eventlog_rotation-w_20_rotations "Event Log Rotation: 20 rotations" "quick;ctest" CTEST DEPENDS "src/condor_tests/lib_eventlog_rotation.pl;${CMAKE_BINARY_DIR}/src/condor_tests/test_log_writer")
	add_dependencies(lib_eventlog_rotation-w_20_rotations test_log_writer)
	condor_pl_test(lib_eventlog_rotation-w_20_rotations_2_loops "Event Log Rotation: 20 rotations / 2 loops" "quick;ctest" CTEST DEPENDS "src/condor_tests/lib_eventlog_rotation.pl;${CMAKE_BINARY_DIR}/src/condor_tests/test_log_writer")
	add_dependencies(lib_eventlog_rotation-w_2_rotations_2_loops test_log_writer)
	condor_pl_test(lib_eventlog_rotation-w_20_rotations_20_loops "Event Log Rotation: 20 rotations / 20 loops" "quick;ctest" CTEST DEPENDS "src/condor_tests/lib_eventlog_rotation.pl;${CMAKE_BINARY_DIR}/src/condor_tests/test_log_writer")
	add_dependencies(lib_eventlog_rotation-w_20_rotations_20_loops test_log_writer)
	condor_pl_test(lib_eventlog_rotation-reader_simple "Event Log Rotation: reader" "quick;ctest" CTEST DEPENDS "src/condor_tests/lib_eventlog_rotation.pl;${CMAKE_BINARY_DIR}/src/condor_tests/test_log_writer;${CMAKE_BINARY_DIR}/src/condor_tests/test_log_reader")
	add_dependencies(lib_eventlog_rotation-reader_simple test_log_writer test_log_reader)
	condor_pl_test(lib_eventlog_rotation-reader_old_rotations "Event Log Rotation: reader, old rotations" "quick;ctest" CTEST DEPENDS "src/condor_tests/lib_eventlog_rotation.pl;${CMAKE_BINARY_DIR}/src/condor_tests/test_log_writer;${CMAKE_BINARY_DIR}/src/condor_tests/test_log_reader")
	add_dependencies(lib_eventlog_rotation-reader_old_rotations test_log_writer test_log_reader)
	condor_pl_test(lib_eventlog_rotation-reader_2_rotations "Event Log Rotation: reader, 2 rotations" "quick;ctest" CTEST DEPENDS "src/condor_tests/lib_eventlog_rotation.pl;${CMAKE_BINARY_DIR}/src/condor_tests/test_log_writer;${CMAKE_BINARY_DIR}/src/condor_tests/test_log_reader")
	add_dependencies(lib_eventlog_rotation-reader_old_rotations test_log_writer test_log_reader)
	condor_pl_test(lib_eventlog_rotation-reader_2_rotations_2_loops "Event Log Rotation: reader, 2 rotations / 2 loops" "quick;ctest" CTEST DEPENDS "src/condor_tests/lib_eventlog_rotation.pl;${CMAKE_BINARY_DIR}/src/condor_tests/test_log_writer;${CMAKE_BINARY_DIR}/src/condor_tests/test_log_reader")
	add_dependencies(lib_eventlog_rotation-reader_old_rotations test_log_writer test_log_reader)
	condor_pl_test(lib_eventlog_rotation-reader_20_rotations "Event Log Rotation: reader, 20 rotations" "quick;ctest" CTEST DEPENDS "src/condor_tests/lib_eventlog_rotation.pl;${CMAKE_BINARY_DIR}/src/condor_tests/test_log_writer;${CMAKE_BINARY_DIR}/src/condor_tests/test_log_reader")
	add_dependencies(lib_eventlog_rotation-reader_old_rotations test_log_writer test_log_reader)
	condor_pl_test(lib_eventlog_rotation-reader_20_rotations_2_loops "Event Log Rotation: reader, 20 rotations / 2 loops" "quick;ctest" CTEST DEPENDS "src/condor_tests/lib_eventlog_rotation.pl;${CMAKE_BINARY_DIR}/src/condor_tests/test_log_writer;${CMAKE_BINARY_DIR}/src/condor_tests/test_log_reader")
	add_dependencies(lib_eventlog_rotation-reader_old_rotations test_log_writer test_log_reader)
	condor_pl_test(lib_eventlog_rotation-reader_20_rotations_20_loops "Event Log Rotation: reader, 20 rotations / 20 loops" "quick;ctest" CTEST DEPENDS "src/condor_tests/lib_eventlog_rotation.pl;${CMAKE_BINARY_DIR}/src/condor_tests/test_log_writer;${CMAKE_BINARY_DIR}/src/condor_tests/test_log_reader")
	add_dependencies(lib_eventlog_rotation-reader_old_rotations test_log_writer test_log_reader)
	condor_pl_test(lib_eventlog_rotation-reader_missed_1 "Event Log Rotation: reader, missed events 1" "quick;ctest" CTEST DEPENDS "src/condor_tests/lib_eventlog_rotation.pl;${CMAKE_BINARY_DIR}/src/condor_tests/test_log_writer;${CMAKE_BINARY_DIR}/src/condor_tests/test_log_reader")
	add_dependencies(lib_eventlog_rotation-reader_old_rotations test_log_writer test_log_reader)
	condor_pl_test(lib_eventlog_rotation-reader_missed_2 "Event Log Rotation: reader, missed events 2" "quick;ctest" CTEST DEPENDS "src/condor_tests/lib_eventlog_rotation.pl;${CMAKE_BINARY_DIR}/src/condor_tests/test_log_writer;${CMAKE_BINARY_DIR}/src/condor_tests/test_log_reader")
	add_dependencies(lib_eventlog_rotation-reader_old_rotations test_log_writer test_log_reader)
	condor_pl_test(negotiator_match_log_exists "ensure Matlog has only matches" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(job_mem_checker "Look at reported memory usage" "quick;ctest" CTEST DEPENDS "${CMAKE_BINARY_DIR}/src/condor_tests/x_job_mem_checker.exe;src/condor_tests/x_sleep.pl")
	add_dependencies_suffix_hack(job_mem_checker x_job_mem_checker.exe)
	condor_pl_test(cmd_condor_submit_dashspool "Look at condor_submit -spool" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_condorsubmitdashspool.pl;src/condor_tests/x_sleep.pl")
	condor_pl_test(config_if_then_else "config file ifdefs" "quick;ctest" CTEST)
	condor_pl_test(job_massive_config "LARGE config file" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(cmd_ccv_set_rset "condor_config_val set/rset" "quick;ctest" CTEST)
	condor_pl_test(cmd_gpu_discovery "gpu discovery file" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")

	if(NOT APPLE)
	# globbiing only firs not supported on these platforms
	condor_pl_test(job_foreach_submit-initialdir "submit file usage InitialDir" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_getsandbox.pl")
	condor_pl_test(job_foreach_submit-initialdir-run "submit file usage InitialDir - run submit file" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_getsandbox.pl")
	endif ()

	if(LINUX)
		condor_pl_test(job_cpu_affinity_check "ensure Matlog has only matches" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_getmyprocinfo.pl;src/condor_tests/x_sleep.pl")
	endif ( LINUX )

	condor_pl_test(job_basic_preempt "preempt and nopreempt tests" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(lib_include "config include mechanism" "quick;ctest" CTEST DEPENDS "src/condor_tests/Includes")
	#condor_pl_test(job_schedd_restart-check_reconnect "test reconnects" "quick;ctest")
	condor_pl_test(job_local_resources "test local resources" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(job_check_exitcode_in_classad "test exit code" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl;src/condor_tests/x_setexitcode.pl")
	condor_pl_test(job_services_during_neg_cycle "test cmds during neg cycle" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(job_basic_kill "kill test" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl;src/condor_tests/x_catchallsignals.pl")
	condor_pl_test(basic_startd_slot_attrs "STARTD_SLOT_ATTRS test" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(job_basic_suspend_continue_test "test suspends and continue" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(job_slash_transfer "dir contents to top sandbox" "quick;ctest" CTEST DEPENDS "src/condor_tests/slashtransfer.pl;src/condor_tests/slashxfer")
	condor_pl_test(job_basic_start_test "START reserve defines a dedicated slot" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(job_basic_param_config_check "param system checks" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(cmd_ccval_remote "more remote param system checks" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(cmd_q_protocols "version variance checking" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(job_rank_and_aclustering "rank and autoclustering checking" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	#condor_pl_test(cmd_ccval_remote "param remote checks" "quick;ctest")
	condor_pl_test(lib_param_completeness "minimal knobs" "quick;ctest" CTEST DEPENDS "${CMAKE_BINARY_DIR}/src/condor_tests/x_complete_params.exe;${CMAKE_BINARY_DIR}/src/condor_utils/param_info_tables.h")
	add_dependencies_suffix_hack(lib_param_completeness x_complete_params.exe)
	condor_pl_test(lib_param_conditionals "config conditionals" "quick;ctest" CTEST DEPENDS "${CMAKE_BINARY_DIR}/src/condor_tests/x_conditional_params.exe")
	add_dependencies_suffix_hack(lib_param_conditionals x_conditional_params.exe)
	condor_pl_test(unit_test_macro_expand "config macro unit tests" "quick;ctest" CTEST DEPENDS "src/condor_tests/NetworkTestConfigs.pm;${CMAKE_BINARY_DIR}/src/condor_tests/test_macro_expand")
	add_dependencies(unit_test_macro_expand test_macro_expand)
	condor_pl_test(unit_test_classad_funcs "classad function tests" "quick;ctest" CTEST DEPENDS "${CMAKE_BINARY_DIR}/src/condor_tests/test_classad_funcs")
	add_dependencies(unit_test_macro_expand test_classad_funcs)
	condor_pl_test(unit_test_user_mapping "MapFile parse and map unit tests" "quick;ctest" CTEST DEPENDS "src/condor_tests/NetworkTestConfigs.pm")
	#condor_pl_test(cmd_condor_ping_basic "Basic default test of condor_ping" "quick;ctest")
	condor_pl_test(job_aggressive_flocking "Test aggressive flocking" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	##### job tests
	#condor_pl_test(job_had_basic "Basic High Availability Daemon Test" "quick;ctest")
	condor_pl_test(job_core_basic_par "Basic Parallel Universe" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_echostring.pl")
	#condor_pl_test(job_core_basic-security_par "Basic Parallel Universe w/security on" "quick;ctest")

	# Our ppc machine has a broken java.  Ignore the tests for now
	if (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "ppc64le")
		set(ENABLE_JAVA_TESTS FALSE)
	endif()

	if ( ENABLE_JAVA_TESTS )
		condor_pl_test(job_core_err_java "Java Universe: basic error test" "quick;ctest;java" CTEST DEPENDS "src/condor_tests/x_java_CondorEcho.class;src/condor_tests/job_core_err_java.cmd" JAVA)
		condor_pl_test(job_core_output_java "Java Universe: basic output test" "quick;ctest;java" CTEST DEPENDS "src/condor_tests/job_core_output_java.cmd;src/condor_tests/x_java_ReadStdin.class;src/condor_tests/job_core_output.in" JAVA)
		condor_pl_test(job_core_hold_java "Java Universe: basic output test" "quick;ctest;java" CTEST DEPENDS "src/condor_tests/x_java_Sleep.class;src/condor_tests/job_core_hold_java.cmd" JAVA)
		condor_pl_test(job_core_initialdir_java "Java Universe: basic output test" "quick;ctest;java" CTEST DEPENDS "src/condor_tests/x_java_CondorEcho.class;src/condor_tests/job_core_initialdir_java.cmd;src/condor_tests/job_core_input.in" JAVA)
		condor_pl_test(job_core_input_java "Java Universe: basic output test" "quick;ctest;java" CTEST DEPENDS "src/condor_tests/x_java_ReadStdin.class;src/condor_tests/job_core_input_java.cmd;src/condor_tests/job_core_input.in" JAVA)
		#condor_pl_test(job_core_plus_java "Java Universe: basic output test" "quick;ctest;java")
	endif ( ENABLE_JAVA_TESTS )

	condor_pl_test(job_filexfer_streamout_van "Vanilla: test for streaming of output when both stream_output is true and false" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_job_filexfer_stream.pl")
	condor_pl_test(job_filexfer_streamerr_van "Vanilla: test for streaming of error when both stream_error is true and false" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_job_filexfer_stream.pl")
	condor_pl_test(job_condorc_ab_van "Condor-C AB test" "condorc;quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(job_condorc_abc_van "Condor-C ABC test" "condorc;quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(job_core_macros-dollardollar_van "Vanilla: Did dollar dollar macros work?" "quick;ctest" CTEST DEPENDS "src/condor_tests/job_core_macros-dollardollar_van.cmd;src/condor_tests/x_print-args-and-env.pl")
	condor_pl_test(job_core_onexithold_local "Local: Verify true and false triggers of on_exit_hold" "quick;ctest;local" CTEST DEPENDS "src/condor_tests/x_time.pl")
	condor_pl_test(job_core_onexitrem_local "Local: Verify true and false triggers of on_exit_remove" "quick;ctest;local" CTEST DEPENDS "src/condor_tests/x_time.pl")
	condor_pl_test(job_core_perhold_local "Local: verify true and false triggers for periodic hold" "quick;ctest;local" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(job_core_perrelease_local "Local: verify a policy of true/false for periodic release does the right thing" "quick;ctest;local" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(job_core_perremove_local "Local: test true/false variations of periodic remove" "quick;ctest;local" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(job_core_crontab "Local and Van: CronTab Scheduling" "quick;ctest;local;cron" CTEST DEPENDS "src/condor_tests/x_time.pl")
	condor_pl_test(job_core_time-deferral "Local and Van: Verify deferred execution" "quick;ctest;local;cron" CTEST DEPENDS "src/condor_tests/x_time.pl")
	condor_pl_test(job_core_time-deferral-hold "Local and Van: Verify HOLD against deferred execution" "quick;ctest;local;cron" CTEST DEPENDS "src/condor_tests/x_time.pl")
	condor_pl_test(job_core_time-deferral-remove "Local and Van: Verify REMOVE against deferred execution" "quick;ctest;local;cron" CTEST DEPENDS "src/condor_tests/x_time.pl")
	condor_pl_test(job_core_lease_duration "Van: check job_lease_duration and lease renewal" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	#condor_pl_test(job_core_onexitrem_sched "Scheduler: Verify true and false triggers of on_exit_remove" "quick;ctest")
	#condor_pl_test(job_core_onexithold_sched "Scheduler: Verify true and false triggers of on_exit_hold" "quick;ctest")
	condor_pl_test(job_filexfer_basic_van "Vanilla: send a file and get it back" "filexfer;quick;ctest" CTEST DEPENDS "src/condor_tests/job_filexfer_basic.data;src/condor_tests/x_copy_binary_file.pl")
	condor_pl_test(job_filexfer_minus1_van "Vanilla: extra specified output file creates shadow exception to rerun to produce missing output" "filexfer;quick;ctest" CTEST DEPENDS "src/condor_tests/x_job_filexfer_testjob.pl")
	condor_pl_test(job_filexfer_output-withvacate_van "Vanilla: 3 files created before vacate - verify their return" "filexfer;quick;ctest" CTEST DEPENDS "src/condor_tests/x_job_filexfer_testjob.pl")
	#condor_pl_test(job_filexfer_trans-nodflts_van,"Vanilla: all transfers explicitely off - complains..." "filexfer;quick;ctest")
	#condor_pl_test(job_core_matchlist_van "Vanilla: test that match_list_length is storing and rotating matches" "quick;ctest")
	condor_pl_test(job_core_onexitrem_van "Vanilla: Verify true and false triggers of on_exit_remove" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_time.pl")
	condor_pl_test(job_core_perhold_van "Vanilla: verify true and false triggers for periodic hold" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(job_core_onexithold_van "Vanilla: Verify true and false triggers of on_exit_hold" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_time.pl")
	condor_pl_test(job_core_holdrelease_sched "Scheduler: Test state changes from running to hold to release." "quick;ctest" CTEST DEPENDS "src/condor_tests/job_core_holdrelease_sched.cmd;src/condor_tests/x_sleep.pl")
	condor_pl_test(job_core_args_van "Vanilla: Did specify args from submit file reach the job?" "quick;ctest" CTEST DEPENDS "src/condor_tests/job_core_args_van.cmd;src/condor_tests/job_core_args.pl")
	condor_pl_test(job_core_args_sched "scheduler: Did specify args from submit file reach the job?" "quick;ctest" CTEST DEPENDS "src/condor_tests/job_core_args_sched.cmd;src/condor_tests/job_core_args.pl")
	condor_pl_test(job_core_holdrelease_van "Vanilla: Test state changes from running to hold to release." "quick;ctest" CTEST DEPENDS "src/condor_tests/job_core_holdrelease_van.cmd;src/condor_tests/x_sleep.pl")
	condor_pl_test(job_core_copytospool_van "Vanilla: test that spooled executable is same via sha256 checksum of both files" "quick;ctest" CTEST DEPENDS "src/condor_tests/job_core_copytospool_van.cmd;src/condor_tests/x_sleep.pl")
	#condor_pl_test(job_core_getenv_van "Vanilla: job which tests getenv both on and off" "quick;ctest")
	condor_pl_test(job_core_doublejeopardy_van "Vanilla: make sure condor does not generate an abort AND the expected abnormal termination." "quick;ctest" CTEST DEPENDS "src/condor_tests/job_core_doublejeopardy_van.cmd;src/condor_tests/x_doublejeopardy.pl")
	condor_pl_test(job_core_doublejeopardy_sched "Scheduler: make sure condor does not generate an abort AND the expected abnormal termination." "quick;ctest" CTEST DEPENDS "src/condor_tests/job_core_doublejeopardy_sched.cmd;src/condor_tests/x_doublejeopardy.pl")
	condor_pl_test(job_core_env_sched "Scheduler: Does environment get to the job" "quick;ctest" CTEST DEPENDS "src/condor_tests/job_core_env_sched.cmd")
	condor_pl_test(job_core_env_van "Vanilla: Does environment get to the job" "quick;ctest" CTEST DEPENDS "src/condor_tests/job_core_env_van.cmd")
	condor_pl_test(job_core_err_sched "Scheduler: Does requested error file get used?" "quick;ctest" CTEST DEPENDS "src/condor_tests/job_core_err.pl")
	condor_pl_test(job_core_err_van "Vanilla: Does requested error file get used?" "quick;ctest" CTEST DEPENDS "src/condor_tests/job_core_err.pl")
	condor_pl_test(job_core_hold_sched "Scheduler: job is told to begin on hold. So we test for that state when we get the submitted event" "quick;ctest" CTEST DEPENDS "src/condor_tests/job_core_hold_sched.cmd;src/condor_tests/x_sleep.pl")
	condor_pl_test(job_core_hold_van "Vanilla: job is told to begin on hold. So we test for that state when we get the submitted event" "quick;ctest" CTEST DEPENDS "src/condor_tests/job_core_hold_van.cmd;src/condor_tests/x_sleep.pl")
	condor_pl_test(job_core_initialdir_sched "Scheduler: We change the initialdir and assure log and stuff goes there" "quick;ctest" CTEST DEPENDS "src/condor_tests/job_core_initialdir_sched.cmd;src/condor_tests/x_sleep.pl")
	condor_pl_test(job_core_initialdir_van "Vanilla: We change the initialdir and assure log and stuff goes there" "quick;ctest" CTEST DEPENDS "src/condor_tests/job_core_initialdir_van.cmd;src/condor_tests/x_echo1234.pl")
	condor_pl_test(job_core_remote-initialdir_van "Vanilla: We change the remote_initialdir and assure the job runs there" "quick;ctest" CTEST DEPENDS "src/condor_tests/job_core_remote-initialdir_van.cmd;src/condor_tests/job_core_remote-initialdir_van.in")
	condor_pl_test(job_core_input_sched "Scheduler: Verify the specified input file is used" "quick;ctest" CTEST DEPENDS "src/condor_tests/job_core_input_sched.cmd;src/condor_tests/job_core_io.pl;src/condor_tests/job_core_input.in")
	condor_pl_test(job_core_input_van "Vanilla: Verify the specified input file is used" "quick;ctest" CTEST DEPENDS "src/condor_tests/job_core_input_van.cmd;src/condor_tests/job_core_io.pl;src/condor_tests/job_core_input.in")
	#condor_pl_test(job_core_leaveinqueue_sched "Scheduler: verify true and false classads do leave_in_queue right" "quick;ctest")
	condor_pl_test(job_core_leaveinqueue_van "Vanilla: verify true and false classads do leave_in_queue right" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_time.pl")
	condor_pl_test(job_core_macros_sched "Scheduler: a number of macros get passed in as args" "quick;ctest" CTEST DEPENDS "src/condor_tests/job_core_macros_sched.cmd;src/condor_tests/job_core_macros.pl")
	condor_pl_test(job_core_output_sched "Scheduler: Verify trivially simple output from an input file" "quick;ctest" CTEST DEPENDS "src/condor_tests/job_core_output_sched.cmd;src/condor_tests/job_core_io.pl;src/condor_tests/job_core_output.in")
	condor_pl_test(job_core_output_van "Vanilla: Verify trivially simple output from an input file" "quick;ctest" CTEST DEPENDS "src/condor_tests/job_core_output_van.cmd;src/condor_tests/job_core_io.pl;src/condor_tests/job_core_output.in")
	condor_pl_test(job_core_perhold_sched "Scheduler: verify true and false triggers for periodic hold" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(job_core_perrelease_sched "Scheduler: verify a policy of true/false for periodic release does the right thing" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(job_core_perrelease_van "Vanilla: verify a policy of true/false for periodic release does the right thing" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(job_core_perremove_van "Vanilla: test true/false variations of periodic remove" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(job_core_perremove_sched "Scheduler: test true/false variations of periodic remove" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(job_core_plus_van "Vanilla: Verify +entries go to job ad" "quick;ctest" CTEST DEPENDS "src/condor_tests/job_core_plus_van.cmd;src/condor_tests/x_sleep.pl")
	condor_pl_test(job_core_plus_sched "Scheduler: Verify +entries go to job ad" "quick;ctest" CTEST DEPENDS "src/condor_tests/job_core_plus_sched.cmd;src/condor_tests/x_sleep.pl")
	condor_pl_test(job_core_niceuser_van "Vanilla: Ensure non nice_user job completes before nice_user job" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_do_niceuser.pl")
	#condor_pl_test(job_core_priority_van "Vanilla: Verify respect for job priority" "quick;ctest")
	condor_pl_test(job_core_queue_sched "Scheduler: Verify the  queue command" "quick;ctest" CTEST DEPENDS "src/condor_tests/job_core_queue_sched.cmd;src/condor_tests/job_core_queue.pl;src/condor_tests/x_general_client.pl;src/condor_tests/x_general_server.pl;src/condor_tests/x_readqueuelog.pl")
	condor_pl_test(job_core_max-running_local "Local: Max job running limit" "quick;ctest;local" CTEST)
	condor_pl_test(job_late_materialize "Late materialization of jobs" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(extended_submit_cmds "Extended submit commands" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_cat.pl")
	condor_pl_test(submit_templates "Submit templates" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_cat.pl")
	condor_pl_test(job_schedd_restart-holdjobs-ok "Held queue ok after schedd restart" "quick;ctest" CTEST)
	condor_pl_test(job_schedd_restart-runningjobs-ok "Running queue ok after schedd restart" "quick;ctest" CTEST)
	condor_pl_test(job_schedd_restart-basic "schedd restarts" "quick;ctest" CTEST)
	condor_pl_test(job_negotiator_restart "Make user's priority factor survives across condor restart" "quick;ctest" CTEST DEPENDS "src/condor_tests/job_negotiator_restart.dag;src/condor_tests/job_negotiator_restart-nodeAC.pl;src/condor_tests/job_negotiator_restart-nodeB.pl;src/condor_tests/job_negotiator_restart-nodeAC.cmd;src/condor_tests/job_negotiator_restart-nodeB.cmd")
	condor_pl_test(job_filexfer_base-input1_van "Vanilla: Tests input file does not make it with filetransfer off" "filexfer;quick;ctest" CTEST DEPENDS "src/condor_tests/job_filexfer_base-input1_van.cmd;src/condor_tests/x_job_filexfer_testjob.pl")
	# Turn this one back on in 7.7.2 after the file xfer kerfuffle is over
	#condor_pl_test(job_filexfer_base-input4_van "Vanilla: Verifies submit fails with input list and with filetransfer off" "filexfer;quick;ctest")
	condor_pl_test(job_filexfer_base_van "Vanilla: Is executable moved correctly to initialdir/sandbox?" "filexfer;quick;ctest" CTEST DEPENDS "src/condor_tests/x_job_filexfer_testjob.pl")
	condor_pl_test(job_filexfer_input-onegone_van "Vanilla: test for failure when specified input file is missing" "filexfer;quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl;src/condor_tests/x_job_filexfer_testjob.pl")
	condor_pl_test(job_filexfer_limitback_van "Vanilla: ask for exactly 2 files returned" "filexfer;quick;ctest" CTEST DEPENDS "src/condor_tests/x_job_filexfer_testjob.pl")
	condor_pl_test(job_filexfer_trans-excut-true_van "Vanilla: explicitely set transfer_executable... test complains" "filexfer;quick;ctest" CTEST DEPENDS "src/condor_tests/x_job_filexfer_testjob.pl")
	condor_pl_test(job_filexfer_whento-base_van "Vanilla: Inputless test of a job running with transfer files enabled." "filexfer;quick;ctest" CTEST DEPENDS "src/condor_tests/x_job_filexfer_testjob.pl")
	condor_pl_test(job_filexfer_whento-simpleinput_van "Vanilla: job runs with input file requested" "filexfer;quick;ctest" CTEST DEPENDS "src/condor_tests/x_job_filexfer_testjob.pl")
	condor_pl_test(job_filexfer_whento-withinput_van "Vanilla: Multiple input files arrive ok" "filexfer;quick;ctest" CTEST DEPENDS "src/condor_tests/x_job_filexfer_testjob.pl")
	condor_pl_test(job_filexfer_output_van "Vanilla: Are 6 output files returned" "filexfer;quick;ctest" CTEST DEPENDS "src/condor_tests/x_job_filexfer_testjob.pl")
	condor_pl_test(job_filexfer_merged-stdout_van "Vanilla: Transfer works with merged stdout/err" "filexfer;quick;ctest" CTEST DEPENDS "src/condor_tests/x_job_filexfer_testjob.pl")
	condor_pl_test(job_filexfer_md5-remote_van "Vanilla: Can we move 200 megs and not impact md5 checksum?" "filexfer;quick;ctest" CTEST DEPENDS "src/condor_tests/job_filexfer_md5-remote.pl")
	condor_pl_test(job_startd_rank_preempt "Does startd rank trump user priority?" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(cmd_wait_shows-all "" "quick;ctest" CTEST DEPENDS "${CMAKE_BINARY_DIR}/src/condor_tests/x_write_joblog_events.exe")
        add_dependencies_suffix_hack(cmd_wait_shows-all x_write_joblog_events.exe)
	condor_pl_test(cmd_wait_shows-base "" "quick;ctest" CTEST DEPENDS "${CMAKE_BINARY_DIR}/src/condor_tests/x_write_joblog_events.exe")
        add_dependencies_suffix_hack(cmd_wait_shows-base x_write_joblog_events.exe)
	condor_pl_test(cmd_wait_shows-notimeout "" "quick;ctest" CTEST DEPENDS "${CMAKE_BINARY_DIR}/src/condor_tests/x_write_joblog_events.exe")
        add_dependencies_suffix_hack(cmd_wait_shows-notimeout x_write_joblog_events.exe)
	condor_pl_test(cmd_wait_shows-partial "" "quick;ctest" CTEST DEPENDS "${CMAKE_BINARY_DIR}/src/condor_tests/x_write_joblog_events.exe")
        add_dependencies_suffix_hack(cmd_wait_shows-partial x_write_joblog_events.exe)
	condor_pl_test(cmd_wait_shows-timeout "" "quick;ctest" CTEST DEPENDS "${CMAKE_BINARY_DIR}/src/condor_tests/x_write_joblog_events.exe")
        add_dependencies_suffix_hack(cmd_wait_shows-timeout x_write_joblog_events.exe)
		condor_pl_test(job_dagman_fullremove "Ensure a removed dag is completely removed" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_fullremove.dag;src/condor_tests/job_dagman_fullremove.cmd;src/condor_tests/job_dagman_fullremove.pl;src/condor_tests/job_dagman_monitor.pl;${CMAKE_BINARY_DIR}/src/condor_tests/x_read_joblog.exe")
	condor_pl_test(job_dagman_basic "Basic one node dag" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_basic.dag;src/condor_tests/job_dagman_basic-node.cmd;src/condor_tests/x_dagman_node-ret.pl;src/condor_tests/x_dagman_premonitor.pl;src/condor_tests/job_dagman_monitor.pl")
	condor_pl_test(job_dagman_local "Test local universe under DAGMan" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_local.dag;src/condor_tests/job_dagman_local.cmd;src/condor_tests/job_dagman_local2.cmd")
	condor_pl_test(job_dagman_maxpostscripts "Check that throttling by post scripts works" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_maxpostscripts.dag;src/condor_tests/job_dagman_maxpostscripts.cmd;src/condor_tests/job_dagman_maxpostscripts-checkin.template;src/condor_tests/x_dagman_boring_prescript.pl;src/condor_tests/x_dagman_node-ret.pl;src/condor_tests/x_general_client.pl;src/condor_tests/x_general_server.pl;src/condor_tests/x_dagman_readflowlog.pl")
	condor_pl_test(job_dagman_maxprescripts "Check that throttling by pre scripts works" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_maxprescripts.dag;src/condor_tests/job_dagman_maxprescripts.cmd;src/condor_tests/job_dagman_maxprescripts-node.cmd;src/condor_tests/job_dagman_maxprescripts-checkin.template;src/condor_tests/x_dagman_node-ret.pl;src/condor_tests/job_dagman_monitor.pl;src/condor_tests/x_general_server.pl;src/condor_tests/x_general_client.pl;src/condor_tests/x_dagman_readflowlog.pl")

	condor_pl_test(job_dagman_prepost "Run a dag and verify each nodes pre and post scripts ran" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_prepost.dag;src/condor_tests/job_dagman_prepost-node.cmd;src/condor_tests/job_dagman_prepost-prehello.template;src/condor_tests/job_dagman_prepost-posthello.template;src/condor_tests/x_dagman_node-ret.pl;src/condor_tests/job_dagman_monitor.pl;src/condor_tests/x_general_server.pl;src/condor_tests/x_general_client.pl;src/condor_tests/x_dagman_readflowlog.pl;src/condor_tests/x_dagman_readprepostlog.pl;${CMAKE_BINARY_DIR}/src/condor_tests/x_read_joblog.exe")

	condor_pl_test(job_dagman_retry "Make sure a retry dag runs correctly." "dagman;quick;ctest")
	condor_pl_test(job_dagman_throttle "Test Dagman throttling ability" "dagman;long")

	condor_pl_test(job_dagman_unlessexit "Test that the retries can be stopped with particular exit values from node." "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_unlessexit.cmd;src/condor_tests/job_dagman_unlessexit.dag;src/condor_tests/x_dagman_node-ret.pl;src/condor_tests/x_dagman_retry-monitor.pl")
	condor_pl_test(job_dagman_abort-A "Test ABORT-DAG-ON" "dagman" )# not in "quick" CTEST DEPENDS "src/condor_tests/job_dagman_abort-A.dag;src/condor_tests/job_dagman_abort-A-node-fail.cmd;src/condor_tests/job_dagman_abort-A-node-succeed.cmd;src/condor_tests/x_dagman_retry-monitor.pl")
	condor_pl_test(job_dagman_parallel-A "Test DAGMan handling nodes w/ multiple jobs" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_parallel-A.dag;src/condor_tests/job_dagman_parallel-A-node.cmd")
	condor_pl_test(job_dagman_subdag-A "Test condor_submit_dag recursion on nested DAGs, SUBDAG EXTERNAL keyword" "dagman;quick;ctest" CTEST DEPENDS
"src/condor_tests/job_dagman_subdag-A-lower1.dag;src/condor_tests/job_dagman_subdag-A-lower1-node.cmd;src/condor_tests/job_dagman_subdag-A-lower2.dag;src/condor_tests/job_dagman_subdag-A-lower2-node.cmd;src/condor_tests/job_dagman_subdag-A-lowerlower1.dag;src/condor_tests/job_dagman_subdag-A-lowerlower1-node.cmd;src/condor_tests/job_dagman_subdag-A-lower-nodeA-post.pl;src/condor_tests/job_dagman_subdag-A-lower-nodeA-pre.pl;src/condor_tests/job_dagman_subdag-A-upper.dag;src/condor_tests/job_dagman_subdag-A-upper-node.cmd")
#   GGT  debug this one later in new batlab
#	condor_pl_test(job_dagman_subdag-AE "Test condor_submit_dag recursion on nested DAGs, SUBDAG EXTERNAL keyword, eager .condor.sub generation" "dagman;quick;ctest")
condor_pl_test(job_dagman_subdag_in_splice-A "Test a sub-DAG inside a splice -- only works with lazy submit file generation" "dagman;quick;ctest" CTEST DEPENDS
	"src/condor_tests/job_dagman_subdag_in_splice-A-lower.dag;src/condor_tests/job_dagman_subdag_in_splice-A-lowerlower.dag;src/condor_tests/job_dagman_subdag_in_splice-A-lowerlower-node.cmd;src/condor_tests/job_dagman_subdag_in_splice-A-lower-nodeA-post.pl;src/condor_tests/job_dagman_subdag_in_splice-A-lower-nodeA-pre.pl;src/condor_tests/job_dagman_subdag_in_splice-A-lower-node.cmd;src/condor_tests/job_dagman_subdag_in_splice-A-upper.dag;src/condor_tests/job_dagman_subdag_in_splice-A-upper-node.cmd")
	condor_pl_test(job_dagman_pre_subdag-A "Test sub-DAG generated by PRE script" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_pre_subdag-A-node.cmd;src/condor_tests/job_dagman_pre_subdag-A-pre.pl;src/condor_tests/job_dagman_pre_subdag-A-upper.dag")
	condor_pl_test(job_dagman_splice-cat "Test DAG splice/node category" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_splice-cat.cmd;src/condor_tests/job_dagman_splice-cat.dag;src/condor_tests/job_dagman_splice-cat-splice1.dag;src/condor_tests/job_dagman_splice-cat-splice2.dag")
	condor_pl_test(job_dagman_node_prio "Test DAG node priorities" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_node_prio.cmd;src/condor_tests/job_dagman_node_prio.config;src/condor_tests/job_dagman_node_prio.dag;src/condor_tests/job_dagman_node_prio-nodeA.cmd;src/condor_tests/job_dagman_node_prio-nodeA.pl;src/condor_tests/job_dagman_node_prio.pl")
	condor_pl_test(job_dagman_depth_first "Make sure DAGMan depth-first traversal works" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_depth_first.config;src/condor_tests/job_dagman_depth_first.dag;src/condor_tests/job_dagman_depth_first-nodeA.cmd;src/condor_tests/job_dagman_depth_first-nodeA.pl;src/condor_tests/job_dagman_depth_first-nodeB.cmd;src/condor_tests/job_dagman_depth_first-nodeB.pl;src/condor_tests/job_dagman_depth_first-nodeC.cmd")
	condor_pl_test(job_dagman_log_path "Test DAG tricky log file paths" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_log_path.dag;src/condor_tests/job_dagman_log_path-nodeA.cmd;src/condor_tests/job_dagman_log_path-nodeB.cmd;src/condor_tests/job_dagman_log_path-nodeC.cmd;src/condor_tests/job_dagman_log_path-nodeD.cmd;src/condor_tests/job_dagman_log_path-node.pl")
	condor_pl_test(job_dagman_retry_recovery "Test DAG recovery w/ retried node" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_retry_recovery.config;src/condor_tests/job_dagman_retry_recovery.dag;src/condor_tests/job_dagman_retry_recovery-nodeA.cmd;src/condor_tests/job_dagman_retry_recovery-nodeA.pl;src/condor_tests/job_dagman_retry_recovery-nodeB2.cmd;src/condor_tests/job_dagman_retry_recovery-nodeB2.pl;src/condor_tests/job_dagman_retry_recovery-nodeB2-post.pl;src/condor_tests/job_dagman_retry_recovery-nodeB2-pre.pl;src/condor_tests/job_dagman_retry_recovery-nodeB.cmd;src/condor_tests/job_dagman_retry_recovery-nodeB.pl;src/condor_tests/job_dagman_retry_recovery-nodeC.cmd;src/condor_tests/job_dagman_retry_recovery-nodeC.pl")
	condor_pl_test(job_dagman_submit_fails_post "Test submit failure on node w/ POST" "dagman;quick;ctest" CTEST DEPENDS src/condor_tests/job_dagman_submit_fails_post.config;src/condor_tests/job_dagman_submit_fails_post.dag;src/condor_tests/job_dagman_submit_fails_post-nodeA.cmd;src/condor_tests/job_dagman_submit_fails_post-nodeA-post.pl)
	condor_pl_test(job_dagman_usedagdir "Test DAG with -usedagdir flag" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_usedagdir-subdir")
	condor_pl_test(job_dagman_node_dir "Test DAG with nodes in subdirectories" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_node_dir.dag;src/condor_tests/job_dagman_node_dir-node.cmd;src/condor_tests/job_dagman_node_dir-node.pl;src/condor_tests/job_dagman_node_dir-post.pl;src/condor_tests/job_dagman_node_dir-pre.pl;src/condor_tests/job_dagman_node_dir-subdir")
	condor_pl_test(job_dagman_default_log "Test DAG with default node log file" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_default_log.config;src/condor_tests/job_dagman_default_log-nodeB1.cmd;src/condor_tests/job_dagman_default_log-nodeB.pl;src/condor_tests/job_dagman_default_log-nodeB-pre.pl;src/condor_tests/job_dagman_default_log-nodeC.pl;src/condor_tests/job_dagman_default_log-subdir;src/condor_tests/job_dagman_default_log.dag;src/condor_tests/job_dagman_default_log-nodeB2.cmd;src/condor_tests/job_dagman_default_log-nodeB-post.pl;src/condor_tests/job_dagman_default_log-nodeC.cmd")
	condor_pl_test(job_dagman_recovery_event_check "Test DAG bad events in recovery mode" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_recovery_event_check.config;src/condor_tests/job_dagman_recovery_event_check-nodeA.cmd;src/condor_tests/job_dagman_recovery_event_check-nodeB.pl;src/condor_tests/job_dagman_recovery_event_check-nodeC.cmd;src/condor_tests/job_dagman_recovery_event_check.dag;src/condor_tests/job_dagman_recovery_event_check-nodeA.pl;src/condor_tests/job_dagman_recovery_event_check-nodeB-post.pl;src/condor_tests/job_dagman_recovery_event_check-nodeC.pl;src/condor_tests/job_dagman_recovery_event_check.log.dummy;src/condor_tests/job_dagman_recovery_event_check-nodeB.cmd;src/condor_tests/job_dagman_recovery_event_check-nodeB-pre.pl")
	condor_pl_test(job_dagman_recovery_preskip "Test DAG recovery with pre skip" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_recovery_preskip.config;src/condor_tests/job_dagman_recovery_preskip-nodeA.pl;src/condor_tests/job_dagman_recovery_preskip-nodeB-post.pl;src/condor_tests/job_dagman_recovery_preskip-nodeC.pl;src/condor_tests/job_dagman_recovery_preskip.dag;src/condor_tests/job_dagman_recovery_preskip-nodeB.cmd;src/condor_tests/job_dagman_recovery_preskip-nodeB-pre.pl;src/condor_tests/job_dagman_recovery_preskip-nodeA.cmd;src/condor_tests/job_dagman_recovery_preskip-nodeB.pl;src/condor_tests/job_dagman_recovery_preskip-nodeC.cmd")
	condor_pl_test(job_dagman_abnormal_term_recovery_retries "Test DAGMan in recovery mode on abnormally-terminated node job with retries" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_abnormal_term_recovery_retries.dag;src/condor_tests/job_dagman_abnormal_term_recovery_retries-nodeA.dummy_log;src/condor_tests/job_dagman_abnormal_term_recovery_retries-nodeA.cmd;src/condor_tests/job_dagman_abnormal_term_recovery_retries-nodeA.pl")
	condor_pl_test(job_dagman_multi_dag "Test DAGMan with multiple DAG files on the command line" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_multi_dag-dag1.dag;src/condor_tests/job_dagman_multi_dag-dag2.dag;src/condor_tests/job_dagman_multi_dag-node.cmd;src/condor_tests/job_dagman_multi_dag-node.pl;src/condor_tests/job_dagman_multi_dag-post.pl;src/condor_tests/job_dagman_multi_dag-pre.pl")
	condor_pl_test(job_dagman_rescue-A "Test DAGMan with rescue DAG" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_rescue-A-lower.dag;src/condor_tests/job_dagman_rescue-A-nodeD.cmd;src/condor_tests/job_dagman_rescue-A-nodeLA.cmd;src/condor_tests/job_dagman_rescue-A-node.pl;src/condor_tests/job_dagman_rescue-A-nodeUA.pl;src/condor_tests/job_dagman_rescue-A-upper.dag;src/condor_tests/job_dagman_rescue-A-node.cmd;src/condor_tests/job_dagman_rescue-A-nodeD.pl;src/condor_tests/job_dagman_rescue-A-nodeLA.pl;src/condor_tests/job_dagman_rescue-A-nodeUA.cmd")
	condor_pl_test(job_dagman_rescue_recov "Test DAGMan with rescue DAG/recovery mode combination" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_rescue_recov-lower.dag;src/condor_tests/job_dagman_rescue_recov-node_conditional.pl;src/condor_tests/job_dagman_rescue_recov-node_hold-release.pl;src/condor_tests/job_dagman_rescue_recov-node_setup.pl;src/condor_tests/job_dagman_rescue_recov-upper.dag;src/condor_tests/job_dagman_rescue_recov-node_conditional.cmd;src/condor_tests/job_dagman_rescue_recov-node_hold-release.cmd;src/condor_tests/job_dagman_rescue_recov-node_setup.cmd")
	condor_pl_test(job_dagman_event_log "Test DAGMan with event log/POST script combination" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_event_log.dag;src/condor_tests/job_dagman_event_log-nodeA.cmd;src/condor_tests/job_dagman_event_log-node.cmd;src/condor_tests/job_dagman_event_log-node.pl;src/condor_tests/job_dagman_event_log-node-post.pl;src/condor_tests/job_dagman_event_log-node-pre.pl")
	condor_pl_test(job_dagman_vars "Test DAG VARS feature" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_vars-attribute.cmd;src/condor_tests/job_dagman_vars-node-newargs2.cmd;src/condor_tests/job_dagman_vars-retry.cmd;src/condor_tests/job_dagman_vars.dag;src/condor_tests/job_dagman_vars-node-newargs.cmd;src/condor_tests/job_dagman_vars-retry.pl;src/condor_tests/job_dagman_vars-node.cmd;src/condor_tests/job_dagman_vars-node.pl")
	condor_pl_test(job_dagman_vars-B "Test DAG VARS ALL_NODES feature" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_vars-B.dag;src/condor_tests/job_dagman_vars-B-inc.dag;src/condor_tests/job_dagman_vars-B.lower.dag;src/condor_tests/job_dagman_vars-B-node.pl;src/condor_tests/job_dagman_vars-B-node.sub")
	condor_pl_test(job_dagman_all_nodes "Test DAG ALL_NODES feature" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_all_nodes.dag;src/condor_tests/job_dagman_all_nodes.inc;src/condor_tests/job_dagman_all_nodes-splice.dag")
	condor_pl_test(job_dagman_vars_prepend "Test DAG VARS APPEND/PREPEND feature" "dagman;quick;ctest" CTEST)
	condor_pl_test(job_dagman_script_args "Test pre/post script arguments" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_script_args.dag;src/condor_tests/job_dagman_script_args-node.cmd;src/condor_tests/job_dagman_script_args-node.pl;src/condor_tests/job_dagman_script_args-script.pl")
	condor_pl_test(job_dagman_noop_node "Test noop nodes" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_noop_node-dummy.dag;src/condor_tests/job_dagman_noop_node-node_hold-release.pl;src/condor_tests/job_dagman_noop_node-subdir;src/condor_tests/job_dagman_noop_node-initialdir.cmd;src/condor_tests/job_dagman_noop_node-node_normal.pl;src/condor_tests/job_dagman_noop_node-upper.dag;src/condor_tests/job_dagman_noop_node-lower.dag;src/condor_tests/job_dagman_noop_node-node_setup.cmd;src/condor_tests/job_dagman_noop_node-withlog.cmd;src/condor_tests/job_dagman_noop_node-node_conditional.cmd;src/condor_tests/job_dagman_noop_node-node_setup.pl;src/condor_tests/job_dagman_noop_node-xmllog.cmd;src/condor_tests/job_dagman_noop_node-node_conditional.pl;src/condor_tests/job_dagman_noop_node-node_hold-release.cmd;src/condor_tests/job_dagman_noop_node-script.pl")
	condor_pl_test(job_dagman_node_name_illegal_chars "Test node names for illegal characters" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_node_name_illegal_chars.cmd;src/condor_tests/job_dagman_node_name_illegal_chars.dag")
	condor_pl_test(job_dagman_node_status "Test node status file" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_node_status.cmd;src/condor_tests/job_dagman_node_status.dag;src/condor_tests/job_dagman_node_status.pl")
	condor_pl_test(job_dagman_node_status_rm "Test node status file /w condor_rm" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_node_status_rm.dag;src/condor_tests/job_dagman_node_status_rm_node.pl;src/condor_tests/job_dagman_node_status_rm_node.cmd;src/condor_tests/job_dagman_node_status_rm_node.template")
	condor_pl_test(job_dagman_job_held "Test removing jobs held too many times" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_job_held.config;src/condor_tests/job_dagman_job_held.dag;src/condor_tests/job_dagman_job_held-nodeA.cmd;src/condor_tests/job_dagman_job_held-nodeB.cmd;src/condor_tests/job_dagman_job_held-node.pl")
	condor_pl_test(job_dagman_reject "Test the REJECT keyword, inside a splice" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_reject-0.dag;src/condor_tests/job_dagman_reject-1.dag;src/condor_tests/job_dagman_reject.cmd")
	condor_pl_test(job_dagman_jobstate_log "Test writing the jobstate.log file, including rescue DAG and recovery mode" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_jobstate_log.config;src/condor_tests/job_dagman_jobstate_log-nodeB.pl;src/condor_tests/job_dagman_jobstate_log-nodeD.pl;src/condor_tests/job_dagman_jobstate_log-upper-nodeA.cmd;src/condor_tests/job_dagman_jobstate_log.dag;src/condor_tests/job_dagman_jobstate_log-nodeC.cmd;src/condor_tests/job_dagman_jobstate_log-nodeE.cmd;src/condor_tests/job_dagman_jobstate_log-upper-nodeA.pl;src/condor_tests/job_dagman_jobstate_log-nodeA.cmd;src/condor_tests/job_dagman_jobstate_log-nodeC.pl;src/condor_tests/job_dagman_jobstate_log-post.pl;src/condor_tests/job_dagman_jobstate_log-wrapper.dag;src/condor_tests/job_dagman_jobstate_log-nodeA.pl;src/condor_tests/job_dagman_jobstate_log-nodeC-post.pl;src/condor_tests/job_dagman_jobstate_log-pre.pl;src/condor_tests/job_dagman_jobstate_log-nodeB.cmd;src/condor_tests/job_dagman_jobstate_log-nodeD.cmd")
	condor_pl_test(job_dagman_gt1957 "Test gittrac #1957 fix" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_gt1957.cmd;src/condor_tests/job_dagman_gt1957.dag;src/condor_tests/job_dagman_gt1957.testlog")
	#condor_pl_test(perf_busy_lynn_100_100_500_sched "Generated cpu loading performance test" "performance;long")
	condor_pl_test(job_dagman_subdag_multi_prohibit-A "Test multi setting prohibiting subdags A" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_subdag_multi_prohibit-A.config;src/condor_tests/job_dagman_subdag_multi_prohibit-A-nodeA.cmd;src/condor_tests/job_dagman_subdag_multi_prohibit-A-sub.dag;src/condor_tests/job_dagman_subdag_multi_prohibit-A.dag;src/condor_tests/job_dagman_subdag_multi_prohibit-A-subnode.cmd")
	condor_pl_test(job_dagman_subdag_multi_prohibit-B "Test multi setting prohibiting subdags B" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_subdag_multi_prohibit-B.config;src/condor_tests/job_dagman_subdag_multi_prohibit-B-nodeA.cmd;src/condor_tests/job_dagman_subdag_multi_prohibit-B-sub.dag;src/condor_tests/job_dagman_subdag_multi_prohibit-B.dag;src/condor_tests/job_dagman_subdag_multi_prohibit-B-subnode.cmd")
	condor_pl_test(job_dagman_subdag_multi_prohibit-C "Test multi setting prohibiting subdags C" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_subdag_multi_prohibit-C.config;src/condor_tests/job_dagman_subdag_multi_prohibit-C-nodeA.cmd;src/condor_tests/job_dagman_subdag_multi_prohibit-C-sub.dag;src/condor_tests/job_dagman_subdag_multi_prohibit-C.dag;src/condor_tests/job_dagman_subdag_multi_prohibit-C-subnode.cmd")
	condor_pl_test(job_dagman_lazy_submit_file "Test lazy submit file creation in DAGMan" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_lazy_submit_file.config;src/condor_tests/job_dagman_lazy_submit_file.dag;src/condor_tests/job_dagman_lazy_submit_file-pre.pl")
	condor_pl_test(job_dagman_retry-B "Test node retries in DAGMan" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_retry-B.config;src/condor_tests/job_dagman_retry-B-nodeA.pl;src/condor_tests/job_dagman_retry-B-nodeB.cmd;src/condor_tests/job_dagman_retry-B-nodeC.cmd;src/condor_tests/job_dagman_retry-B-nodeD.cmd;src/condor_tests/job_dagman_retry-B-nodeF-pre.pl;src/condor_tests/job_dagman_retry-B.dag;src/condor_tests/job_dagman_retry-B-nodeA-post.pl;src/condor_tests/job_dagman_retry-B-nodeB.pl;src/condor_tests/job_dagman_retry-B-nodeC.pl;src/condor_tests/job_dagman_retry-B-nodeD.pl;src/condor_tests/job_dagman_retry-B.run;src/condor_tests/job_dagman_retry-B-nodeA.cmd;src/condor_tests/job_dagman_retry-B-nodeA-pre.pl;src/condor_tests/job_dagman_retry-B-nodeB-pre.pl;src/condor_tests/job_dagman_retry-B-nodeC-post.pl;src/condor_tests/job_dagman_retry-B-nodeE-post.pl")
	condor_pl_test(job_dagman_propogate_priorities "Test propogation of priorities" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_propogate_priorities.config;src/condor_tests/job_dagman_propogate_priorities-lower1.dag;src/condor_tests/job_dagman_propogate_priorities-node.cmd;src/condor_tests/job_dagman_propogate_priorities.dag;src/condor_tests/job_dagman_propogate_priorities-lower2.dag")
	# turned off for now ticket #5076
	#condor_pl_test(job_dagman_use_hold_claim "Test holding of claims for DAGman" "dagman;quick;ctest")
	# turned off for now ticket #4386
	#condor_pl_test(job_dagman_halt-A "Test DAG halting" "dagman;quick;ctest")
	condor_pl_test(job_dagman_loglink "Test node job user logs as symlinks" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_loglink.cmd;src/condor_tests/job_dagman_loglink.dag")
	condor_pl_test(job_dagman_global_event_log-A "Test behavior in presence of global event log" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_global_event_log-A.cmd;src/condor_tests/job_dagman_global_event_log-A.dag;src/condor_tests/x_echostring.pl")
	condor_pl_test(job_dagman_global_event_log-B "Test behavior in presence of global event log" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_global_event_log-B.cmd;src/condor_tests/job_dagman_global_event_log-B.dag;src/condor_tests/x_echostring.pl")
	condor_pl_test(job_dagman_condor_restart "Make sure DAG runs survive across condor restart" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_condor_restart.dag;src/condor_tests/job_dagman_condor_restart-nodeA.cmd;src/condor_tests/job_dagman_condor_restart-nodeA.pl")
	condor_pl_test(job_dagman_suppress_notification "Make sure DAG suppresses notification" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_suppress_notification.cfg;src/condor_tests/job_dagman_suppress_notification.dag;src/condor_tests/job_dagman_suppress_notification-cmd_line.dag;src/condor_tests/job_dagman_suppress_notification-node.cmd;src/condor_tests/job_dagman_suppress_notification-subdag-cmd_line.dag;src/condor_tests/job_dagman_suppress_notification-cmd_line-node.cmd;src/condor_tests/job_dagman_suppress_notification-post-allow.pl;src/condor_tests/job_dagman_suppress_notification-subdag.dag;src/condor_tests/job_dagman_suppress_notification-post.pl")
	condor_pl_test(job_dagman_suppress_notification-cmd_line "Make sure DAG respects -dont_suppress_notification command line flag" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_suppress_notification-cmd_line.dag;src/condor_tests/job_dagman_suppress_notification-cmd_line-node.cmd;src/condor_tests/job_dagman_suppress_notification-post-allow.pl;src/condor_tests/job_dagman_suppress_notification-subdag-cmd_line.dag")
	condor_pl_test(job_dagman_classad "Test the DAGMan classad status update capability" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_classad.config;src/condor_tests/job_dagman_classad.dag;src/condor_tests/job_dagman_classad-node.cmd;src/condor_tests/job_dagman_classad-node.pl")
	# Turned off for now ticket #4387
	#condor_pl_test(job_dagman_repeat_holds "Test the suppression of repeat hold events" "dagman;quick;ctest")
	condor_pl_test(job_dagman_pegasus_recovery "Test recovery of Pegasus-generated sub-DAGs" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_pegasus_recovery.dag;src/condor_tests/job_dagman_pegasus_recovery-nodeA.pl;src/condor_tests/job_dagman_pegasus_recovery-nodeC.cmd;src/condor_tests/job_dagman_pegasus_recovery.dag.condor.sub_template;src/condor_tests/job_dagman_pegasus_recovery-nodeB.cmd;src/condor_tests/job_dagman_pegasus_recovery-nodeC.pl;src/condor_tests/job_dagman_pegasus_recovery-nodeA.cmd;src/condor_tests/job_dagman_pegasus_recovery-nodeB.pl")
	condor_pl_test(job_dagman_abort-B "Test DAG abort with failure" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_abort-B.cfg;src/condor_tests/job_dagman_abort-B.dag;src/condor_tests/job_dagman_abort-B-script.pl;src/condor_tests/x_dagman_retry-monitor.pl")

	# The following test has many random faiures
	#condor_pl_test(job_dagman_abort-final-A "Test DAG abort with final node" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_abort-final-A.dag;src/condor_tests/job_dagman_abort-final-A-nodeE-pre.pl;src/condor_tests/job_dagman_abort-final-A-node-sleep.pl;src/condor_tests/job_dagman_abort-final-A-nodeE.cmd;src/condor_tests/job_dagman_abort-final-A-node-fail.cmd;src/condor_tests/job_dagman_abort-final-A-node-succeed.cmd;src/condor_tests/job_dagman_abort-final-A-nodeE.pl;src/condor_tests/job_dagman_abort-final-A-node-sleep.cmd;src/condor_tests/x_echostring.pl;src/condor_tests/x_dagman_retry-monitor.pl")

	condor_pl_test(job_dagman_abort-final-B "Test DAG abort with final node" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_abort-final-B.dag;src/condor_tests/job_dagman_abort-final-B-nodeE.pl;src/condor_tests/job_dagman_abort-final-B-node-succeed.cmd;src/condor_tests/job_dagman_abort-final-B-script.pl;src/condor_tests/job_dagman_abort-final-B-nodeE.cmd;src/condor_tests/job_dagman_abort-final-B-node-fail.cmd;src/condor_tests/x_echostring.pl")
	condor_pl_test(job_dagman_cycle-A "Test DAG status file with cycle" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_cycle-A.dag;src/condor_tests/job_dagman_cycle-A-node.cmd;src/condor_tests/x_sleep.pl")
	condor_pl_test(job_dagman_rm "Test correct functionality of condor_rm of DAGMan job" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_rm.config;src/condor_tests/job_dagman_rm-nodeA.cmd;src/condor_tests/job_dagman_rm-nodeD.cmd;src/condor_tests/job_dagman_rm-nodeE.pl;src/condor_tests/job_dagman_rm-nodeF.pl;src/condor_tests/job_dagman_rm.dag;src/condor_tests/job_dagman_rm-nodeA.pl;src/condor_tests/job_dagman_rm-nodeE.cmd;src/condor_tests/job_dagman_rm-nodeF.cmd;src/condor_tests/job_dagman_rm-nodeZ.cmd;src/condor_tests/job_dagman_rm-wait.pl;src/condor_scripts/CondorTest.pm;src/condor_scripts/Condor.pm;src/condor_scripts/CondorUtils.pm;src/condor_scripts/CondorPersonal.pm")
	condor_pl_test(job_dagman_script_defer "Test DAG pre/post script deferral feature" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_script_defer_Apre.pl;src/condor_tests/job_dagman_script_defer_Cpost.pl;src/condor_tests/job_dagman_script_defer-node.cmd;src/condor_tests/job_dagman_script_defer-nodeD.pl;src/condor_tests/job_dagman_script_defer_Bpre.pl;src/condor_tests/job_dagman_script_defer.dag;src/condor_tests/job_dagman_script_defer-nodeD.cmd;src/condor_tests/x_sleep.pl")
	condor_pl_test(job_dagman_submit_glob-A "Test DAG with node jobs using new submit globbing feature" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_submit_glob-A-1.dat;src/condor_tests/job_dagman_submit_glob-A-nodeA.cmd;src/condor_tests/job_dagman_submit_glob-A-2.dat;src/condor_tests/job_dagman_submit_glob-A-nodeB.cmd;src/condor_tests/job_dagman_submit_glob-A-3.dat;src/condor_tests/job_dagman_submit_glob-A-nodeC.cmd;src/condor_tests/job_dagman_submit_glob-A.dag;src/condor_tests/job_dagman_submit_glob-A.run;src/condor_tests/x_cat.pl")
	condor_pl_test(job_dagman_submit_glob-B "Test DAG with node jobs using feature, in combination with DAGMAN_PROHIBIT_MULTI_JOBS" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_submit_glob-B-1.dat;src/condor_tests/job_dagman_submit_glob-B.config;src/condor_tests/job_dagman_submit_glob-B-nodeB.cmd;src/condor_tests/job_dagman_submit_glob-B-subA.dag;src/condor_tests/job_dagman_submit_glob-B-2.dat;src/condor_tests/job_dagman_submit_glob-B.dag;src/condor_tests/job_dagman_submit_glob-B-nodeC.cmd;src/condor_tests/job_dagman_submit_glob-B-subB.dag;src/condor_tests/job_dagman_submit_glob-B-3.dat;src/condor_tests/job_dagman_submit_glob-B-nodeA.cmd;src/condor_tests/job_dagman_submit_glob-B.run;src/condor_tests/job_dagman_submit_glob-B-subC.dag;src/condor_tests/x_cat.pl")
	condor_pl_test(job_dagman_set_attr "Test setting ClassAd attrs in DAG file" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_set_attr1.dag;src/condor_tests/job_dagman_set_attr-nodeB.cmd;src/condor_tests/job_dagman_set_attr.sub;src/condor_tests/job_dagman_set_attr2.dag;src/condor_tests/job_dagman_set_attr-node.pl;src/condor_tests/job_dagman_set_attr-nodeA.cmd")
	condor_pl_test(job_dagman_event_timestamp "Test event timestamps in dagman.out file" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_event_timestamp.dag;src/condor_tests/job_dagman_event_timestamp-node.cmd;src/condor_tests/x_echostring.pl")
	condor_pl_test(job_dagman_batch_name "Test setting & propagation of batch-name" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_batch_name-lower.dag;src/condor_tests/job_dagman_batch_name-node.sub;src/condor_tests/job_dagman_batch_name-upper.dag;src/condor_tests/x_echostring.pl")
	condor_pl_test(job_dagman_cmd_order "Test flexible command order" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_cmd_order.dag;src/condor_tests/job_dagman_cmd_order-lower.dag;src/condor_tests/job_dagman_cmd_order-node.sub;src/condor_tests/x_sleep.pl;src/condor_tests/x_echostring.pl")
	condor_pl_test(job_dagman_acct_grp "Test setting & propagation of accounting group and user" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_acct_grp-lower.dag;src/condor_tests/job_dagman_acct_grp-node.sub;src/condor_tests/job_dagman_acct_grp-upper.dag;src/condor_tests/x_echostring.pl")
	condor_pl_test(job_dagman_splice_connect-A "Test splice pin connections" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_splice_connect-A.dag;src/condor_tests/job_dagman_splice_connect-A-node.pl;src/condor_tests/job_dagman_splice_connect-A-splice1.dag;src/condor_tests/job_dagman_splice_connect-A-inc1.dag;src/condor_tests/job_dagman_splice_connect-A-node.sub;src/condor_tests/job_dagman_splice_connect-A-splice2.dag;src/condor_tests/job_dagman_splice_connect-A-inc2.dag;src/condor_tests/job_dagman_splice_connect-A-splice3.dag")
	condor_pl_test(job_dagman_splice_connect-B "Test splice pin connections" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_splice_connect-B-1.dag;src/condor_tests/job_dagman_splice_connect-B-2-2a.dag;src/condor_tests/job_dagman_splice_connect-B-3.dag;src/condor_tests/job_dagman_splice_connect-B-node.sub;src/condor_tests/job_dagman_splice_connect-B-2-1a.dag;src/condor_tests/job_dagman_splice_connect-B-2-2b.dag;src/condor_tests/job_dagman_splice_connect-B.dag;src/condor_tests/job_dagman_splice_connect-B-2-1b.dag;src/condor_tests/job_dagman_splice_connect-B-2.dag;src/condor_tests/job_dagman_splice_connect-B-node.pl")
	condor_pl_test(job_dagman_splice_connect-C "Test splice pin connections" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_splice_connect-C.dag;src/condor_tests/job_dagman_splice_connect-C-splice1.dag;src/condor_tests/job_dagman_splice_connect-C-splice2.dag;src/condor_tests/job_dagman_splice_connect-C-node.sub;src/condor_tests/x_echostring.pl")
	condor_pl_test(job_dagman_splice_connect-D "Test splice pin connections" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_splice_connect-D.dag;src/condor_tests/job_dagman_splice_connect-D-1.dag;src/condor_tests/job_dagman_splice_connect-D-2.dag;src/condor_tests/x_echostring.pl")
	condor_pl_test(job_dagman_splice_connect-E "Test splice pin connections" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_splice_connect-E.dag;src/condor_tests/job_dagman_splice_connect-E-1.dag;src/condor_tests/job_dagman_splice_connect-E-2.dag")
	condor_pl_test(job_dagman_splice_connect-F "Test splice pin connections" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_splice_connect-F.dag;src/condor_tests/job_dagman_splice_connect-F-1.dag;src/condor_tests/job_dagman_splice_connect-F-2.dag")
	condor_pl_test(job_dagman_splice_connect-G "Test splice pin connections" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_splice_connect-G-1upper.dag;src/condor_tests/job_dagman_splice_connect-G-2upper.dag;src/condor_tests/job_dagman_splice_connect-G-1lowerA.dag;src/condor_tests/job_dagman_splice_connect-G-1lowerB.dag;src/condor_tests/job_dagman_splice_connect-G-2lowerA.dag;src/condor_tests/job_dagman_splice_connect-G-2lowerB.dag")
	condor_pl_test(job_dagman_suppress_log "Test suppressing node job log files" "dagman;quick;ctest" CTEST DEPENDS "src/condor_tests/job_dagman_suppress_log.dag;src/condor_tests/job_dagman_suppress_log.config;src/condor_tests/job_dagman_suppress_log-node.sub;src/condor_tests/job_dagman_suppress_log-node.pl" )
	#condor_pl_test(perf_jobs_sue_10_10_30_van "Generated jobs performance test" "performance;long")
	#condor_pl_test(perf_xfer_deb_10_10_30_van "Generated transfer performance test" "performance;long")
	condor_pl_test(job_filexfer_sandbox-empty_van "Are job sandboxes with bad permissions cleaned up?" "quick;ctest" CTEST DEPENDS "src/condor_tests/job_filexfer_sandbox-empty_van.pl")
	condor_pl_test(job_core_sh-loop_van "Simple vanilla job that loops for N seconds" "quick;ctest" CTEST DEPENDS "src/condor_tests/job_core_sh-loop_van.cmd;src/condor_tests/job_core_sh-loop_van.sh")
	condor_pl_test(job_core_bigenv_van "Vanilla test for enormous environment" "quick;ctest" CTEST DEPENDS "${CMAKE_BINARY_DIR}/src/condor_tests/job_core_bigenv.exe;src/condor_tests/job_core_bigenv_van.cmd")
	add_dependencies_suffix_hack(job_core_bigenv_van job_core_bigenv.exe)
	condor_pl_test(job_core_bigenv_sched "Scheduler test for enormous environment" "quick;ctest" CTEST DEPENDS "${CMAKE_BINARY_DIR}/src/condor_tests/job_core_bigenv.exe;src/condor_tests/job_core_bigenv_sched.cmd" )
	add_dependencies_suffix_hack(job_core_bigenv_sched job_core_bigenv.exe)
	condor_pl_test(lib_ccb_startd "Test of execute daemons using CCB." "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(lib_ccb_schedd "Test of submit daemons using CCB." "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(lib_ccb_schedd_privnet "Test of submit daemons using CCB in private net." "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(lib_job_router_local "Test of JobRouter routing to local universe." "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(job_router_basic "Basic test of JobRouter routing." "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(job_router_xform "Test of JobRouter transforms." "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(condor_view_classad_types "CONDOR_VIEW_CLASSAD_TYPES test" "quick;ctest" CTEST)
	condor_pl_test(job_partitionable_basic_van "Test basic partitionable slot capability" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(job_hgq_negfail_basic_van "Test basic Hierarchical Group Quota behavior" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(job_hgq_autoregroup_basic_van "Test Hierarchical Group Quota autoregroup and group sort" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(job_consumption_policies_basic_van "Test Consumption Policies and negotiator resource consumption" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(job_cp_cclim_acctgrp_basic_van "Test Consumption Policies with cc-limits and acct-groups" "quick;ctest" CTEST DEPENDS "src/condor_tests/x_sleep.pl")
	condor_pl_test(job_starter_script-A "Test starter pre and post scripts" "quick;ctest" CTEST DEPENDS "src/condor_tests/job_starter_script-A.cmd;src/condor_tests/job_starter_script-A-job.pl;src/condor_tests/job_starter_script-A-post.pl;src/condor_tests/job_starter_script-A-pre.pl")
  #condor_pl_test(condor_urlfetch_test "Tests tool condor_urlfetch" "quick;ctest")

if (WINDOWS)

# These helper binaries for the tests are built a bit different 
# than our daemon/tool executables, so we don't want the compiler flags
# for our other binaries to infect them.  Therefore, we build them
# in this usual way.
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/timed_cmd.exe
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/timed_cmd.cpp
			COMMAND cl /O1 /GS- ${CMAKE_CURRENT_SOURCE_DIR}/timed_cmd.cpp)
add_custom_target(timed_cmd_exe ALL DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/timed_cmd.cpp ${CMAKE_CURRENT_BINARY_DIR}/timed_cmd.exe tests)
set_target_properties(timed_cmd_exe PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${TEST_TARGET_DIR})

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/appendmsg.exe
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/appendmsg.cpp
			COMMAND cl /O1 /GS- ${CMAKE_CURRENT_SOURCE_DIR}/appendmsg.cpp)
add_custom_target(appendmsg_exe ALL DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/appendmsg.cpp ${CMAKE_CURRENT_BINARY_DIR}/appendmsg.exe tests)
set_target_properties(appendmsg_exe PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${TEST_TARGET_DIR})

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/sleep.exe
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/sleep.cpp
			COMMAND cl /O1 /GS- ${CMAKE_CURRENT_SOURCE_DIR}/sleep.cpp)
add_custom_target(sleep_exe ALL DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/sleep.cpp ${CMAKE_CURRENT_BINARY_DIR}/sleep.exe tests)
set_target_properties(sleep_exe PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${TEST_TARGET_DIR})

# Note the additional dependency on sleep.exe [sic] below -- as both executables
# share the same source file, if we build them in parallel, we collide on
# the sleep.obj file, so add this otherwise unused dependency to serialize 
# the builds and avoid the collision
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/sleepw.exe
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/sleep.cpp
			COMMAND cl /O1 /GS- /DNOCONSOLE=1 /Fesleepw.exe ${CMAKE_CURRENT_SOURCE_DIR}/sleep.cpp)
add_custom_target(sleepw_exe ALL DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/sleep.cpp ${CMAKE_CURRENT_BINARY_DIR}/sleepw.exe sleep_exe tests)
set_target_properties(sleepw_exe PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${TEST_TARGET_DIR})

endif()

endif(BUILD_TESTING)
