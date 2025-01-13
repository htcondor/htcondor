/***************************************************************
 *
 * Copyright (C) 1990-2024, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *	http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


#include "condor_common.h"
#include "condor_config.h"
#include "condor_daemon_core.h"
#include "subsystem_info.h"
#include "basename.h"
#include "setenv.h"
#include "dag.h"
#include "debug.h"
#include "parse.h"
#include "my_username.h"
#include "condor_environ.h"
#include "dagman_main.h"
#include "condor_getcwd.h"
#include "condor_version.h"
#include "dagman_metrics.h"
#include "directory.h"

namespace deep = DagmanDeepOptions;
namespace shallow = DagmanShallowOptions;
namespace conf = DagmanConfigOptions;

void ExitSuccess();

static Dagman dagman;

strict_level_t Dagman::_strict = DAG_STRICT_1;

DagmanUtils dagmanUtils(DEBUG_MSG_STREAM::DEBUG_LOG);

//---------------------------------------------------------------------------
static void Usage() {
	std::string outFile = "<DAG File>.lib.out";
	if (dagman.options.isMultiDag()) {
		std::string primaryDag = dagman.options.primaryDag();
		formatstr(outFile, "%s.lib.out", primaryDag.c_str());
	}
	debug_printf(DEBUG_SILENT, "To view condor_dagman usage look at %s\n", outFile.c_str());
	fprintf(stdout, "Usage: condor_dagman -p 0 -f -l . -Dag <NAME.dag> -Lockfile <NAME.dag.lock> -CsdVersion <version string>\n");
	fprintf(stdout, "\t[-Help]\n"
	                "\t[-Version]\n");
	dagmanUtils.DisplayDAGManOptions("\t[%s]\n", DagOptionSrc::DAGMAN_MAIN);
	fprintf(stdout, "Where NAME is the name of your DAG file.\n"
	                "Default -Debug is -Debug %d\n", DEBUG_VERBOSE);
	DC_Exit(EXIT_ERROR);
}

// In Config() we get DAGMan-related configuration values.  This
// is a three-step process:
// 1. Get the name of the DAGMan-specific config file (if any).
// 2. If there is a DAGMan-specific config file, process it so
//    that its values are added to the configuration.
// 3. Get the values we want from the configuration.
bool Dagman::Config() {
	// Note: debug_printfs are DEBUG_NORMAL here because when we
	// get here we haven't processed command-line arguments yet.

	// Get and process the DAGMan-specific config file (if any)
	// before getting any of the other parameters.
	param(config[conf::str::DagConfig], "DAGMAN_CONFIG_FILE");
	if ( ! config[conf::str::DagConfig].empty()) {
		debug_printf(DEBUG_NORMAL, "Using DAGMan config file: %s\n", config[conf::str::DagConfig].c_str());
		// We do this test here because the corresponding error
		// message from the config code doesn't show up in dagman.out.
		if (access(config[conf::str::DagConfig].c_str(), R_OK) != 0 && !is_piped_command(config[conf::str::DagConfig].c_str())) {
			debug_printf(DEBUG_QUIET, "ERROR: Can't read DAGMan config file: %s\n",
			             config[conf::str::DagConfig].c_str());
			DC_Exit(EXIT_ERROR);
		}
		process_config_source(config[conf::str::DagConfig].c_str(), 0, "DAGMan config", nullptr, true);
	}

	_strict = (strict_level_t)param_integer("DAGMAN_USE_STRICT", _strict, DAG_STRICT_0, DAG_STRICT_3);
	debug_printf(DEBUG_NORMAL, "DAGMAN_USE_STRICT setting: %d\n", _strict);

	options[shallow::i::DebugLevel] = (debug_level_t)param_integer("DAGMAN_VERBOSITY", debug_level, DEBUG_SILENT, DEBUG_DEBUG_4);
	debug_printf(DEBUG_NORMAL, "DAGMAN_VERBOSITY setting: %d\n", options[shallow::i::DebugLevel]);

	config[conf::i::DebugCacheSize] = param_integer("DAGMAN_DEBUG_CACHE_SIZE", (1024*1024)*5, 0, INT_MAX);
	debug_printf(DEBUG_NORMAL, "DAGMAN_DEBUG_CACHE_SIZE setting: %d\n", config[conf::i::DebugCacheSize]);

	config[conf::b::CacheDebug] = param_boolean("DAGMAN_DEBUG_CACHE_ENABLE", false);
	debug_printf(DEBUG_NORMAL, "DAGMAN_DEBUG_CACHE_ENABLE setting: %s\n", config[conf::b::CacheDebug] ? "True" : "False");

	config[conf::i::SubmitDelay] = param_integer("DAGMAN_SUBMIT_DELAY", 0, 0);
	debug_printf(DEBUG_NORMAL, "DAGMAN_SUBMIT_DELAY setting: %d\n", config[conf::i::SubmitDelay]);

	config[conf::i::MaxSubmitAttempts] = param_integer("DAGMAN_MAX_SUBMIT_ATTEMPTS", 6, 1, 16);
	debug_printf(DEBUG_NORMAL, "DAGMAN_MAX_SUBMIT_ATTEMPTS setting: %d\n", config[conf::i::MaxSubmitAttempts]);

	config[conf::b::DetectCycle] = param_boolean("DAGMAN_STARTUP_CYCLE_DETECT", false);
	debug_printf(DEBUG_NORMAL, "DAGMAN_STARTUP_CYCLE_DETECT setting: %s\n", config[conf::b::DetectCycle] ? "True" : "False");

	config[conf::b::UseJoinNodes] = param_boolean("DAGMAN_USE_JOIN_NODES", true);
	debug_printf(DEBUG_NORMAL, "DAGMAN_USE_JOIN_NODES setting: %s\n", config[conf::b::UseJoinNodes] ? "True" : "False");

	config[conf::b::ReportGraphMetrics] = param_boolean("DAGMAN_REPORT_GRAPH_METRICS", false);
	debug_printf(DEBUG_NORMAL, "DAGMAN_REPORT_GRAPH_METRICS setting: %s\n", config[conf::b::ReportGraphMetrics] ? "True" : "False");

	config[conf::i::SubmitsPerInterval] = param_integer("DAGMAN_MAX_SUBMITS_PER_INTERVAL", MAX_SUBMITS_PER_INT_DEFAULT, 1, 1000);
	debug_printf(DEBUG_NORMAL, "DAGMAN_MAX_SUBMITS_PER_INTERVAL setting: %d\n", config[conf::i::SubmitsPerInterval]);

	config[conf::b::AggressiveSubmit] = param_boolean("DAGMAN_AGGRESSIVE_SUBMIT", false);
	debug_printf(DEBUG_NORMAL, "DAGMAN_AGGRESSIVE_SUBMIT setting: %s\n", config[conf::b::AggressiveSubmit] ? "True" : "False");

	config[conf::i::LogScanInterval] = param_integer("DAGMAN_USER_LOG_SCAN_INTERVAL", LOG_SCAN_INT_DEFAULT, 1, INT_MAX);
	debug_printf(DEBUG_NORMAL, "DAGMAN_USER_LOG_SCAN_INTERVAL setting: %d\n", config[conf::i::LogScanInterval]);

	config[conf::dbl::ScheddUpdateInterval] = param_double("DAGMAN_QUEUE_UPDATE_INTERVAL", 120, 1, std::numeric_limits<double>::max());
	debug_printf(DEBUG_NORMAL, "DAGMAN_QUEUE_UPDATE_INTERVAL setting: %lf\n", config[conf::dbl::ScheddUpdateInterval]);

	options[shallow::i::Priority] = param_integer("DAGMAN_DEFAULT_PRIORITY", 0, INT_MIN, INT_MAX, false);
	debug_printf(DEBUG_NORMAL, "DAGMAN_DEFAULT_PRIORITY setting: %d\n", options[shallow::i::Priority]);

	options[deep::b::SuppressNotification] = param_boolean("DAGMAN_SUPPRESS_NOTIFICATION", false);
	debug_printf(DEBUG_NORMAL, "DAGMAN_SUPPRESS_NOTIFICATION setting: %s\n",
	             options[deep::b::SuppressNotification] ? "True" : "False");

	// We want to default to allowing the terminated/aborted
	// combination (that's what we've defaulted to in the past).
	// Okay, we also want to allow execute before submit because
	// we've run into that, and since DAGMan doesn't really care
	// about the execute events, it shouldn't abort the DAG.
	// And we further want to allow two terminated events for a
	// single job because people are seeing that with Globus
	// jobs!!
	int allow_events = CheckEvents::ALLOW_TERM_ABORT |
	               CheckEvents::ALLOW_EXEC_BEFORE_SUBMIT |
	               CheckEvents::ALLOW_DOUBLE_TERMINATE |
	               CheckEvents::ALLOW_DUPLICATE_EVENTS;

	// Now get the new DAGMAN_ALLOW_EVENTS value -- that can override
	// all of the previous stuff.
	config[conf::i::AllowEvents] = param_integer("DAGMAN_ALLOW_EVENTS", allow_events);
	debug_printf(DEBUG_NORMAL, "allow_events (DAGMAN_ALLOW_EVENTS) setting: %d\n", config[conf::i::AllowEvents]);

	config[conf::b::RetrySubmitFirst] = param_boolean("DAGMAN_RETRY_SUBMIT_FIRST", true);
	debug_printf(DEBUG_NORMAL, "DAGMAN_RETRY_SUBMIT_FIRST setting: %s\n", config[conf::b::RetrySubmitFirst] ? "True" : "False");

	config[conf::b::RetryNodeFirst] = param_boolean("DAGMAN_RETRY_NODE_FIRST", false);
	debug_printf(DEBUG_NORMAL, "DAGMAN_RETRY_NODE_FIRST setting: %s\n", config[conf::b::RetryNodeFirst] ? "True" : "False");

	options[shallow::i::MaxIdle] = param_integer("DAGMAN_MAX_JOBS_IDLE", MAX_IDLE_DEFAULT, 0, INT_MAX);
	debug_printf(DEBUG_NORMAL, "DAGMAN_MAX_JOBS_IDLE setting: %d\n", options[shallow::i::MaxIdle]);

	options[shallow::i::MaxJobs] = param_integer("DAGMAN_MAX_JOBS_SUBMITTED", 0, 0, INT_MAX);
	debug_printf(DEBUG_NORMAL, "DAGMAN_MAX_JOBS_SUBMITTED setting: %d\n", options[shallow::i::MaxJobs]);

	options[shallow::i::MaxPre] = param_integer("DAGMAN_MAX_PRE_SCRIPTS", 20, 0, INT_MAX);
	debug_printf(DEBUG_NORMAL, "DAGMAN_MAX_PRE_SCRIPTS setting: %d\n", options[shallow::i::MaxPre]);

	options[shallow::i::MaxPost] = param_integer("DAGMAN_MAX_POST_SCRIPTS", 20, 0, INT_MAX);
	debug_printf(DEBUG_NORMAL, "DAGMAN_MAX_POST_SCRIPTS setting: %d\n", options[shallow::i::MaxPost]);

	options[shallow::i::MaxHold] = param_integer( "DAGMAN_MAX_HOLD_SCRIPTS", 20, 0, INT_MAX);
	debug_printf(DEBUG_NORMAL, "DAGMAN_MAX_HOLD_SCRIPTS setting: %d\n", options[shallow::i::MaxHold]);

	config[conf::b::MungeNodeNames] = param_boolean("DAGMAN_MUNGE_NODE_NAMES", true);
	debug_printf(DEBUG_NORMAL, "DAGMAN_MUNGE_NODE_NAMES setting: %s\n", config[conf::b::MungeNodeNames] ? "True" : "False");

	config[conf::b::AllowIllegalChars] = param_boolean("DAGMAN_ALLOW_ANY_NODE_NAME_CHARACTERS", false);
	debug_printf(DEBUG_NORMAL, "DAGMAN_ALLOW_ANY_NODE_NAME_CHARACTERS setting: %s\n", config[conf::b::AllowIllegalChars] ? "True" : "False");

	config[conf::b::ProhibitMultiJobs] = param_boolean("DAGMAN_PROHIBIT_MULTI_JOBS", false);
	debug_printf( DEBUG_NORMAL, "DAGMAN_PROHIBIT_MULTI_JOBS setting: %s\n", config[conf::b::ProhibitMultiJobs] ? "True" : "False");

	config[conf::b::DepthFirst] = param_boolean("DAGMAN_SUBMIT_DEPTH_FIRST", false);
	debug_printf(DEBUG_NORMAL, "DAGMAN_SUBMIT_DEPTH_FIRST setting: %s\n", config[conf::b::DepthFirst] ? "True" : "False");

	options[shallow::b::PostRun] = param_boolean("DAGMAN_ALWAYS_RUN_POST", false);
	debug_printf(DEBUG_NORMAL, "DAGMAN_ALWAYS_RUN_POST setting: %s\n", options[shallow::b::PostRun] ? "True" : "False");

	param(config[conf::str::SubmitExe], "DAGMAN_CONDOR_SUBMIT_EXE", "condor_submit");
	debug_printf(DEBUG_NORMAL, "DAGMAN_CONDOR_SUBMIT_EXE setting: %s\n", config[conf::str::SubmitExe].c_str());

	param(config[conf::str::RemoveExe], "DAGMAN_CONDOR_RM_EXE", "condor_rm");
	debug_printf(DEBUG_NORMAL, "DAGMAN_CONDOR_RM_EXE setting: %s\n", config[conf::str::RemoveExe].c_str());

	// Currently only two submit options: condor_submit (0) and direct submit (1)
	options[deep::i::SubmitMethod] = (int)param_boolean("DAGMAN_USE_DIRECT_SUBMIT", true);
	debug_printf(DEBUG_NORMAL, "DAGMAN_USE_DIRECT_SUBMIT setting: %s\n", options[deep::i::SubmitMethod] ? "True" : "False");

	config[conf::b::ProduceJobCreds] = param_boolean("DAGMAN_PRODUCE_JOB_CREDENTIALS", true);
	debug_printf(DEBUG_NORMAL, "DAGMAN_PRODUCE_JOB_CREDENTIALS setting: %s\n", config[conf::b::ProduceJobCreds] ? "True" : "False");

	config[conf::b::RemoveTempSubFiles] = param_boolean("DAGMAN_REMOVE_TEMP_SUBMIT_FILES", true); // Undocumented on purpose

	config[conf::b::AppendVars] = param_boolean("DAGMAN_DEFAULT_APPEND_VARS", false);
	debug_printf(DEBUG_NORMAL, "DAGMAN_DEFAULT_APPEND_VARS setting: %s\n", config[conf::b::AppendVars] ? "True" : "False");

	param(config[conf::str::InheritAttrsPrefix], "DAGMAN_INHERIT_ATTRS_PREFIX");
	debug_printf(DEBUG_NORMAL, "DAGMAN_INHERIT_ATTRS_PREFIX setting: %s\n", config[conf::str::InheritAttrsPrefix].c_str());

	std::string inheritAttrsList;
	param(inheritAttrsList, "DAGMAN_INHERIT_ATTRS");
	debug_printf(DEBUG_NORMAL, "DAGMAN_INHERIT_ATTRS setting: %s\n", inheritAttrsList.c_str());
	for (const auto& attr : StringTokenIterator(inheritAttrsList)) {
		inheritAttrs.emplace(std::make_pair(config[conf::str::InheritAttrsPrefix] + attr, ""));
	}

	//DAGMan information to be added to node jobs job ads (expects comma seperated list)
	//Note: If more keywords/options added please update the config knob description accordingly
	//-Cole Bollig 2023-03-09
	std::string adInjectInfo;
	param(adInjectInfo, "DAGMAN_NODE_RECORD_INFO");
	if ( ! adInjectInfo.empty()) {
		debug_printf(DEBUG_NORMAL, "DAGMAN_NODE_RECORD_INFO: %s\n", adInjectInfo.c_str());
		for (const auto& info : StringTokenIterator(adInjectInfo)) {
			std::string mutable_info{info};
			trim(mutable_info);
			lower_case(mutable_info);
			if (mutable_info.compare("retry") == MATCH) {
				config[conf::b::JobInsertRetry] = true;
			}
		}
	}

	param(config[conf::str::MachineAttrs], "DAGMAN_RECORD_MACHINE_ATTRS");
	if ( ! config[conf::str::MachineAttrs].empty()) {
		debug_printf(DEBUG_NORMAL, "DAGMAN_RECORD_MACHINE_ATTRS: %s\n", config[conf::str::MachineAttrs].c_str());
		//Use machine attrs list to construct new job ad attributes to add to userlog
		bool firstAttr = true;
		for (auto& attr : StringTokenIterator(config[conf::str::MachineAttrs])) {
			if ( ! firstAttr) { config[conf::str::UlogMachineAttrs] += ","; }
			else { firstAttr = false; }
			config[conf::str::UlogMachineAttrs] += "MachineAttr" + attr + "0";
		}
		//Also add DAGNodeName to list of attrs to put in userlog event
		config[conf::str::UlogMachineAttrs] += ",DAGNodeName";
	}

	config[conf::b::AbortDuplicates] = param_boolean("DAGMAN_ABORT_DUPLICATES", true);
	debug_printf(DEBUG_NORMAL, "DAGMAN_ABORT_DUPLICATES setting: %s\n", config[conf::b::AbortDuplicates] ? "True" : "False");

	config[conf::b::AbortOnScarySubmit] = param_boolean("DAGMAN_ABORT_ON_SCARY_SUBMIT", true);
	debug_printf( DEBUG_NORMAL, "DAGMAN_ABORT_ON_SCARY_SUBMIT setting: %s\n", config[conf::b::AbortOnScarySubmit] ? "True" : "False");

	config[conf::i::PendingReportInverval] = param_integer("DAGMAN_PENDING_REPORT_INTERVAL", 600);
	debug_printf(DEBUG_NORMAL, "DAGMAN_PENDING_REPORT_INTERVAL setting: %d\n", config[conf::i::PendingReportInverval]);

	config[conf::i::VerifyScheddInterval] = param_integer("DAGMAN_CHECK_QUEUE_INTERVAL", 28'800);
	debug_printf(DEBUG_NORMAL, "DAGMAN_CHECK_QUEUE_INTERVAL setting: %d\n", config[conf::i::VerifyScheddInterval]);

	config[conf::i::JobStateTableInterval] = param_integer("DAGMAN_PRINT_JOB_TABLE_INTERVAL", 900, 0, INT_MAX);
	debug_printf(DEBUG_NORMAL, "DAGMAN_PRINT_JOB_TABLE_INTERVAL setting: %d\n", config[conf::i::JobStateTableInterval]);

	options[deep::i::AutoRescue] = (int)param_boolean("DAGMAN_AUTO_RESCUE", true);
	debug_printf(DEBUG_NORMAL, "DAGMAN_AUTO_RESCUE setting: %s\n", options[deep::i::AutoRescue] ? "True" : "False");
	
	config[conf::i::MaxRescueNum] = param_integer("DAGMAN_MAX_RESCUE_NUM", MAX_RESCUE_DAG_DEFAULT, 0, ABS_MAX_RESCUE_DAG_NUM);
	debug_printf(DEBUG_NORMAL, "DAGMAN_MAX_RESCUE_NUM setting: %d\n", config[conf::i::MaxRescueNum]);

	config[conf::b::PartialRescue] = param_boolean("DAGMAN_WRITE_PARTIAL_RESCUE", true);
	if ( ! config[conf::b::PartialRescue]) {
		debug_printf(DEBUG_NORMAL, "\n\nWARNING: DAGMan is configured to write Full Rescue DAG files.\n"
		                           "This is Deprecated and will be removed during V24 feature series of HTCondor!\n\n");
	}
	debug_printf(DEBUG_NORMAL, "DAGMAN_WRITE_PARTIAL_RESCUE setting: %s\n", config[conf::b::PartialRescue] ? "True" : "False");

	config[conf::b::RescueResetRetry] = param_boolean("DAGMAN_RESET_RETRIES_UPON_RESCUE", true);
	debug_printf(DEBUG_NORMAL, "DAGMAN_RESET_RETRIES_UPON_RESCUE setting: %s\n", config[conf::b::RescueResetRetry] ? "True" : "False");

	config[conf::i::MetricsVersion] = param_integer("DAGMAN_METRICS_FILE_VERSION", 2, 1, 2);
	debug_printf(DEBUG_NORMAL, "DAGMAN_METRICS_FILE_VERSION setting: %d\n", config[conf::i::MetricsVersion]);

	param(config[conf::str::NodesLog], "DAGMAN_DEFAULT_NODE_LOG", "@(DAG_DIR)/@(DAG_FILE).nodes.log");
	debug_printf(DEBUG_NORMAL, "DAGMAN_DEFAULT_NODE_LOG setting: %s\n", config[conf::str::NodesLog].c_str());

	config[conf::b::NfsLogError] = param_boolean("DAGMAN_LOG_ON_NFS_IS_ERROR", false);
	debug_printf(DEBUG_NORMAL, "DAGMAN_LOG_ON_NFS_IS_ERROR setting: %s\n", config[conf::b::NfsLogError] ? "True" : "False");

	config[conf::b::GenerateSubdagSubmit] = param_boolean("DAGMAN_GENERATE_SUBDAG_SUBMITS", true);
	debug_printf(DEBUG_NORMAL, "DAGMAN_GENERATE_SUBDAG_SUBMITS setting: %s\n", config[conf::b::GenerateSubdagSubmit] ? "True" : "False");

	config[conf::i::MaxJobHolds] = param_integer("DAGMAN_MAX_JOB_HOLDS", 100, 0, 1'000'000);
	debug_printf(DEBUG_NORMAL, "DAGMAN_MAX_JOB_HOLDS setting: %d\n", config[conf::i::MaxJobHolds]);

	config[conf::i::HoldClaimTime] = param_integer("DAGMAN_HOLD_CLAIM_TIME", 20, 0, 3600);
	debug_printf(DEBUG_NORMAL, "DAGMAN_HOLD_CLAIM_TIME setting: %d\n", config[conf::i::HoldClaimTime]);

	std::string debug;
	param(debug, "ALL_DEBUG");
	if ( ! debug.empty()) {
		debug_printf(DEBUG_NORMAL, "ALL_DEBUG setting: %s\n", debug.c_str());
	}

	debug.clear();
	param(debug, "DAGMAN_DEBUG");
	if ( ! debug.empty()) {
		debug_printf(DEBUG_NORMAL, "DAGMAN_DEBUG setting: %s\n", debug.c_str());
	}

	config[conf::b::SuppressJobLogs] = param_boolean("DAGMAN_SUPPRESS_JOB_LOGS", false);
	debug_printf(DEBUG_NORMAL, "DAGMAN_SUPPRESS_JOB_LOGS setting: %s\n", config[conf::b::SuppressJobLogs] ? "True" : "False");

	config[conf::b::RemoveJobs] = param_boolean("DAGMAN_REMOVE_NODE_JOBS", true);
	debug_printf(DEBUG_NORMAL, "DAGMAN_REMOVE_NODE_JOBS setting: %s\n", config[conf::b::RemoveJobs] ? "True" : "False");

	config[conf::b::HoldFailedJobs] = param_boolean("DAGMAN_PUT_FAILED_JOBS_ON_HOLD", false);
	debug_printf(DEBUG_NORMAL, "DAGMAN_PUT_FAILED_JOBS_ON_HOLD setting: %s\n", config[conf::b::HoldFailedJobs] ? "True" : "False");

	config[conf::b::EnforceNewJobLimits] = param_boolean("DAGMAN_REMOVE_JOBS_AFTER_LIMIT_CHANGE", false);
	debug_printf(DEBUG_NORMAL, "DAGMAN_REMOVE_JOBS_AFTER_LIMIT_CHANGE setting: %s\n", config[conf::b::EnforceNewJobLimits] ? "True" : "False");

	debug_printf(DEBUG_NORMAL, "DAGMAN will adjust edges after parsing\n");

	// enable up the debug cache if needed
	if (config[conf::b::CacheDebug]) {
		debug_cache_set_size(config[conf::i::DebugCacheSize]);
		debug_cache_enable();
	}

	return true;
}

void Dagman::LocateSchedd() {
	_schedd = new DCSchedd(nullptr, nullptr);
	if (!_schedd || !_schedd->locate()) {
		const char *errMsg = _schedd ? _schedd->error() : "?";
		debug_printf(DEBUG_QUIET, "WARNING: can't find address of local schedd for ClassAd updates (%s)\n",
		             errMsg );
		check_warning_strictness(DAG_STRICT_3);
	}
}


// NOTE: this is only called on reconfig, not at startup
void main_config() {
		// This is commented out because, even if we get new config
		// values here, they don't get passed to the Dag object (which
		// is where most of them actually take effect).  (See Gnats
		// PR 808.)  wenger 2007-02-09
	// dagman.Config();
}

// this is called by DC when the schedd is shutdown fast
void main_shutdown_fast() {
	if (dagman.dag) { dagman.dag->GetJobstateLog().WriteDagmanFinished(EXIT_RESTART); }
	// Don't report metrics here because we should restart.
	DC_Exit(EXIT_RESTART);
}

// this can be called by other functions, or by DC when the schedd is
// shutdown gracefully; this also gets called if condor_hold is done
// on the DAGMan job
void main_shutdown_graceful() {
	print_status(true);
	if (dagman.dag) {
		dagman.dag->DumpNodeStatus(true, false);
		dagman.dag->GetJobstateLog().WriteDagmanFinished(EXIT_RESTART);
	}
	// Don't report metrics here because we should restart.
	dagman.CleanUp();
	DC_Exit(EXIT_RESTART);
}

// Special case shutdown when the log file gets corrupted
void main_shutdown_logerror() {
	print_status();
	if (dagman.dag) {
		dagman.dag->DumpNodeStatus(true, false);
		dagman.dag->GetJobstateLog().WriteDagmanFinished(EXIT_ABORT);
		dagman.ReportMetrics(EXIT_ABORT);
	}
	dagman.CleanUp();
	DC_Exit(EXIT_ABORT);
}

void main_shutdown_rescue(int exitVal, DagStatus dagStatus,bool removeCondorJobs) {
	// Avoid possible infinite recursion if you hit a fatal error
	// while writing a rescue DAG.
	static bool inShutdownRescue = false;
	if (inShutdownRescue) { return;}

	inShutdownRescue = true;

	debug_printf(DEBUG_QUIET, "Aborting DAG...\n");
	// Avoid writing two different rescue DAGs if the "main" DAG and
	// the final node (if any) both fail.
	static bool wroteRescue = false;
	// If statement here in case failure occurred during parsing
	if (dagman.dag) {
		dagman.dag->_dagStatus = dagStatus;
		// We write the rescue DAG *before* removing jobs because
		// otherwise if we crashed, failed, or were killed while
		// removing them, we would leave the DAG in an
		// unrecoverable state...
		if (exitVal != 0) {
			if (dagman.config[conf::i::MaxRescueNum] > 0) {
				dagman.dag->Rescue(dagman.options.primaryDag().c_str(), dagman.options.isMultiDag(),
				                   dagman.config[conf::i::MaxRescueNum], wroteRescue, false,
				                   dagman.config[conf::b::PartialRescue]);
				wroteRescue = true;
			} else {
				debug_printf(DEBUG_QUIET, "No rescue DAG written because DAGMAN_MAX_RESCUE_NUM is 0\n");
			}
		}

		debug_printf(DEBUG_DEBUG_1, "We have %d running nodes to remove\n",
		             dagman.dag->NumNodesSubmitted());
		// We just go ahead and do a condor_rm here even if we don't
		// think we have any jobs running, because if we're aborting
		// because of DAGMAN_PROHIBIT_MULTI_JOBS getting triggered,
		// we may have jobs in the queue even if we think we don't.
		// (See gittrac #4960.) wenger 2015-04-22
		debug_printf(DEBUG_NORMAL, "Removing submitted jobs...\n");

		const char* rm_reason = "DAG Abort: DAG is exiting and writing rescue file.";
		if (dagStatus == DagStatus::DAG_STATUS_RM) {
			rm_reason = "DAG Removed: User removed scheduler job from queue.";
		}

		dagman.dag->RemoveRunningJobs(dagman.DAGManJobId, rm_reason, removeCondorJobs, false);

		if (dagman.dag->NumScriptsRunning() > 0) {
			debug_printf(DEBUG_NORMAL, "Removing running scripts...\n");
			dagman.dag->RemoveRunningScripts();
		}
		dagman.dag->PrintDeferrals(DEBUG_NORMAL, true);

			// Start the final node if we have one.
		if (dagman.dag->StartFinalNode()) {
			// We started a final node; return here so we wait for the
			// final node to finish, instead of exiting immediately.
			inShutdownRescue = false;
			return;
		}
		print_status(true);
		bool removed = (dagStatus == DagStatus::DAG_STATUS_RM);
		dagman.dag->DumpNodeStatus(false, removed);
		dagman.dag->GetJobstateLog().WriteDagmanFinished(exitVal);
		dagman.ReportMetrics(exitVal);
	}

	dagman.PublishStats();
	dagmanUtils.tolerant_unlink(dagman.options[shallow::str::LockFile]);
	dagman.CleanUp();
	inShutdownRescue = false;
	DC_Exit(exitVal);
}

// this gets called by DC when DAGMan receives a SIGUSR1 -- which,
// assuming the DAGMan submit file was properly written, is the signal
// the schedd will send if the DAGMan job is removed from the queue
int main_shutdown_remove(int) {
	debug_printf(DEBUG_QUIET, "Received SIGUSR1\n");
	// We don't remove Condor node jobs here because the schedd will
	// automatically remove them itself.
	main_shutdown_rescue(EXIT_ABORT, DagStatus::DAG_STATUS_RM, dagman.config[conf::b::RemoveJobs]);
	return FALSE;
}

void ExitSuccess() {
	print_status(true);
	dagman.dag->DumpNodeStatus(false, false);
	dagman.dag->GetJobstateLog().WriteDagmanFinished(EXIT_OKAY);
	dagman.ReportMetrics(EXIT_OKAY);
	dagman.PublishStats();
	dagmanUtils.tolerant_unlink(dagman.options[shallow::str::LockFile]);
	dagman.CleanUp();
	DC_Exit(EXIT_OKAY);
}

void condor_event_timer(int tid);

/****** FOR TESTING *******
int main_testing_stub( Service *, int ) {
	if( dagman.paused ) {
		ResumeDag(dagman);
	}
	else {
		PauseDag(dagman);
	}
	return true;
}
****** FOR TESTING ********/

//---------------------------------------------------------------------------
void main_init(int argc, char ** const argv) {
	printf("Executing condor dagman ... \n");

// We are seeing crashes in dagman after exit when global dtors 
// are being called after the classad cache map is destroyed,
// but only on Windows, probably because order of destruction
// is not defined.  
// Surely there must be better ways to fix this, but 
// for now, just turn off the cache on Windows for dagman.

#ifdef WIN32
	classad::ClassAdSetExpressionCaching(false);
#endif

	// process any config vars -- this happens before we process
	// argv[], since arguments should override config settings
	dagman.Config();

	dagman.LocateSchedd();

	dagman._protectedUrlMap = getProtectedURLMap();

	// get dagman job id from environment, if it's there (otherwise it will be set to "-1.-1.-1")
	dagman.DAGManJobId.SetFromString(getenv(ENV_CONDOR_ID));

	// The DCpermission (last parm) should probably be PARENT, if it existed
	daemonCore->Register_Signal(SIGUSR1, "SIGUSR1",
	                            main_shutdown_remove,
	                            "main_shutdown_remove");

	// Reclaim the working directory
	if (chdir(dagman.workingDir.c_str()) != 0) {
		debug_printf(DEBUG_NORMAL, "WARNING: Failed to change working directory to %s\n",
		             dagman.workingDir.c_str());
	}

/****** FOR TESTING *******
	daemonCore->Register_Signal( SIGUSR2, "SIGUSR2",
								  main_testing_stub,
								 "main_testing_stub", NULL);
****** FOR TESTING ********/

	// flag used if DAGMan is invoked with -WaitForDebug so we
	// wait for a developer to attach with a debugger...
	volatile int wait_for_debug = 0;

	// Version of this condor_dagman binary.
	CondorVersionInfo dagmanVersion;
	// Version of this condor_submit_dag binary.
	// Defaults to same version as condor_dagman, updated by input args
	std::string csdVersion = dagmanVersion.get_version_stdstring();

	debug_progname = condor_basename(argv[0]);

	DagmanOptions &dagOpts = dagman.options;

	for (int i = 0; i < argc; i++) {
		debug_printf(DEBUG_NORMAL, "argv[%d] == \"%s\"\n", i, argv[i]);
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		// Minimum legal version for a .condor.sub file to be compatible
		// with this condor_dagman binary.

		// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		// Be sure to change this if the arguments or environment
		// passed to condor_dagman change in an incompatible way!!
		// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

	struct DagVersionData {
		int majorVer;
		int minorVer;
		int subMinorVer;
	};
	const DagVersionData MIN_CSD_VERSION = { 7, 1, 2 };

		// Construct a string of the minimum submit file version.
	std::string minSubmitVersionStr;
	formatstr(minSubmitVersionStr, "%d.%d.%d",
	          MIN_CSD_VERSION.majorVer,
	          MIN_CSD_VERSION.minorVer,
	          MIN_CSD_VERSION.subMinorVer);
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	// Process command-line arguments
	for (size_t iArg = 1; iArg < (size_t)argc; iArg++) {
		std::string strArg = argv[iArg];

		if (strArg[0] != '-') {
			dagOpts.addDAGFile(strArg);
		} else {
			lower_case(strArg);
			if (strArg.find("-help") != std::string::npos || strArg.find("-h") != std::string::npos) { // -help
				Usage();
			} else if (strArg == "-waitfordebug") {
				wait_for_debug = 1;
			} else {
				std::string errMsg;
				if ( ! dagOpts.AutoParse(strArg, iArg, argc, argv, errMsg, &dagman.inheritOpts)) {
					fprintf(stderr, "%s\n", errMsg.c_str());
					Usage();
				}
			}
		}
	}

	dagman.CreateMetrics(); // Must be created post argument parsing

	dagman._dagmanClassad = new DagmanClassad(dagman.DAGManJobId, dagman._schedd);
	int parentDAGid = dagman._dagmanClassad->Initialize(dagOpts);

	dagman.metrics->SetParentDag(parentDAGid);

	if ( ! dagman.inheritAttrs.empty()) {
		std::string& prefix = dagman.config[conf::str::InheritAttrsPrefix];
		dagman._dagmanClassad->GetRequestedAttrs(dagman.inheritAttrs, (prefix.empty() ? nullptr : prefix.c_str()));
	}

	debug_level = (debug_level_t)dagOpts[shallow::i::DebugLevel];

	if ( ! dagOpts[shallow::str::CsdVersion].empty()) {
		csdVersion = dagOpts[shallow::str::CsdVersion];
	}

	// If no user specified BatchName and no ClassAd information set batchname = primary DAG
	if (dagOpts[deep::str::BatchName].empty()) {
		dagOpts[deep::str::BatchName] = dagOpts.primaryDag();
	}

	if ( ! dagOpts[shallow::str::SaveFile].empty()) {
		if (dagOpts[deep::i::DoRescueFrom] != 0 ) {
			debug_printf(DEBUG_SILENT, "Error: Cannot run DAG from both a save file and a specified rescue file.\n");
			Usage();
		}
		if (dagOpts[shallow::b::DoRecovery]) {
			debug_printf(DEBUG_SILENT, "Error: Cannot run DAG from a save file in recovery mode.\n");
			Usage();
		}
	}

	// We expect at the very least to have a dag filename specified
	// If not, show the Usage details and exit now.
	if (dagOpts.primaryDag().empty()) {
		debug_printf(DEBUG_SILENT, "No DAG file was specified\n");
		Usage();
	}

	dagman.ResolveDefaultLog();

	// Check the arguments...

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// Checking for version compatibility between the .condor.sub
	// file and this condor_dagman binary...

	// Note: if we're in recovery mode and the submit file version
	// causes us to quit, we leave any existing node jobs still
	// running -- may want to change that eventually.  wenger 2009-10-13.

	// Version of the condor_submit_dag that created our submit file.
	CondorVersionInfo submitFileVersion(csdVersion.c_str());

	// Just generate this message fragment in one place.
	std::string versionMsg;
	formatstr(versionMsg, "the version (%s) of this DAG's HTCondor submit file",
	          csdVersion.c_str());

	// Make sure version in submit file is valid.
	if ( ! submitFileVersion.is_valid()) {
		if ( ! dagOpts[deep::b::AllowVersionMismatch]) {
			debug_printf(DEBUG_QUIET, "Error: %s is invalid!\n", versionMsg.c_str());
			DC_Exit(EXIT_ERROR);
		} else {
			debug_printf(DEBUG_NORMAL,
			             "Warning: %s is invalid; continuing because of -AllowVersionMismatch flag\n",
			             versionMsg.c_str());
		}

	// Make sure .condor.sub file is recent enough.
	} else if (submitFileVersion.compare_versions(CondorVersion()) != 0) {
		if ( ! submitFileVersion.built_since_version(MIN_CSD_VERSION.majorVer,
		                                             MIN_CSD_VERSION.minorVer,
		                                             MIN_CSD_VERSION.subMinorVer))
		{
			if ( ! dagOpts[deep::b::AllowVersionMismatch]) {
				debug_printf(DEBUG_QUIET, "Error: %s is older than oldest permissible version (%s)\n",
				             versionMsg.c_str(), minSubmitVersionStr.c_str());
				DC_Exit(EXIT_ERROR);
			} else {
				debug_printf(DEBUG_NORMAL,
				            "Warning: %s is older than oldest permissible version (%s); continuing because of -AllowVersionMismatch flag\n",
				            versionMsg.c_str(), minSubmitVersionStr.c_str());
			}

		// Warn if .condor.sub file is a newer version than this binary.
		} else if (dagmanVersion.compare_versions(csdVersion.c_str()) > 0) {
			debug_printf(DEBUG_NORMAL, "Warning: %s is newer than condor_dagman version (%s)\n",
			             versionMsg.c_str(), CondorVersion());
			check_warning_strictness(DAG_STRICT_3);
		} else {
			debug_printf(DEBUG_NORMAL,
			             "Note: %s differs from condor_dagman version (%s), but the difference is permissible\n",
			             versionMsg.c_str(), CondorVersion());
		}
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// Set a few default values for environment variables and arguments that
	// might not have been provided.

	// If lockFileName not provided in arguments, set a default
	if (dagOpts[shallow::str::LockFile].empty()) {
		dagOpts[shallow::str::LockFile] = dagOpts.primaryDag() + ".lock";
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	if (dagOpts[shallow::i::MaxJobs] < 0) {
		debug_printf(DEBUG_SILENT, "-MaxJobs must be non-negative\n");
		Usage();
	}
	if (dagOpts[shallow::i::MaxIdle] < 0) {
		debug_printf(DEBUG_SILENT, "-MaxIdle must be non-negative\n");
		Usage();
	}
	if (dagOpts[shallow::i::MaxPre] < 0) {
		debug_printf(DEBUG_SILENT, "-MaxPre must be non-negative\n");
		Usage();
	}
	if (dagOpts[shallow::i::MaxPost] < 0) {
		debug_printf(DEBUG_SILENT, "-MaxPost must be non-negative\n");
		Usage();
	}
	if (dagOpts[shallow::i::MaxHold] < 0) {
		debug_printf(DEBUG_SILENT, "-MaxHold must be non-negative\n");
		Usage();
	}
	if (dagOpts[deep::i::DoRescueFrom] < 0) {
		debug_printf( DEBUG_SILENT, "-DoRescueFrom must be non-negative\n" );
		Usage();
	}

	// This is kind of a guess at adjusting
	// DAGMAN_MAX_SUBMITS_PER_INTERVAL and DAGMAN_USER_LOG_SCAN_INTERVAL
	// so that they don't goof up DAGMAN_MAX_JOBS_IDLE too much...
	// wenger 2015-11-05
	int maxIdle = dagOpts[shallow::i::MaxIdle];
	if (maxIdle != MAX_IDLE_DEFAULT) {
		debug_printf(DEBUG_QUIET,
		             "Note:  DAGMAN_MAX_JOBS_IDLE has been changed from the default setting; if your submit files create "
		             "multiple procs, you should probably set DAGMAN_MAX_SUBMITS_PER_INTERVAL to 1\n");

		int submitsLimit = MAX(1, maxIdle/10);
		if (dagman.config[conf::i::SubmitsPerInterval] > submitsLimit) {
			if (dagman.config[conf::i::SubmitsPerInterval] == MAX_SUBMITS_PER_INT_DEFAULT) {
				// The user hasn't changed DAGMAN_MAX_SUBMITS_PER_INTERVAL,
				// so change it to our best guess at something that will
				// work with DAGMAN_MAX_JOBS_IDLE.
				dagman.config[conf::i::SubmitsPerInterval] = submitsLimit;
				debug_printf(DEBUG_QUIET,
				             "Note:  DAGMAN_MAX_SUBMITS_PER_INTERVAL has been changed to %d because of your "
				             "DAGMAN_MAX_JOBS_IDLE setting of %d\n", dagman.config[conf::i::SubmitsPerInterval], maxIdle);
			} else {
				// If the user has changed this from the default, leave
				// their setting alone.
				debug_printf(DEBUG_QUIET,
				             "Warning: your DAGMAN_MAX_SUBMITS_PER_INTERVAL setting of %d may interfere with your "
				             "DAGMAN_MAX_JOBS_IDLE setting of %d\n", dagman.config[conf::i::SubmitsPerInterval], maxIdle);
				check_warning_strictness(DAG_STRICT_2);
			}
		}

		if (dagman.config[conf::i::LogScanInterval] > dagman.config[conf::i::SubmitsPerInterval]) {
			if (dagman.config[conf::i::LogScanInterval] == LOG_SCAN_INT_DEFAULT) {
				// The user hasn't changed DAGMAN_USER_LOG_SCAN_INTERVAL,
				// so change it to our best guess at something that will
				// work with DAGMAN_MAX_SUBMITS_PER_INTERVAL.
				dagman.config[conf::i::LogScanInterval] = dagman.config[conf::i::SubmitsPerInterval];
				debug_printf(DEBUG_QUIET,
				             "Note:  DAGMAN_USER_LOG_SCAN_INTERVAL has been changed to %d because of the "
				             "DAGMAN_MAX_SUBMITS_PER_INTERVAL setting of %d\n", dagman.config[conf::i::LogScanInterval],
				             dagman.config[conf::i::SubmitsPerInterval]);
			} else {
				// If the user has changed this from the default, leave
				// their setting alone.
				debug_printf(DEBUG_QUIET,
				             "Warning: your DAGMAN_USER_LOG_SCAN_INTERVAL setting of %d may interfere with the "
				             "DAGMAN_MAX_SUBMITS_PER_INTERVAL setting of %d\n", dagman.config[conf::i::LogScanInterval],
				             dagman.config[conf::i::SubmitsPerInterval] );
				check_warning_strictness(DAG_STRICT_2);
			}
		}
	}

	// ...done checking arguments.

	debug_printf(DEBUG_VERBOSE, "DAG Lockfile will be written to %s\n",
	             dagOpts[shallow::str::LockFile].c_str());
	if (dagOpts.isMultiDag()) {
		std::string msg = "DAG Input files are";
		for (const auto & dagFile : dagOpts.dagFiles()) {
			msg += " " + dagFile;
		}
		msg += "\n";
		debug_printf(DEBUG_VERBOSE, "%s", msg.c_str());
	} else {
		debug_printf(DEBUG_VERBOSE, "DAG Input file is %s\n",
		             dagOpts.primaryDag().c_str());
	}

	// if requested, wait for someone to attach with a debugger...
	while (wait_for_debug) { }

	{
		std::string cwd;
		if( ! condor_getcwd(cwd)) {
			cwd = "<null>";
		}
		debug_printf(DEBUG_DEBUG_1, "Current path is %s\n", cwd.c_str());

		char *temp = my_username();
		debug_printf(DEBUG_DEBUG_1, "Current user is %s\n", temp ? temp : "<null>");
		if (temp) { free( temp ); }
	}

	// Figure out the rescue DAG to run, if any (this is with "new-
	// style" rescue DAGs).
	int rescueDagNum = 0;
	std::string rescueDagMsg;

	if (dagOpts[deep::i::DoRescueFrom] != 0) {
		rescueDagNum = dagOpts[deep::i::DoRescueFrom];
		formatstr(rescueDagMsg, "Rescue DAG number %d specified", rescueDagNum);
		dagmanUtils.RenameRescueDagsAfter(dagOpts.primaryDag(), dagOpts.isMultiDag(), rescueDagNum, dagman.config[conf::i::MaxRescueNum]);

	} else if (dagOpts[deep::i::AutoRescue]) {
		rescueDagNum = dagmanUtils.FindLastRescueDagNum(dagOpts.primaryDag(), dagOpts.isMultiDag(), dagman.config[conf::i::MaxRescueNum]);
		formatstr(rescueDagMsg, "Found rescue DAG number %d", rescueDagNum);
	}

	dagman.metrics->SetRescueNum(rescueDagNum);

	// Create the DAG
	// Note: a bunch of the parameters we pass here duplicate things
	// in submitDagOpts, but I'm keeping them separate so we don't have to
	// bother to construct a new SubmitDagOtions object for splices.
	// wenger 2010-03-25
	dagman.dag = new Dag(dagman); /* toplevel dag! */

	if ( ! dagman.dag) { EXCEPT("ERROR: out of memory!"); }

	// Parse the input files.  The parse() routine
	// takes care of adding jobs and dependencies to the DagMan

	if ( ! dagOpts.isMultiDag()) { dagman.config[conf::b::MungeNodeNames] = false; }
	parseSetDoNameMunge(dagman.config[conf::b::MungeNodeNames]);
	debug_printf(DEBUG_VERBOSE, "Parsing %zu dagfiles\n", dagOpts.numDagFiles());

	// Here we make a copy of the dagFiles for iteration purposes. Deep inside
	// of the parsing, copies of the dagman.dagFile string list happen which
	// mess up the iteration of this list.
	str_list sl(dagOpts.dagFiles());
	for (const auto & file : sl) {
		debug_printf(DEBUG_VERBOSE, "Parsing %s ...\n", file.c_str());

		if( ! parse(dagman, dagman.dag, file.c_str())) {
			if (dagman.options[shallow::b::DumpRescueDag]) {
				// Dump the rescue DAG so we can see what we got
				// in the failed parse attempt.
				debug_printf(DEBUG_QUIET, "Dumping rescue DAG because of -DumpRescue flag\n");
				dagman.dag->Rescue(dagOpts.primaryDag().c_str(), dagOpts.isMultiDag(), dagman.config[conf::i::MaxRescueNum], false, true, false);
			}
			
			// I guess we're setting bForce to true here in case we're
			// in recovery mode and we have any leftover jobs from
			// before (e.g., user did condor_hold, modified DAG file
			// introducing a syntax error, and then did condor_release).
			// (wenger 2014-10-28)
			std::string rm_reason;
			formatstr(rm_reason, "Startup Error: DAGMan failed to parse DAG file (%s). Was likely in recovery mode.",
			          file.c_str());
			dagman.dag->RemoveRunningJobs(dagman.DAGManJobId, rm_reason, true, true);
			dagmanUtils.tolerant_unlink(dagOpts[shallow::str::LockFile]);
			dagman.CleanUp();
			
				// Note: debug_error calls DC_Exit().
			debug_error(1, DEBUG_QUIET, "Failed to parse %s\n", file.c_str());
		}
	}

	if (dagOpts[shallow::i::Priority] != 0) {
		dagman.dag->SetNodePriorities(); // Applies to the nodes of the dag
	}

	dagman.dag->GetJobstateLog().WriteDagmanStarted(dagman.DAGManJobId);

	// lift the final set of splices into the main dag.
	dagman.dag->LiftSplices(SELF);

	// adjust the parent/child edges removing duplicates and setting up for processing
	debug_printf(DEBUG_VERBOSE, "Adjusting edges\n");
	dagman.dag->AdjustEdges();

	dagman.metrics->CountNodes(dagman.dag);

	// Set nodes marked as DONE in dag file to STATUS_DONE
	dagman.dag->SetPreDoneNodes();

	if ( ! dagOpts[shallow::str::SaveFile].empty()) {
		// Parse rescue formatted Save file. Reading from a save file
		// and a specific rescue number is prohibited (enforced post cmd arg parsing)
		// But if auto rescue is on and a rescue file is found prioritize
		// specified save file for parsing. - Cole Bollig 2023-03-30
		std::string loadSaveFile = dagOpts[shallow::str::SaveFile];
		debug_printf(DEBUG_QUIET, "Loading saved progress from %s for DAG.\n", loadSaveFile.c_str());
		debug_printf(DEBUG_QUIET, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
		auto [saveFile, _] = dagmanUtils.ResolveSaveFile(dagman.options.primaryDag(), loadSaveFile);
		//Don't munge node names because save files written via rescue code already munged
		parseSetDoNameMunge(false);
		//Attempt to parse the save file. Run parse with useDagDir = false because
		//there is no point risking changing directories just to read save file (i.e. partial rescue)
		auto saveUseDadDir = dagman.options[deep::b::UseDagDir];
		dagman.options[deep::b::UseDagDir] = false;
		if ( ! parse(dagman, dagman.dag, saveFile.c_str())) {
			std::string rm_reason;
			formatstr(rm_reason, "Startup Error: DAGMan failed to parse save file (%s).",
			          saveFile.c_str());
			dagman.dag->RemoveRunningJobs(dagman.DAGManJobId, rm_reason, true, true);
			dagmanUtils.tolerant_unlink(dagOpts[shallow::str::LockFile]);
			dagman.CleanUp();
			debug_error(1, DEBUG_QUIET, "Failed to parse save file\n");
		}
		dagman.options[deep::b::UseDagDir] = saveUseDadDir;
	} else if (rescueDagNum > 0) {
		// Actually parse the "new-new" style (partial DAG info only)
		// rescue DAG here.  Note: this *must* be done after splices
		// are lifted!
		dagman.dag->GetJobstateLog().InitializeRescue();
		dagman.rescueFileToRun = dagmanUtils.RescueDagName(dagOpts.primaryDag(), dagOpts.isMultiDag(), rescueDagNum);
		debug_printf(DEBUG_QUIET, "%s; running %s in combination with normal DAG file%s\n",
		             rescueDagMsg.c_str(), dagman.rescueFileToRun.c_str(),
		             dagOpts.isMultiDag() ? "s" : "");
		debug_printf(DEBUG_QUIET, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
		debug_printf(DEBUG_QUIET, "USING RESCUE DAG %s\n", dagman.rescueFileToRun.c_str());

		// Turn off node name munging for the rescue DAG, because
		// it will already have munged node names.
		parseSetDoNameMunge(false);

		if( ! parse(dagman, dagman.dag, dagman.rescueFileToRun.c_str())) {
			if (dagOpts[shallow::b::DumpRescueDag]) {
				// Dump the rescue DAG so we can see what we got
				// in the failed parse attempt.
				debug_printf(DEBUG_QUIET, "Dumping rescue DAG because of -DumpRescue flag\n");
				dagman.dag->Rescue(dagOpts.primaryDag().c_str(), dagOpts.isMultiDag(), dagman.config[conf::i::MaxRescueNum], true, false);
			}
			
			// I guess we're setting bForce to true here in case we're
			// in recovery mode and we have any leftover jobs from
			// before (e.g., user did condor_hold, modified DAG (or
			// rescue DAG) file introducing a syntax error, and then
			// did condor_release). (wenger 2014-10-28)
			std::string rm_reason;
			formatstr(rm_reason, "Startup Error: DAGMan failed to parse rescue file (%s).",
			          dagman.rescueFileToRun.c_str());
			dagman.dag->RemoveRunningJobs(dagman.DAGManJobId, rm_reason, true, true);
			dagmanUtils.tolerant_unlink(dagOpts[shallow::str::LockFile]);
			dagman.CleanUp();
			
			// Note: debug_error calls DC_Exit().
			debug_error(1, DEBUG_QUIET, "Failed to parse dag file\n");
		}
	}

	dagman.dag->CheckThrottleCats();

/*	debug_printf(DEBUG_QUIET, "COMPLETED DAG!\n");*/
/*	dagman.dag->PrintNodeList();*/

#ifndef NOT_DETECT_CYCLE
	if (dagman.config[conf::b::DetectCycle] && dagman.dag->isCycle()) {
		// Note: maybe we should run the final node here, if there is one.
		// wenger 2011-12-19.
		debug_error(1, DEBUG_QUIET, "ERROR: a cycle exists in the dag, please check input\n");
	}
#endif
	debug_printf(DEBUG_VERBOSE, "Dag contains %d total nodes\n", dagman.dag->NumNodes(true));

	std::string firstLocation;
	if (dagman.dag->GetReject(firstLocation)) {
		debug_printf(DEBUG_QUIET,
		             "Exiting because of REJECT specification in %s. This most likely means that the DAG "
		             "file was produced with the -DumpRescue flag when parsing the original DAG failed.\n",
		             firstLocation.c_str());
		DC_Exit(EXIT_ERROR);
		return;
	}

	dagman.dag->DumpDotFile();
	if (dagOpts[shallow::b::OnlyDumpDot]) {
		if ( ! dagman.dag->GetDotFileName()) {
			debug_printf(DEBUG_NORMAL, "Unable to write .dot file, no DOT filename specified\n");
		}
		else {
			debug_printf(DEBUG_NORMAL, "Writing .dot file: %s\n", dagman.dag->GetDotFileName());
		}
		ExitSuccess();
	}

	if (dagOpts[shallow::b::DumpRescueDag]) {
		debug_printf(DEBUG_QUIET, "Dumping rescue DAG and exiting because of -DumpRescue flag\n");
		dagman.dag->Rescue(dagOpts.primaryDag().c_str(), dagOpts.isMultiDag(), dagman.config[conf::i::MaxRescueNum], false, false, false);
		ExitSuccess();
		return;
	}

	//------------------------------------------------------------------------
	// Bootstrap and Recovery
	//
	// If the Lockfile exists, this indicates a premature termination
	// of a previous run of Dagman. If condor log is also present,
	// we run in recovery mode

	// If the Daglog is not present, then we do not run in recovery
	// mode
	// I don't know what this comment means.  wenger 2013-09-11

	{
		const std::string &lockFile = dagOpts[shallow::str::LockFile];
		bool recovery = access(lockFile.c_str(), F_OK) == 0;

		if (recovery) {
			debug_printf(DEBUG_VERBOSE, "Lock file %s detected,\n", lockFile.c_str());
			if (dagman.config[conf::b::AbortDuplicates]) {
				if (dagmanUtils.check_lock_file(lockFile.c_str()) == 1) {
					debug_printf(DEBUG_QUIET,
					             "Aborting because it looks like another instance of DAGMan is "
					             "currently running on this DAG; if that is not the case, delete the lock file (%s) "
					             "and re-submit the DAG.\n", lockFile.c_str());
					dagman.dag->GetJobstateLog().WriteDagmanFinished(EXIT_RESTART);
					dagman.CleanUp();
					DC_Exit(EXIT_ERROR);
					// We should never get to here!
				}
			}

		} else if (dagOpts[shallow::b::DoRecovery]) {
			debug_printf(DEBUG_VERBOSE, "Running in recovery mode because -DoRecovery flag was specified\n");
			recovery = true;
		}

		// If this DAGMan continues, it should overwrite the lock file if it exists.
		dagmanUtils.create_lock_file(lockFile.c_str(), dagman.config[conf::b::AbortDuplicates]);

		debug_printf(DEBUG_VERBOSE, "Bootstrapping...\n");
		if( ! dagman.dag->Bootstrap(recovery)) {
			dagman.dag->PrintReadyQ(DEBUG_DEBUG_1);
			debug_error(1, DEBUG_QUIET, "ERROR while bootstrapping\n");
		}
		print_status(true);
	}

	debug_printf(DEBUG_VERBOSE, "Registering condor_event_timer...\n");
	daemonCore->Register_Timer(1, dagman.config[conf::i::LogScanInterval], condor_event_timer, "condor_event_timer");
}

//---------------------------------------------------------------------------
void Dagman::ResolveDefaultLog() {
	const std::string& primaryDag = options.primaryDag();
	std::string& nodesLog = config[conf::str::NodesLog];
	std::string dagDir = condor_dirname(primaryDag.c_str());
	const char *dagFile = condor_basename(primaryDag.c_str());

	std::string owner;
	std::string nodeName;
	_dagmanClassad->GetInfo(owner, nodeName);
	std::string cluster(std::to_string(DAGManJobId._cluster));

	replace_str(nodesLog, "@(DAG_DIR)", dagDir);
	replace_str(nodesLog, "@(DAG_FILE)", dagFile);
	replace_str(nodesLog, "@(CLUSTER)", cluster);
	replace_str(nodesLog, "@(OWNER)", owner);
	replace_str(nodesLog, "@(NODE_NAME)", nodeName);

	if (nodesLog.find("@(") != std::string::npos) {
		debug_printf(DEBUG_QUIET,
		             "Warning: default node log file %s contains a '@(' character sequence -- unresolved macro substituion?\n",
		             nodesLog.c_str());
		check_warning_strictness(DAG_STRICT_1);
	}

	// Force default log file path to be absolute so it works with -usedagdir and DIR nodes.
	CondorError errstack;
	if ( ! MultiLogFiles::makePathAbsolute(nodesLog, errstack)) {
		debug_printf(DEBUG_QUIET, "Unable to convert default log file name to absolute path: %s\n",
		             errstack.getFullText().c_str());
		DC_Exit(EXIT_ERROR);
	}

	// DO NOT DOCUMENT this testing-only knob.
	if (param_boolean("DAGMAN_AVOID_SLASH_TMP", true)) {
		if (nodesLog.find("/tmp") == 0) {
			debug_printf(DEBUG_QUIET, "Warning: default node log file %s is in /tmp\n",
			             nodesLog.c_str());
			check_warning_strictness(DAG_STRICT_1);
		}
	}

	bool nfsLogIsError = config[conf::b::NfsLogError];
	if (nfsLogIsError) {
		if (param_boolean("ENABLE_USERLOG_LOCKING", false)) {
			if (param_boolean("CREATE_LOCKS_ON_LOCAL_DISK", true)) {
				debug_printf(DEBUG_QUIET,
				             "Ignoring value of DAGMAN_LOG_ON_NFS_IS_ERROR because ENABLE_USERLOG_LOCKING "
				             "and CREATE_LOCKS_ON_LOCAL_DISK are true.\n");
				nfsLogIsError = false;
			}
		} else {
			debug_printf(DEBUG_QUIET,
			             "Ignoring value of DAGMAN_LOG_ON_NFS_IS_ERROR because ENABLE_USERLOG_LOCKING is false.\n");
			nfsLogIsError = false;
		}
	}

	// This function returns true if the log file is on NFS and
	// that is an error.  If the log file is on NFS, but nfsIsError
	// is false, it prints a warning but returns false.
	if (MultiLogFiles::logFileNFSError(nodesLog.c_str(), nfsLogIsError)) {
		debug_printf(DEBUG_QUIET, "Error: log file %s on NFS\n", nodesLog.c_str());
		DC_Exit(EXIT_ERROR);
	}

	debug_printf(DEBUG_NORMAL, "Default node log file is: <%s>\n", nodesLog.c_str());
}

void Dagman::PublishStats() {
	ClassAd statsAd;
	stats.Publish(statsAd);

	std::string statsString;
	for (const auto&[key, _] : statsAd) {
		double value;
		if ( ! statsAd.EvaluateAttrReal(key.c_str(), value)) {
			debug_printf(DEBUG_VERBOSE, "Failed to get %s statistic value.\n", key.c_str());
			continue;
		}
		formatstr_cat(statsString, "%s=%.3lf; ", key.c_str(), value);
	}

	debug_printf(DEBUG_VERBOSE, "DAGMan Runtime Statistics: [%s]\n", statsString.c_str());
}

void Dagman::ReportMetrics(const int exitCode) {
	if ( ! metrics) { return; }
	if ( ! metrics->Report(exitCode, *this)) {
		debug_printf(DEBUG_QUIET, "Failed to report metrics.\n");
	}
}

void print_status(bool forceScheddUpdate) {
	debug_printf(DEBUG_VERBOSE, "DAG status: %d (%s)\n", dagman.dag->_dagStatus, dagman.dag->GetStatusName());

	int total = dagman.dag->NumNodes( true );
	int done = dagman.dag->NumNodesDone( true );
	int pre = dagman.dag->PreRunNodeCount();
	int submitted = dagman.dag->NumNodesSubmitted();
	int post = dagman.dag->PostRunNodeCount();
	int ready =  dagman.dag->NumNodesReady();
	int failed = dagman.dag->NumNodesFailed();
	int futile = dagman.dag->NumNodesFutile();
	int unready = dagman.dag->NumNodesUnready( true );

	debug_printf(DEBUG_VERBOSE, "Of %d nodes total:\n", total );
	debug_printf(DEBUG_VERBOSE, " Done     Pre   Queued    Post   Ready   Un-Ready   Failed   Futile\n");
	debug_printf(DEBUG_VERBOSE, "  ===     ===      ===     ===     ===        ===      ===      ===\n");
	debug_printf(DEBUG_VERBOSE, "%5d   %5d    %5d   %5d   %5d      %5d    %5d    %5d\n",
	             done, pre, submitted, post, ready, unready, failed, futile);
	debug_printf(DEBUG_VERBOSE, "%d job proc(s) currently held\n", dagman.dag->NumHeldJobProcs());
	dagman.dag->PrintDeferrals(DEBUG_VERBOSE, false);

	dagman.PublishStats();

	if (forceScheddUpdate) { dagman.UpdateAd(); }
}

void condor_event_timer (int /* tid */) {

	ASSERT(dagman.dag);

	//------------------------------------------------------------------------
	// Proceed with normal operation
	//
	// At this point, the DAG is bootstrapped.  All jobs premarked DONE
	// are in a STATUS_DONE state, and all their children have been
	// marked ready to submit.
	//
	// If recovery was needed, the log file has been completely read and
	// we are ready to proceed with jobs yet unsubmitted.
	//------------------------------------------------------------------------

	if (dagman.paused) {
		debug_printf(DEBUG_DEBUG_1, "(DAGMan paused)\n");
		return;
	}

	static int prevNodesDone = 0;
	static int prevNodes = 0;
	static int prevNodesFailed = 0;
	static int prevNodesSubmitted = 0;
	static int prevNodesReady = 0;
	static int prevScriptRunNodes = 0;
	static int prevJobsHeld = 0;

	static time_t lastPrintJobTable = time(nullptr);

	static double eventTimerStartTime = 0;
	static double eventTimerEndTime = 0;
	
	double logProcessCycleStartTime;
	double logProcessCycleEndTime;
	double submitCycleStartTime;
	double submitCycleEndTime;

	// Gather some statistics
	eventTimerStartTime = condor_gettimestamp_double();
	if (eventTimerEndTime > 0) {
		dagman.stats.SleepCycleTime.Add(eventTimerStartTime - eventTimerEndTime);
	}
	

	dagman.dag->RunWaitingScripts();

	// Before submitting ready jobs, check the user log for errors or shrinking.
	// If either happens, this is really really bad! Bail out immediately.
	ReadUserLog::FileStatus log_status = dagman.dag->GetCondorLogStatus(dagman.config[conf::i::VerifyScheddInterval]);
	if (log_status == ReadUserLog::LOG_STATUS_ERROR || log_status == ReadUserLog::LOG_STATUS_SHRUNK) {
		debug_printf(DEBUG_NORMAL, "DAGMan exiting due to error in log file\n");
		dagman.dag->PrintReadyQ(DEBUG_DEBUG_1);
		dagman.dag->_dagStatus = DagStatus::DAG_STATUS_ERROR;
		main_shutdown_logerror();
		return;
	}

	int justSubmitted;
	debug_printf(DEBUG_DEBUG_1, "Starting submit cycle\n");
	submitCycleStartTime = condor_gettimestamp_double();
	justSubmitted = dagman.dag->SubmitReadyNodes(dagman);
	submitCycleEndTime = condor_gettimestamp_double();
	dagman.stats.SubmitCycleTime.Add(submitCycleEndTime - submitCycleStartTime);
	debug_printf(DEBUG_DEBUG_1, "Finished submit cycle\n");
	if (justSubmitted) {
		// Note: it would be nice to also have the proc submit
		// count here.  wenger, 2006-02-08.
		debug_printf(DEBUG_VERBOSE, "Just submitted %d job%s this cycle...\n", justSubmitted, justSubmitted == 1 ? "" : "s");
	}

	// Check log status for growth. If it grew, process log events.
	if (log_status == ReadUserLog::LOG_STATUS_GROWN) {
		logProcessCycleStartTime = condor_gettimestamp_double();
		if (dagman.dag->ProcessLogEvents() == false) {
			debug_printf(DEBUG_NORMAL, "ProcessLogEvents() returned false\n");
			dagman.dag->PrintReadyQ(DEBUG_DEBUG_1);
			main_shutdown_rescue(EXIT_ERROR, DagStatus::DAG_STATUS_ERROR);
			return;
		}
		logProcessCycleEndTime = condor_gettimestamp_double();
		dagman.stats.LogProcessCycleTime.Add(logProcessCycleEndTime - logProcessCycleStartTime);
	}

	int currJobsHeld = dagman.dag->NumHeldJobProcs();
	// print status if anything's changed (or we're in a high debug level)
	if (prevNodesDone != dagman.dag->NumNodesDone(true)
		|| prevNodes != dagman.dag->NumNodes(true)
		|| prevNodesFailed != dagman.dag->NumNodesFailed()
		|| prevNodesSubmitted != dagman.dag->NumNodesSubmitted()
		|| prevNodesReady != dagman.dag->NumNodesReady()
		|| prevScriptRunNodes != dagman.dag->ScriptRunNodeCount()
		|| prevJobsHeld != currJobsHeld
		|| DEBUG_LEVEL(DEBUG_DEBUG_4))
	{
		print_status();

		prevNodesDone = dagman.dag->NumNodesDone(true);
		prevNodes = dagman.dag->NumNodes(true);
		prevNodesFailed = dagman.dag->NumNodesFailed();
		prevNodesSubmitted = dagman.dag->NumNodesSubmitted();
		prevNodesReady = dagman.dag->NumNodesReady();
		prevScriptRunNodes = dagman.dag->ScriptRunNodeCount();
		prevJobsHeld = currJobsHeld;
		
		if (dagman.dag->GetDotFileUpdate()) { dagman.dag->DumpDotFile(); }
	}

	time_t printJobTableDelay = (time_t)dagman.config[conf::i::JobStateTableInterval];
	if (printJobTableDelay && ((time(nullptr) - lastPrintJobTable) >= printJobTableDelay)) {
		int jobsIdle, jobsHeld, jobsRunning, jobsTerminated, jobsSuccess;
		dagman.dag->NumJobProcStates(&jobsHeld,&jobsIdle,&jobsRunning, &jobsTerminated);
		jobsSuccess = dagman.dag->TotalJobsCompleted();

		debug_printf(DEBUG_VERBOSE, "Total jobs placed to AP: %d\n", dagman.dag->TotalJobsSubmitted());
		debug_printf(DEBUG_VERBOSE, "     Idle     Held     Running     Successful     Failed\n");
		debug_printf(DEBUG_VERBOSE, "      ===      ===         ===            ===        ===\n");
		debug_printf(DEBUG_VERBOSE, "  %7d  %7d     %7d     %10d %10d\n",
		            jobsIdle, jobsHeld,
		            jobsRunning, jobsSuccess,
		            jobsTerminated - jobsSuccess);
		lastPrintJobTable = time(nullptr);
	}

	// Periodically perform a two-way update with the job ad
	double currentTime = condor_gettimestamp_double();
	static double scheddLastUpdateTime = 0.0;
	if (scheddLastUpdateTime <= 0.0) {
		scheddLastUpdateTime = currentTime;
	}
	if (currentTime > (scheddLastUpdateTime + dagman.config[conf::dbl::ScheddUpdateInterval])) {
		dagman.UpdateAd();
		scheddLastUpdateTime = currentTime;
	}

	dagman.dag->DumpNodeStatus(false, false);

	ASSERT(dagman.dag->NumNodesDone(true) + dagman.dag->NumNodesFailed() <= dagman.dag->NumNodes(true));

	// If DAG is complete, hurray, and exit.
	if (dagman.dag->DoneSuccess(true)) {
		ASSERT(dagman.dag->NumNodesSubmitted() == 0);
		dagman.dag->RemoveServiceNodes();
		dagman.dag->CheckAllJobs();
		debug_printf(DEBUG_NORMAL, "All jobs Completed!\n");
		dagman.dag->PrintDeferrals(DEBUG_NORMAL, true);
		if (dagman.dag->NumIdleJobProcs() != 0) {
			debug_printf(DEBUG_NORMAL,
			             "Warning:  DAGMan thinks there are %d idle jobs, even though the DAG is completed!\n",
			             dagman.dag->NumIdleJobProcs() );
			check_warning_strictness(DAG_STRICT_1);
		}
		ExitSuccess();
		return;
	}

	// DAG has failed -- dump rescue DAG.
	if (dagman.dag->DoneFailed(true)) {
		debug_printf(DEBUG_QUIET, "ERROR: the following job(s) failed:\n");
		dagman.dag->PrintNodeList(Node::STATUS_ERROR);
		main_shutdown_rescue(EXIT_ERROR, dagman.dag->_dagStatus);
		return;
	}

	// If Final node has exited but DAGMan is still waiting on jobs
	// something is wrong (likely missed job events)
	if (dagman.dag->FinalNodeFinished()) {
		// Replace with a world view check to hopefully exit with above paths
		debug_printf(DEBUG_QUIET,
		             "ERROR: DAGMan FINAL node has terminated but DAGMan thinks %d job(s) are still running.\n",
		             dagman.dag->NumNodesSubmitted());
		main_shutdown_rescue(EXIT_ABORT, dagman.dag->_dagStatus);
		return;
	}

	// DAG has succeeded but we haven't run final node yet, so do that.
	if (dagman.dag->DoneSuccess(false)) {
		dagman.dag->StartFinalNode();
		return;
	}

	// If the DAG is halted, we don't want to actually exit yet if
	// jobs are still in the queue, or any POST scripts need to be
	// run (we need to run POST scripts so we don't "waste" jobs
	// that completed; on the other hand, we don't care about waiting
	// for PRE scripts because they'll be re-run when the rescue
	// DAG is run anyhow).
	if (dagman.dag->IsHalted() && dagman.dag->NumNodesSubmitted() == 0 &&
	    dagman.dag->PostRunNodeCount() == 0 && !dagman.dag->FinalNodeRun())
	{
		// Note:  main_shutdown_rescue() will run the final node
		// if there is one.
		debug_printf (DEBUG_QUIET, "Exiting because DAG is halted and no jobs or scripts are running\n");
		debug_printf( DEBUG_QUIET, "ERROR: the following Node(s) failed:\n");
		dagman.dag->PrintNodeList(Node::STATUS_ERROR);
		main_shutdown_rescue(EXIT_ERROR, DagStatus::DAG_STATUS_HALTED);
		return;
	}

	// If no nodes are submitted and no scripts are running, but the
	// dag is not complete, then at least one node failed, or a cycle
	// exists.  (Note that if the DAG completed successfully, we already
	// returned from this function above.)
	if (dagman.dag->FinishedRunning(false)) {
		DagStatus dagStatus = DagStatus::DAG_STATUS_OK;
		if (dagman.dag->DoneFailed(false)) {
			if (DEBUG_LEVEL(DEBUG_QUIET)) {
				debug_printf(DEBUG_QUIET, "ERROR: the following Node(s) failed:\n");
				dagman.dag->PrintNodeList(Node::STATUS_ERROR);
			}
			dagStatus = DagStatus::DAG_STATUS_NODE_FAILED;
		} else {
			// no Nodes failed, so a cycle must exist
			debug_printf(DEBUG_QUIET, "ERROR: DAG finished but not all nodes are complete -- checking for a cycle...\n");
			if (dagman.dag->isCycle()) {
				debug_printf(DEBUG_QUIET, "... ERROR: a cycle exists in the dag, please check input\n");
				dagStatus = DagStatus::DAG_STATUS_CYCLE;
			} else {
				debug_printf(DEBUG_QUIET, "... ERROR: no cycle found; unknown error condition\n");
				dagStatus = DagStatus::DAG_STATUS_ERROR;
			}
			if (debug_level >= DEBUG_NORMAL) {
				dagman.dag->PrintNodeList();
			}
		}

		main_shutdown_rescue(EXIT_ERROR, dagStatus);
		return;
	}
	
	// Statistics gathering
	eventTimerEndTime = condor_gettimestamp_double();
	dagman.stats.EventCycleTime.Add(eventTimerEndTime - eventTimerStartTime);
}

void main_pre_dc_init (int, char*[]) {
	DC_Skip_Core_Init();
#ifdef WIN32
	_setmaxstdio(2048);
#endif

	// Convert the DAGMan log file name to an absolute path if it's not one already
	std::string fullLogFile;
	const char* logFile = GetEnv("_CONDOR_DAGMAN_LOG");
	if (logFile) {
		if ( ! fullpath(logFile)) {
			dircat(dagman.workingDir.c_str(), logFile, fullLogFile);
			SetEnv("_CONDOR_DAGMAN_LOG", fullLogFile.c_str());
		} else { fullLogFile = logFile; }
	} else {
		dircat(dagman.workingDir.c_str(), "default.dagman.out", fullLogFile);
		SetEnv("_CONDOR_DAGMAN_LOG", fullLogFile.c_str());
	}

	// Manually setup debugging file since default log is disabled
	dprintf_config_tool(get_mySubSystem()->getName(), nullptr, fullLogFile.c_str());
}

void main_pre_command_sock_init() {
	daemonCore->m_create_family_session = false;
}

int main(int argc, char **argv) {
	set_mySubSystem("DAGMAN", false, SUBSYSTEM_TYPE_DAGMAN);
	DC_Disable_Default_Log();

	// Record the workingDir before invoking daemoncore (which hijacks it)
	if ( ! condor_getcwd(dagman.workingDir)) {
		fprintf(stderr, "ERROR (%d): unable to get working directory: %s\n", errno, strerror(errno));
		return EXIT_ERROR;
	}

	debug_level = DEBUG_VERBOSE; // Default debug level is verbose output

	dc_main_init = main_init;
	dc_main_config = main_config;
	dc_main_shutdown_fast = main_shutdown_fast;
	dc_main_shutdown_graceful = main_shutdown_graceful;
	dc_main_pre_dc_init = main_pre_dc_init;
	dc_main_pre_command_sock_init = main_pre_command_sock_init;

	return dc_main(argc, argv);
}
