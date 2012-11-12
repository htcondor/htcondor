/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
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
#include "condor_string.h"
#include "subsystem_info.h"
#include "basename.h"
#include "setenv.h"
#include "dag.h"
#include "debug.h"
#include "parse.h"
#include "my_username.h"
#include "condor_environ.h"
#include "dagman_main.h"
#include "dagman_commands.h"
#include "dagman_multi_dag.h"
#include "util.h"
#include "condor_getcwd.h"
#include "condor_version.h"
#include "subsystem_info.h"

void ExitSuccess();

	// From condor_utils/condor_config.C
	// Note: these functions are declared 'extern "C"' where they're
	// implemented; if we don't do that here we get a link failure
	// (I think because of the name mangling).  wenger 2007-02-09.
extern "C" void process_config_source( char* file, const char* name,
			char* host, int required );
extern "C" bool is_piped_command(const char* filename);

static char* lockFileName = NULL;

static Dagman dagman;

strict_level_t Dagman::_strict = DAG_STRICT_0;

//---------------------------------------------------------------------------
static void Usage() {
    debug_printf( DEBUG_SILENT, "\nUsage: condor_dagman -f -t -l .\n"
            "\t\t-Lockfile <NAME.dag.lock>\n"
            "\t\t-Dag <NAME.dag>\n"
            "\t\t-CsdVersion <version string>\n"
            "\t\t[-Debug <level>]\n"
            "\t\t[-MaxIdle <int N>]\n"
            "\t\t[-MaxJobs <int N>]\n"
            "\t\t[-MaxPre <int N>]\n"
            "\t\t[-MaxPost <int N>]\n"
            "\t\t[-DontAlwaysRunPost]\n"
            "\t\t[-WaitForDebug]\n"
            "\t\t[-NoEventChecks]\n"
            "\t\t[-AllowLogError]\n"
            "\t\t[-UseDagDir]\n"
            "\t\t[-AutoRescue <0|1>]\n"
            "\t\t[-DoRescueFrom <int N>]\n"
            "\t\t[-Priority <int N>]\n"
			"\t\t[-AllowVersionMismatch]\n"
			"\t\t[-DumpRescue]\n"
			"\t\t[-Verbose]\n"
			"\t\t[-Force]\n"
			"\t\t[-Notification <never|always|complete|error>]\n"
			"\t\t[-Dagman <dagman_executable>]\n"
			"\t\t[-Outfile_dir <directory>]\n"
			"\t\t[-Update_submit]\n"
			"\t\t[-Import_env]\n"
			"\t\t[-Suppress_notification]\n"
			"\t\t[-Dont_Suppress_notification]\n"
            "\twhere NAME is the name of your DAG.\n"
            "\tdefault -Debug is -Debug %d\n", DEBUG_NORMAL);
	DC_Exit( EXIT_ERROR );
}

//---------------------------------------------------------------------------


Dagman::Dagman() :
	dag (NULL),
	maxIdle (0),
	maxJobs (0),
	maxPreScripts (0),
	maxPostScripts (0),
	paused (false),
	condorSubmitExe (NULL),
	condorRmExe (NULL),
	storkSubmitExe (NULL),
	storkRmExe (NULL),
	submit_delay (0),
	max_submit_attempts (6),
	max_submits_per_interval (5), // so Coverity is happy
	m_user_log_scan_interval (5),
	primaryDagFile (""),
	multiDags (false),
	startup_cycle_detect (false), // so Coverity is happy
	allowLogError (false),
	useDagDir (false),
	allow_events (CheckEvents::ALLOW_NONE), // so Coverity is happy
	retrySubmitFirst (true), // so Coverity is happy
	retryNodeFirst (false), // so Coverity is happy
	mungeNodeNames (true), // so Coverity is happy
	prohibitMultiJobs (false), // so Coverity is happy
	abortDuplicates (true), // so Coverity is happy
	submitDepthFirst (false), // so Coverity is happy
	abortOnScarySubmit (true), // so Coverity is happy
	pendingReportInterval (10 * 60), // 10 minutes
	_dagmanConfigFile (NULL), // so Coverity is happy
	autoRescue(true),
	doRescueFrom(0),
	maxRescueDagNum(MAX_RESCUE_DAG_DEFAULT),
	rescueFileToRun(""),
	dumpRescueDag(false),
	_writePartialRescueDag(true),
	_defaultNodeLog(NULL),
	_generateSubdagSubmits(true),
	_maxJobHolds(100),
	_runPost(true),
	_defaultPriority(0),
	_claim_hold_time(20)
{
    debug_level = DEBUG_VERBOSE;  // Default debug level is verbose output
}


Dagman::~Dagman()
{
	// check if dag is NULL, since we may have 
	// already delete'd it in the dag.CleanUp() method.
	if ( dag != NULL ) {
		delete dag;
		dag = NULL;
	}
}

	// 
	// In Config() we get DAGMan-related configuration values.  This
	// is a three-step process:
	// 1. Get the name of the DAGMan-specific config file (if any).
	// 2. If there is a DAGMan-specific config file, process it so
	//    that its values are added to the configuration.
	// 3. Get the values we want from the configuration.
	//
bool
Dagman::Config()
{
	int debug_cache_size = (1024*1024)*5; // 5 MB
	bool debug_cache_enabled = false;

		// Note: debug_printfs are DEBUG_NORMAL here because when we
		// get here we haven't processed command-line arguments yet.

		// Get and process the DAGMan-specific config file (if any)
		// before getting any of the other parameters.
	_dagmanConfigFile = param( "DAGMAN_CONFIG_FILE" );
	if ( _dagmanConfigFile ) {
		debug_printf( DEBUG_NORMAL, "Using DAGMan config file: %s\n",
					_dagmanConfigFile );
			// We do this test here because the corresponding error
			// message from the config code doesn't show up in dagman.out.
		if ( access( _dagmanConfigFile, R_OK ) != 0 &&
					!is_piped_command( _dagmanConfigFile ) ) {
			debug_printf( DEBUG_QUIET,
						"ERROR: Can't read DAGMan config file: %s\n",
						_dagmanConfigFile );
    		DC_Exit( EXIT_ERROR );
		}
		process_config_source( _dagmanConfigFile, "DAGMan config",
					NULL, true );
	}

	_strict = (strict_level_t)param_integer( "DAGMAN_USE_STRICT",
				_strict, DAG_STRICT_0, DAG_STRICT_3 );
	debug_printf( DEBUG_NORMAL, "DAGMAN_USE_STRICT setting: %d\n",
				_strict );

	debug_level = (debug_level_t)param_integer( "DAGMAN_VERBOSITY",
				debug_level, DEBUG_SILENT, DEBUG_DEBUG_4 );
	debug_printf( DEBUG_NORMAL, "DAGMAN_VERBOSITY setting: %d\n",
				debug_level );

	debug_cache_size = 
		param_integer( "DAGMAN_DEBUG_CACHE_SIZE", debug_cache_size,
		0, INT_MAX);
	debug_printf( DEBUG_NORMAL, "DAGMAN_DEBUG_CACHE_SIZE setting: %d\n",
				debug_cache_size );

	debug_cache_enabled = 
		param_boolean( "DAGMAN_DEBUG_CACHE_ENABLE", debug_cache_enabled );
	debug_printf( DEBUG_NORMAL, "DAGMAN_DEBUG_CACHE_ENABLE setting: %s\n",
				debug_cache_enabled?"True":"False" );

	submit_delay = param_integer( "DAGMAN_SUBMIT_DELAY", submit_delay, 0);
	debug_printf( DEBUG_NORMAL, "DAGMAN_SUBMIT_DELAY setting: %d\n",
				submit_delay );

	max_submit_attempts =
		param_integer( "DAGMAN_MAX_SUBMIT_ATTEMPTS", max_submit_attempts,
		1, 16 );
	debug_printf( DEBUG_NORMAL, "DAGMAN_MAX_SUBMIT_ATTEMPTS setting: %d\n",
				max_submit_attempts );

	startup_cycle_detect =
		param_boolean( "DAGMAN_STARTUP_CYCLE_DETECT", startup_cycle_detect );
	debug_printf( DEBUG_NORMAL, "DAGMAN_STARTUP_CYCLE_DETECT setting: %s\n",
				startup_cycle_detect ? "True" : "False" );

	max_submits_per_interval =
		param_integer( "DAGMAN_MAX_SUBMITS_PER_INTERVAL",
		max_submits_per_interval, 1, 1000 );
	debug_printf( DEBUG_NORMAL, "DAGMAN_MAX_SUBMITS_PER_INTERVAL setting: %d\n",
				max_submits_per_interval );

	m_user_log_scan_interval =
		param_integer( "DAGMAN_USER_LOG_SCAN_INTERVAL",
		m_user_log_scan_interval, 1, INT_MAX);
	debug_printf( DEBUG_NORMAL, "DAGMAN_USER_LOG_SCAN_INTERVAL setting: %d\n",
				m_user_log_scan_interval );

	_defaultPriority = param_integer("DAGMAN_DEFAULT_PRIORITY", 0, INT_MIN,
		INT_MAX, false);
	debug_printf( DEBUG_NORMAL, "DAGMAN_DEFAULT_PRIORITY setting: %d\n",
				_defaultPriority );

	_submitDagDeepOpts.always_use_node_log = param_boolean( "DAGMAN_ALWAYS_USE_NODE_LOG", true);
	debug_printf( DEBUG_NORMAL, "DAGMAN_ALWAYS_USE_NODE_LOG setting: %s\n",
				_submitDagDeepOpts.always_use_node_log ? "True" : "False" );

	_submitDagDeepOpts.suppress_notification = param_boolean(
		"DAGMAN_SUPPRESS_NOTIFICATION",
		_submitDagDeepOpts.suppress_notification);
	debug_printf( DEBUG_NORMAL, "DAGMAN_SUPPRESS_NOTIFICATION setting: %s\n",
		_submitDagDeepOpts.suppress_notification ? "True" : "False" );

		// Event checking setup...

		// We want to default to allowing the terminated/aborted
		// combination (that's what we've defaulted to in the past).
		// Okay, we also want to allow execute before submit because
		// we've run into that, and since DAGMan doesn't really care
		// about the execute events, it shouldn't abort the DAG.
		// And we further want to allow two terminated events for a
		// single job because people are seeing that with Globus
		// jobs!!
	allow_events = CheckEvents::ALLOW_TERM_ABORT |
			CheckEvents::ALLOW_EXEC_BEFORE_SUBMIT |
			CheckEvents::ALLOW_DOUBLE_TERMINATE |
			CheckEvents::ALLOW_DUPLICATE_EVENTS;

		// If the old DAGMAN_IGNORE_DUPLICATE_JOB_EXECUTION param is set,
		// we also allow extra runs.
		// Note: this parameter is probably only used by CDF, and only
		// really needed until they update all their systems to 6.7.3
		// or later (not 6.7.3 pre-release), which fixes the "double-run"
		// bug.
	bool allowExtraRuns = param_boolean(
			"DAGMAN_IGNORE_DUPLICATE_JOB_EXECUTION", false );

	if ( allowExtraRuns ) {
		allow_events |= CheckEvents::ALLOW_RUN_AFTER_TERM;
		debug_printf( DEBUG_NORMAL, "Warning: "
				"DAGMAN_IGNORE_DUPLICATE_JOB_EXECUTION "
				"is deprecated -- used DAGMAN_ALLOW_EVENTS instead\n" );
		check_warning_strictness( DAG_STRICT_1 );
	}

		// Now get the new DAGMAN_ALLOW_EVENTS value -- that can override
		// all of the previous stuff.
	allow_events = param_integer("DAGMAN_ALLOW_EVENTS", allow_events);
	debug_printf( DEBUG_NORMAL, "allow_events ("
				"DAGMAN_IGNORE_DUPLICATE_JOB_EXECUTION, DAGMAN_ALLOW_EVENTS"
				") setting: %d\n", allow_events );

		// ...end of event checking setup.

	retrySubmitFirst = param_boolean( "DAGMAN_RETRY_SUBMIT_FIRST",
				retrySubmitFirst );
	debug_printf( DEBUG_NORMAL, "DAGMAN_RETRY_SUBMIT_FIRST setting: %s\n",
				retrySubmitFirst ? "True" : "False" );

	retryNodeFirst = param_boolean( "DAGMAN_RETRY_NODE_FIRST",
				retryNodeFirst );
	debug_printf( DEBUG_NORMAL, "DAGMAN_RETRY_NODE_FIRST setting: %s\n",
				retryNodeFirst ? "True" : "False" );

	maxIdle =
		param_integer( "DAGMAN_MAX_JOBS_IDLE", maxIdle, 0, INT_MAX );
	debug_printf( DEBUG_NORMAL, "DAGMAN_MAX_JOBS_IDLE setting: %d\n",
				maxIdle );

	maxJobs =
		param_integer( "DAGMAN_MAX_JOBS_SUBMITTED", maxJobs, 0, INT_MAX );
	debug_printf( DEBUG_NORMAL, "DAGMAN_MAX_JOBS_SUBMITTED setting: %d\n",
				maxJobs );

	maxPreScripts = param_integer( "DAGMAN_MAX_PRE_SCRIPTS", maxPreScripts,
				0, INT_MAX );
	debug_printf( DEBUG_NORMAL, "DAGMAN_MAX_PRE_SCRIPTS setting: %d\n",
				maxPreScripts );

	maxPostScripts = param_integer( "DAGMAN_MAX_POST_SCRIPTS", maxPostScripts,
				0, INT_MAX );
	debug_printf( DEBUG_NORMAL, "DAGMAN_MAX_POST_SCRIPTS setting: %d\n",
				maxPostScripts );

	allowLogError = param_boolean( "DAGMAN_ALLOW_LOG_ERROR", allowLogError );
	debug_printf( DEBUG_NORMAL, "DAGMAN_ALLOW_LOG_ERROR setting: %s\n",
				allowLogError ? "True" : "False" );

	mungeNodeNames = param_boolean( "DAGMAN_MUNGE_NODE_NAMES",
				mungeNodeNames );
	debug_printf( DEBUG_NORMAL, "DAGMAN_MUNGE_NODE_NAMES setting: %s\n",
				mungeNodeNames ? "True" : "False" );

	prohibitMultiJobs = param_boolean( "DAGMAN_PROHIBIT_MULTI_JOBS",
				prohibitMultiJobs );
	debug_printf( DEBUG_NORMAL, "DAGMAN_PROHIBIT_MULTI_JOBS setting: %s\n",
				prohibitMultiJobs ? "True" : "False" );

	submitDepthFirst = param_boolean( "DAGMAN_SUBMIT_DEPTH_FIRST",
				submitDepthFirst );
	debug_printf( DEBUG_NORMAL, "DAGMAN_SUBMIT_DEPTH_FIRST setting: %s\n",
				submitDepthFirst ? "True" : "False" );

	_runPost = param_boolean( "DAGMAN_ALWAYS_RUN_POST", true );
	debug_printf( DEBUG_NORMAL, "DAGMAN_ALWAYS_RUN_POST setting: %s\n",
			_runPost ? "True" : "False" );

	free( condorSubmitExe );
	condorSubmitExe = param( "DAGMAN_CONDOR_SUBMIT_EXE" );
	if( !condorSubmitExe ) {
		condorSubmitExe = strdup( "condor_submit" );
		ASSERT( condorSubmitExe );
	}

	free( condorRmExe );
	condorRmExe = param( "DAGMAN_CONDOR_RM_EXE" );
	if( !condorRmExe ) {
		condorRmExe = strdup( "condor_rm" );
		ASSERT( condorRmExe );
	}

	free( storkSubmitExe );
	storkSubmitExe = param( "DAGMAN_STORK_SUBMIT_EXE" );
	if( !storkSubmitExe ) {
		storkSubmitExe = strdup( "stork_submit" );
		ASSERT( storkSubmitExe );
	}

	free( storkRmExe );
	storkRmExe = param( "DAGMAN_STORK_RM_EXE" );
	if( !storkRmExe ) {
		storkRmExe = strdup( "stork_rm" );
		ASSERT( storkRmExe );
	}

	abortDuplicates = param_boolean( "DAGMAN_ABORT_DUPLICATES",
				abortDuplicates );
	debug_printf( DEBUG_NORMAL, "DAGMAN_ABORT_DUPLICATES setting: %s\n",
				abortDuplicates ? "True" : "False" );

	abortOnScarySubmit = param_boolean( "DAGMAN_ABORT_ON_SCARY_SUBMIT",
				abortOnScarySubmit );
	debug_printf( DEBUG_NORMAL, "DAGMAN_ABORT_ON_SCARY_SUBMIT setting: %s\n",
				abortOnScarySubmit ? "True" : "False" );

	pendingReportInterval = param_integer( "DAGMAN_PENDING_REPORT_INTERVAL",
				pendingReportInterval );
	debug_printf( DEBUG_NORMAL, "DAGMAN_PENDING_REPORT_INTERVAL setting: %d\n",
				pendingReportInterval );

	if ( param_boolean( "DAGMAN_OLD_RESCUE", false ) ) {
		debug_printf( DEBUG_NORMAL, "Warning: DAGMAN_OLD_RESCUE is "
					"no longer supported\n" );
		check_warning_strictness( DAG_STRICT_1 );
	}

	autoRescue = param_boolean( "DAGMAN_AUTO_RESCUE", autoRescue );
	debug_printf( DEBUG_NORMAL, "DAGMAN_AUTO_RESCUE setting: %s\n",
				autoRescue ? "True" : "False" );
	
	maxRescueDagNum = param_integer( "DAGMAN_MAX_RESCUE_NUM",
				maxRescueDagNum, 0, ABS_MAX_RESCUE_DAG_NUM );
	debug_printf( DEBUG_NORMAL, "DAGMAN_MAX_RESCUE_NUM setting: %d\n",
				maxRescueDagNum );

	_writePartialRescueDag = param_boolean( "DAGMAN_WRITE_PARTIAL_RESCUE",
				_writePartialRescueDag );
	debug_printf( DEBUG_NORMAL, "DAGMAN_WRITE_PARTIAL_RESCUE setting: %s\n",
				_writePartialRescueDag ? "True" : "False" );

	free( _defaultNodeLog );
	_defaultNodeLog = param( "DAGMAN_DEFAULT_NODE_LOG" );
	debug_printf( DEBUG_NORMAL, "DAGMAN_DEFAULT_NODE_LOG setting: %s\n",
				_defaultNodeLog ? _defaultNodeLog : "null" );

	_generateSubdagSubmits = 
		param_boolean( "DAGMAN_GENERATE_SUBDAG_SUBMITS",
		_generateSubdagSubmits );
	debug_printf( DEBUG_NORMAL, "DAGMAN_GENERATE_SUBDAG_SUBMITS setting: %s\n",
				_generateSubdagSubmits ? "True" : "False" );

	_maxJobHolds = param_integer( "DAGMAN_MAX_JOB_HOLDS", _maxJobHolds,
				0, 1000000 );
	debug_printf( DEBUG_NORMAL, "DAGMAN_MAX_JOB_HOLDS setting: %d\n", _maxJobHolds );

	_claim_hold_time = param_integer( "DAGMAN_HOLD_CLAIM_TIME", _claim_hold_time, 0, 3600);
	debug_printf( DEBUG_NORMAL, "DAGMAN_HOLD_CLAIM_TIME setting: %d\n", _claim_hold_time );

	char *debugSetting = param( "ALL_DEBUG" );
	debug_printf( DEBUG_NORMAL, "ALL_DEBUG setting: %s\n",
				debugSetting ? debugSetting : "" );
	if ( debugSetting ) {
		free( debugSetting );
	}

	debugSetting = param( "DAGMAN_DEBUG" );
	debug_printf( DEBUG_NORMAL, "DAGMAN_DEBUG setting: %s\n",
				debugSetting ? debugSetting : "" );
	if ( debugSetting ) {
		free( debugSetting );
	}

	// enable up the debug cache if needed
	if (debug_cache_enabled) {
		debug_cache_set_size(debug_cache_size);
		debug_cache_enable();
	}

	return true;
}


// NOTE: this is only called on reconfig, not at startup
void
main_config()
{
		// This is commented out because, even if we get new config
		// values here, they don't get passed to the Dag object (which
		// is where most of them actually take effect).  (See Gnats
		// PR 808.)  wenger 2007-02-09
	// dagman.Config();
}

// this is called by DC when the schedd is shutdown fast
void
main_shutdown_fast()
{
	dagman.dag->GetJobstateLog().WriteDagmanFinished( EXIT_RESTART );
    DC_Exit( EXIT_RESTART );
}

// this can be called by other functions, or by DC when the schedd is
// shutdown gracefully
void main_shutdown_graceful() {
	dagman.dag->DumpNodeStatus( true, false );
	dagman.dag->GetJobstateLog().WriteDagmanFinished( EXIT_RESTART );
	dagman.CleanUp();
	DC_Exit( EXIT_RESTART );
}

void main_shutdown_rescue( int exitVal, Dag::dag_status dagStatus ) {
		// Avoid possible infinite recursion if you hit a fatal error
		// while writing a rescue DAG.
	static bool inShutdownRescue = false;
	if ( inShutdownRescue ) {
		return;
	}
	inShutdownRescue = true;

	dagman.dag->_dagStatus = dagStatus;
	debug_printf( DEBUG_QUIET, "Aborting DAG...\n" );
		// Avoid writing two different rescue DAGs if the "main" DAG and
		// the final node (if any) both fail.
	static bool wroteRescue = false;
	if( dagman.dag ) {
			// We write the rescue DAG *before* removing jobs because
			// otherwise if we crashed, failed, or were killed while
			// removing them, we would leave the DAG in an
			// unrecoverable state...
		if( exitVal != 0 ) {
			if ( dagman.maxRescueDagNum > 0 ) {
				dagman.dag->Rescue( dagman.primaryDagFile.Value(),
							dagman.multiDags, dagman.maxRescueDagNum,
							wroteRescue, false,
							dagman._writePartialRescueDag );
				wroteRescue = true;
			} else {
				debug_printf( DEBUG_QUIET, "No rescue DAG written because "
							"DAGMAN_MAX_RESCUE_NUM is 0\n" );
			}
		}

		debug_printf( DEBUG_DEBUG_1, "We have %d running jobs to remove\n",
					dagman.dag->NumJobsSubmitted() );
		if( dagman.dag->NumJobsSubmitted() > 0 ) {
			debug_printf( DEBUG_NORMAL, "Removing submitted jobs...\n" );
			dagman.dag->RemoveRunningJobs(dagman);
		}
		if ( dagman.dag->NumScriptsRunning() > 0 ) {
			debug_printf( DEBUG_NORMAL, "Removing running scripts...\n" );
			dagman.dag->RemoveRunningScripts();
		}
		dagman.dag->PrintDeferrals( DEBUG_NORMAL, true );

			// Start the final node if we have one.
		if ( dagman.dag->StartFinalNode() ) {
				// We started a final node; return here so we wait for the
				// final node to finish, instead of exiting immediately.
			inShutdownRescue = false;
			return;
		}
		dagman.dag->DumpNodeStatus( false, true );
		dagman.dag->GetJobstateLog().WriteDagmanFinished( exitVal );
	}
	MSC_SUPPRESS_WARNING_FIXME(6031) // return falue of unlink ignored.
	unlink( lockFileName ); 
	dagman.CleanUp();
	inShutdownRescue = false;
	DC_Exit( exitVal );
}

// this gets called by DC when DAGMan receives a SIGUSR1 -- which,
// assuming the DAGMan submit file was properly written, is the signal
// the schedd will send if the DAGMan job is removed from the queue
int main_shutdown_remove(Service *, int) {
    debug_printf( DEBUG_QUIET, "Received SIGUSR1\n" );
	main_shutdown_rescue( EXIT_ABORT, Dag::DAG_STATUS_RM );
	return FALSE;
}

void ExitSuccess() {
	dagman.dag->DumpNodeStatus( false, false );
	dagman.dag->GetJobstateLog().WriteDagmanFinished( EXIT_OKAY );
	MSC_SUPPRESS_WARNING_FIXME(6031) // return falue of unlink ignored.
	unlink( lockFileName ); 
	dagman.CleanUp();
	DC_Exit( EXIT_OKAY );
}

void condor_event_timer();

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
void main_init (int argc, char ** const argv) {

	printf ("Executing condor dagman ... \n");

		// flag used if DAGMan is invoked with -WaitForDebug so we
		// wait for a developer to attach with a debugger...
	volatile int wait_for_debug = 0;

		// process any config vars -- this happens before we process
		// argv[], since arguments should override config settings
	dagman.Config();

	// The DCpermission (last parm) should probably be PARENT, if it existed
    daemonCore->Register_Signal( SIGUSR1, "SIGUSR1",
                                 (SignalHandler) main_shutdown_remove,
                                 "main_shutdown_remove", NULL);

/****** FOR TESTING *******
    daemonCore->Register_Signal( SIGUSR2, "SIGUSR2",
                                 (SignalHandler) main_testing_stub,
                                 "main_testing_stub", NULL);
****** FOR TESTING ********/
    debug_progname = condor_basename(argv[0]);

		// condor_submit_dag version from .condor.sub
	bool allowVerMismatch = false;
	const char *csdVersion = "undefined";

	int i;
    for (i = 0 ; i < argc ; i++) {
        debug_printf( DEBUG_NORMAL, "argv[%d] == \"%s\"\n", i, argv[i] );
    }

    if (argc < 2) Usage();  //  Make sure an input file was specified

		// get dagman job id from environment, if it's there
		// (otherwise it will be set to "-1.-1.-1")
	dagman.DAGManJobId.SetFromString( getenv( EnvGetName( ENV_ID ) ) );

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
	const DagVersionData MIN_SUBMIT_FILE_VERSION = { 7, 1, 2 };

		// Construct a string of the minimum submit file version.
	MyString minSubmitVersionStr;
	minSubmitVersionStr.formatstr( "%d.%d.%d",
				MIN_SUBMIT_FILE_VERSION.majorVer,
				MIN_SUBMIT_FILE_VERSION.minorVer,
				MIN_SUBMIT_FILE_VERSION.subMinorVer );
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    //
    // Process command-line arguments
    //
    for (i = 1; i < argc; i++) {
        if( !strcasecmp( "-Debug", argv[i] ) ) {
            i++;
            if( argc <= i || strcmp( argv[i], "" ) == 0 ) {
                debug_printf( DEBUG_SILENT, "No debug level specified\n" );
                Usage();
            }
            debug_level = (debug_level_t) atoi (argv[i]);
        } else if( !strcasecmp( "-Lockfile", argv[i] ) ) {
            i++;
            if( argc <= i || strcmp( argv[i], "" ) == 0 ) {
                debug_printf( DEBUG_SILENT, "No DagMan lockfile specified\n" );
                Usage();
            }
            lockFileName = argv[i];
        } else if( !strcasecmp( "-Help", argv[i] ) ) {
            Usage();
        } else if (!strcasecmp( "-Dag", argv[i] ) ) {
            i++;
            if( argc <= i || strcmp( argv[i], "" ) == 0 ) {
                debug_printf( DEBUG_SILENT, "No DAG specified\n" );
                Usage();
            }
			dagman.dagFiles.append( argv[i] );
        } else if( !strcasecmp( "-MaxIdle", argv[i] ) ) {
            i++;
            if( argc <= i || strcmp( argv[i], "" ) == 0 ) {
                debug_printf( DEBUG_SILENT,
							  "Integer missing after -MaxIdle\n" );
                Usage();
            }
            dagman.maxIdle = atoi( argv[i] );
        } else if( !strcasecmp( "-MaxJobs", argv[i] ) ) {
            i++;
            if( argc <= i || strcmp( argv[i], "" ) == 0 ) {
                debug_printf( DEBUG_SILENT,
							  "Integer missing after -MaxJobs\n" );
                Usage();
            }
            dagman.maxJobs = atoi( argv[i] );
        } else if( !strcasecmp( "-MaxScripts", argv[i] ) ) {
			debug_printf( DEBUG_SILENT, "-MaxScripts has been replaced with "
						   "-MaxPre and -MaxPost arguments\n" );
			Usage();
        } else if( !strcasecmp( "-MaxPre", argv[i] ) ) {
            i++;
            if( argc <= i || strcmp( argv[i], "" ) == 0 ) {
                debug_printf( DEBUG_SILENT,
							  "Integer missing after -MaxPre\n" );
                Usage();
            }
            dagman.maxPreScripts = atoi( argv[i] );
        } else if( !strcasecmp( "-MaxPost", argv[i] ) ) {
            i++;
            if( argc <= i || strcmp( argv[i], "" ) == 0 ) {
                debug_printf( DEBUG_SILENT,
							  "Integer missing after -MaxPost\n" );
                Usage();
            }
            dagman.maxPostScripts = atoi( argv[i] );
        } else if( !strcasecmp( "-NoEventChecks", argv[i] ) ) {
			debug_printf( DEBUG_QUIET, "Warning: -NoEventChecks is "
						"ignored; please use the DAGMAN_ALLOW_EVENTS "
						"config parameter instead\n");
			check_warning_strictness( DAG_STRICT_1 );

        } else if( !strcasecmp( "-AllowLogError", argv[i] ) ) {
			dagman.allowLogError = true;

        } else if( !strcasecmp( "-DontAlwaysRunPost",argv[i] ) ) {
			dagman._runPost = false;

        } else if( !strcasecmp( "-WaitForDebug", argv[i] ) ) {
			wait_for_debug = 1;

        } else if( !strcasecmp( "-UseDagDir", argv[i] ) ) {
			dagman.useDagDir = true;

        } else if( !strcasecmp( "-AutoRescue", argv[i] ) ) {
            i++;
            if( argc <= i || strcmp( argv[i], "" ) == 0 ) {
                debug_printf( DEBUG_SILENT, "No AutoRescue value specified\n" );
                Usage();
            }
            dagman.autoRescue = (atoi( argv[i] ) != 0);

        } else if( !strcasecmp( "-DoRescueFrom", argv[i] ) ) {
            i++;
            if( argc <= i || strcmp( argv[i], "" ) == 0 ) {
                debug_printf( DEBUG_SILENT, "No rescue DAG number specified\n" );
                Usage();
            }
            dagman.doRescueFrom = atoi (argv[i]);

        } else if( !strcasecmp( "-CsdVersion", argv[i] ) ) {
            i++;
            if( argc <= i || strcmp( argv[i], "" ) == 0 ) {
                debug_printf( DEBUG_SILENT, "No CsdVersion value specified\n" );
                Usage();
            }
			csdVersion = argv[i];

        } else if( !strcasecmp( "-AllowVersionMismatch", argv[i] ) ) {
			allowVerMismatch = true;

        } else if( !strcasecmp( "-DumpRescue", argv[i] ) ) {
			dagman.dumpRescueDag = true;

        } else if( !strcasecmp( "-verbose", argv[i] ) ) {
			dagman._submitDagDeepOpts.bVerbose = true;

        } else if( !strcasecmp( "-force", argv[i] ) ) {
			dagman._submitDagDeepOpts.bForce = true;
		
        } else if( !strcasecmp( "-notification", argv[i] ) ) {
            i++;
            if( argc <= i || strcmp( argv[i], "" ) == 0 ) {
                debug_printf( DEBUG_SILENT, "No notification value specified\n" );
                Usage();
            }
			dagman._submitDagDeepOpts.strNotification = argv[i];

		} else if( !strcasecmp( "-suppress_notification",argv[i] ) ) {
			dagman._submitDagDeepOpts.suppress_notification = true;

		} else if( !strcasecmp( "-dont_suppress_notification",argv[i] ) ) {
			dagman._submitDagDeepOpts.suppress_notification = false;

        } else if( !strcasecmp( "-dagman", argv[i] ) ) {
            i++;
            if( argc <= i || strcmp( argv[i], "" ) == 0 ) {
                debug_printf( DEBUG_SILENT, "No dagman value specified\n" );
                Usage();
            }
			dagman._submitDagDeepOpts.strDagmanPath = argv[i];

        } else if( !strcasecmp( "-outfile_dir", argv[i] ) ) {
            i++;
            if( argc <= i || strcmp( argv[i], "" ) == 0 ) {
                debug_printf( DEBUG_SILENT, "No outfile_dir value specified\n" );
                Usage();
            }
			dagman._submitDagDeepOpts.strOutfileDir = argv[i];

        } else if( !strcasecmp( "-update_submit", argv[i] ) ) {
			dagman._submitDagDeepOpts.updateSubmit = true;

        } else if( !strcasecmp( "-import_env", argv[i] ) ) {
			dagman._submitDagDeepOpts.importEnv = true;

        } else if( !strcasecmp( "-priority", argv[i] ) ) {
		++i;
		if( i >= argc || strcmp( argv[i], "" ) == 0 ) {
			debug_printf( DEBUG_NORMAL, "No priority value specified\n");
			Usage();
		}
		dagman._submitDagDeepOpts.priority = atoi(argv[i]);
		} else if( !strcasecmp( "-dont_use_default_node_log", argv[i] ) ) {
			dagman._submitDagDeepOpts.always_use_node_log = false;
        } else {
    		debug_printf( DEBUG_SILENT, "\nUnrecognized argument: %s\n",
						argv[i] );
			Usage();
		}
    }

	dagman.dagFiles.rewind();
	dagman.primaryDagFile = dagman.dagFiles.next();
	dagman.multiDags = (dagman.dagFiles.number() > 1);

	MyString tmpDefaultLog;
	if ( dagman._defaultNodeLog != NULL ) {
		tmpDefaultLog = dagman._defaultNodeLog;
		free( dagman._defaultNodeLog );
	} else {
		tmpDefaultLog = dagman.primaryDagFile + ".nodes.log";
	}

		// Force default log file path to be absolute so it works
		// with -usedagdir and DIR nodes.
	CondorError errstack;
	if ( !MultiLogFiles::makePathAbsolute( tmpDefaultLog, errstack) ) {
       	debug_printf( DEBUG_QUIET, "Unable to convert default log "
					"file name to absolute path: %s\n",
					errstack.getFullText().c_str() );
		dagman.dag->GetJobstateLog().WriteDagmanFinished( EXIT_ERROR );
		DC_Exit( EXIT_ERROR );
	}
	dagman._defaultNodeLog = strdup( tmpDefaultLog.Value() );
	debug_printf( DEBUG_NORMAL, "Default node log file is: <%s>\n",
				dagman._defaultNodeLog);

    //
    // Check the arguments
    //

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// Checking for version compatibility between the .condor.sub
	// file and this condor_dagman binary...

	// Note: if we're in recovery mode and the submit file version
	// causes us to quit, we leave any existing node jobs still
	// running -- may want to change that eventually.  wenger 2009-10-13.

		// Version of the condor_submit_dag that created our submit file.
	CondorVersionInfo submitFileVersion( csdVersion );

		// Version of this condor_dagman binary.
	CondorVersionInfo dagmanVersion;

		// Just generate this message fragment in one place.
	MyString versionMsg;
	versionMsg.formatstr("the version (%s) of this DAG's Condor submit "
				"file (created by condor_submit_dag)", csdVersion );

		// Make sure version in submit file is valid.
	if( !submitFileVersion.is_valid() ) {
		if ( !allowVerMismatch ) {
        	debug_printf( DEBUG_QUIET, "Error: %s is invalid!\n",
						versionMsg.Value() );
			DC_Exit( EXIT_ERROR );
		} else {
        	debug_printf( DEBUG_NORMAL, "Warning: %s is invalid; "
						"continuing because of -AllowVersionMismatch flag\n",
						versionMsg.Value() );
		}

		// Make sure .condor.sub file is recent enough.
	} else if ( submitFileVersion.compare_versions(
				CondorVersion() ) != 0 ) {

		if( !submitFileVersion.built_since_version(
					MIN_SUBMIT_FILE_VERSION.majorVer,
					MIN_SUBMIT_FILE_VERSION.minorVer,
					MIN_SUBMIT_FILE_VERSION.subMinorVer ) ) {
			if ( !allowVerMismatch ) {
        		debug_printf( DEBUG_QUIET, "Error: %s is older than "
							"oldest permissible version (%s)\n",
							versionMsg.Value(), minSubmitVersionStr.Value() );
				DC_Exit( EXIT_ERROR );
			} else {
        		debug_printf( DEBUG_NORMAL, "Warning: %s is older than "
							"oldest permissible version (%s); continuing "
							"because of -AllowVersionMismatch flag\n",
							versionMsg.Value(), minSubmitVersionStr.Value() );
			}

			// Warn if .condor.sub file is a newer version than this binary.
		} else if (dagmanVersion.compare_versions( csdVersion ) > 0 ) {
        	debug_printf( DEBUG_NORMAL, "Warning: %s is newer than "
						"condor_dagman version (%s)\n", versionMsg.Value(),
						CondorVersion() );
			check_warning_strictness( DAG_STRICT_3 );
		} else {
        	debug_printf( DEBUG_NORMAL, "Note: %s differs from "
						"condor_dagman version (%s), but the "
						"difference is permissible\n", 
						versionMsg.Value(), CondorVersion() );
		}
	}
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    if( dagman.primaryDagFile == "" ) {
        debug_printf( DEBUG_SILENT, "No DAG file was specified\n" );
        Usage();
    }
    if (lockFileName == NULL) {
        debug_printf( DEBUG_SILENT, "No DAG lock file was specified\n" );
        Usage();
    }
    if( dagman.maxJobs < 0 ) {
        debug_printf( DEBUG_SILENT, "-MaxJobs must be non-negative\n");
        Usage();
    }
    if( dagman.maxPreScripts < 0 ) {
        debug_printf( DEBUG_SILENT, "-MaxPre must be non-negative\n" );
        Usage();
    }
    if( dagman.maxPostScripts < 0 ) {
        debug_printf( DEBUG_SILENT, "-MaxPost must be non-negative\n" );
        Usage();
    }
    if( dagman.doRescueFrom < 0 ) {
        debug_printf( DEBUG_SILENT, "-DoRescueFrom must be non-negative\n" );
        Usage();
    }

    debug_printf( DEBUG_VERBOSE, "DAG Lockfile will be written to %s\n",
                   lockFileName );
	if ( dagman.dagFiles.number() == 1 ) {
    	debug_printf( DEBUG_VERBOSE, "DAG Input file is %s\n",
				  	dagman.primaryDagFile.Value() );
	} else {
		MyString msg = "DAG Input files are ";
		dagman.dagFiles.rewind();
		const char *dagFile;
		while ( (dagFile = dagman.dagFiles.next()) != NULL ) {
			msg += dagFile;
			msg += " ";
		}
		msg += "\n";
    	debug_printf( DEBUG_VERBOSE, "%s", msg.Value() );
	}

		// if requested, wait for someone to attach with a debugger...
	while( wait_for_debug ) { }

    {
		MyString cwd;
		if( !condor_getcwd(cwd) ) {
			cwd = "<null>";
		}
        debug_printf( DEBUG_DEBUG_1, "Current path is %s\n",cwd.Value());

		char *temp = my_username();
		debug_printf( DEBUG_DEBUG_1, "Current user is %s\n",
					   temp ? temp : "<null>" );
		if( temp ) {
			free( temp );
		}
    }

		//
		// Figure out the rescue DAG to run, if any (this is with "new-
		// style" rescue DAGs).
		//
	int rescueDagNum = 0;
	MyString rescueDagMsg;

	if ( dagman.doRescueFrom != 0 ) {
		rescueDagNum = dagman.doRescueFrom;
		rescueDagMsg.formatstr( "Rescue DAG number %d specified", rescueDagNum );
		RenameRescueDagsAfter( dagman.primaryDagFile.Value(),
					dagman.multiDags, rescueDagNum, dagman.maxRescueDagNum );

	} else if ( dagman.autoRescue ) {
		rescueDagNum = FindLastRescueDagNum(
					dagman.primaryDagFile.Value(),
					dagman.multiDags, dagman.maxRescueDagNum );
		rescueDagMsg.formatstr( "Found rescue DAG number %d", rescueDagNum );
	}

		//
		// Fill in values in the deep submit options that we haven't
		// already set.
		//
	dagman._submitDagDeepOpts.bAllowLogError = dagman.allowLogError;
	dagman._submitDagDeepOpts.useDagDir = dagman.useDagDir;
	dagman._submitDagDeepOpts.autoRescue = dagman.autoRescue;
	dagman._submitDagDeepOpts.doRescueFrom = dagman.doRescueFrom;
	dagman._submitDagDeepOpts.allowVerMismatch = allowVerMismatch;
	dagman._submitDagDeepOpts.recurse = false;

    //
    // Create the DAG
    //

	// Note: a bunch of the parameters we pass here duplicate things
	// in submitDagOpts, but I'm keeping them separate so we don't have to
	// bother to construct a new SubmitDagOtions object for splices.
	// wenger 2010-03-25
    dagman.dag = new Dag( dagman.dagFiles, dagman.maxJobs,
						  dagman.maxPreScripts, dagman.maxPostScripts,
						  dagman.allowLogError, dagman.useDagDir,
						  dagman.maxIdle, dagman.retrySubmitFirst,
						  dagman.retryNodeFirst, dagman.condorRmExe,
						  dagman.storkRmExe, &dagman.DAGManJobId,
						  dagman.prohibitMultiJobs, dagman.submitDepthFirst,
						  dagman._defaultNodeLog,
						  dagman._generateSubdagSubmits,
						  &dagman._submitDagDeepOpts,
						  false ); /* toplevel dag! */

    if( dagman.dag == NULL ) {
        EXCEPT( "ERROR: out of memory!\n");
    }

	dagman.dag->SetAbortOnScarySubmit( dagman.abortOnScarySubmit );
	dagman.dag->SetAllowEvents( dagman.allow_events );
	dagman.dag->SetConfigFile( dagman._dagmanConfigFile );
	dagman.dag->SetMaxJobHolds( dagman._maxJobHolds );
	dagman.dag->SetPostRun(dagman._runPost);
	if( dagman._submitDagDeepOpts.priority != 0 ) { // From command line
		dagman.dag->SetDefaultPriority(dagman._submitDagDeepOpts.priority);
	} else if( dagman._defaultPriority != 0 ) { // From config file
		dagman.dag->SetDefaultPriority(dagman._defaultPriority);
		dagman._submitDagDeepOpts.priority = dagman._defaultPriority;
	}

    //
    // Parse the input files.  The parse() routine
    // takes care of adding jobs and dependencies to the DagMan
    //
	dagman.mungeNodeNames = (dagman.dagFiles.number() > 1);
	parseSetDoNameMunge( dagman.mungeNodeNames );
   	debug_printf( DEBUG_VERBOSE, "Parsing %d dagfiles\n", 
		dagman.dagFiles.number() );
	dagman.dagFiles.rewind();
	char *dagFile;

	// Here we make a copy of the dagFiles for iteration purposes. Deep inside
	// of the parsing, copies of the dagman.dagFile string list happen which
	// mess up the iteration of this list.
	StringList sl( dagman.dagFiles );
	sl.rewind();
	while ( (dagFile = sl.next()) != NULL ) {
    	debug_printf( DEBUG_VERBOSE, "Parsing %s ...\n", dagFile );

    	if( !parse( dagman.dag, dagFile, dagman.useDagDir ) ) {
			if ( dagman.dumpRescueDag ) {
					// Dump the rescue DAG so we can see what we got
					// in the failed parse attempt.
    			debug_printf( DEBUG_QUIET, "Dumping rescue DAG "
							"because of -DumpRescue flag\n" );
				dagman.dag->Rescue( dagman.primaryDagFile.Value(),
							dagman.multiDags, dagman.maxRescueDagNum,
							false, true, false );
			}
			
			dagman.dag->RemoveRunningJobs(dagman, true);
			MSC_SUPPRESS_WARNING_FIXME(6031) // return falue of unlink ignored.
			unlink( lockFileName );
			dagman.CleanUp();
			
				// Note: debug_error calls DC_Exit().
        	debug_error( 1, DEBUG_QUIET, "Failed to parse %s\n",
					 	dagFile );
    	}
	}
	if( dagman.dag->GetDefaultPriority() != 0 ) {
		dagman.dag->SetDefaultPriorities(); // Applies to the nodes of the dag
	}
	dagman.dag->GetJobstateLog().WriteDagmanStarted( dagman.DAGManJobId );
	if ( rescueDagNum > 0 ) {
			// Get our Pegasus sequence numbers set correctly.
		dagman.dag->GetJobstateLog().InitializeRescue();
	}

	// lift the final set of splices into the main dag.
	dagman.dag->LiftSplices(SELF);

		//
		// Actually parse the "new-new" style (partial DAG info only)
		// rescue DAG here.  Note: this *must* be done after splices
		// are lifted!
		//
	if ( rescueDagNum > 0 ) {
		dagman.rescueFileToRun = RescueDagName(
					dagman.primaryDagFile.Value(),
					dagman.multiDags, rescueDagNum );
		debug_printf ( DEBUG_QUIET, "%s; running %s in combination with "
					"normal DAG file%s\n", rescueDagMsg.Value(),
					dagman.rescueFileToRun.Value(),
					dagman.multiDags ? "s" : "");
		debug_printf ( DEBUG_QUIET,
					"~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");

		debug_printf ( DEBUG_QUIET, "USING RESCUE DAG %s\n",
					dagman.rescueFileToRun.Value() );

			// Turn off node name munging for the rescue DAG, because
			// it will already have munged node names.
		parseSetDoNameMunge( false );

    	if( !parse( dagman.dag, dagman.rescueFileToRun.Value(),
					dagman.useDagDir ) ) {
			if ( dagman.dumpRescueDag ) {
					// Dump the rescue DAG so we can see what we got
					// in the failed parse attempt.
    			debug_printf( DEBUG_QUIET, "Dumping rescue DAG "
							"because of -DumpRescue flag\n" );
				dagman.dag->Rescue( dagman.primaryDagFile.Value(),
							dagman.multiDags, dagman.maxRescueDagNum,
							true, false );
			}
			
			dagman.dag->RemoveRunningJobs(dagman, true);
			MSC_SUPPRESS_WARNING_FIXME(6031) // return falue of unlink ignored.
			unlink( lockFileName );
			dagman.CleanUp();
			
				// Note: debug_error calls DC_Exit().
        	debug_error( 1, DEBUG_QUIET, "Failed to parse %s\n",
					 	dagFile );
    	}
	}

	dagman.dag->CheckThrottleCats();

	// fix up any use of $(JOB) in the vars values for any node
	dagman.dag->ResolveVarsInterpolations();

/*	debug_printf(DEBUG_QUIET, "COMPLETED DAG!\n");*/
/*	dagman.dag->PrintJobList();*/

#ifndef NOT_DETECT_CYCLE
	if( dagman.startup_cycle_detect && dagman.dag->isCycle() )
	{
		// Note: maybe we should run the final node here, if there is one.
		// wenger 2011-12-19.
		debug_error (1, DEBUG_QUIET, "ERROR: a cycle exists in the dag, please check input\n");
	}
#endif
    debug_printf( DEBUG_VERBOSE, "Dag contains %d total jobs\n",
				  dagman.dag->NumNodes( true ) );

	MyString firstLocation;
	if ( dagman.dag->GetReject( firstLocation ) ) {
    	debug_printf( DEBUG_QUIET, "Exiting because of REJECT "
					"specification in %s.  This most likely means "
					"that the DAG file was produced with the -DumpRescue "
					"flag when parsing the original DAG failed.\n",
					firstLocation.Value() );
		DC_Exit( EXIT_ERROR );
		return;
	}

	dagman.dag->DumpDotFile();

	if ( dagman.dumpRescueDag ) {
    	debug_printf( DEBUG_QUIET, "Dumping rescue DAG and exiting "
					"because of -DumpRescue flag\n" );
		dagman.dag->Rescue( dagman.primaryDagFile.Value(),
					dagman.multiDags, dagman.maxRescueDagNum, false,
					false, false );
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
  
    {
      bool recovery = access(lockFileName,  F_OK) == 0;
      
        if (recovery) {
            debug_printf( DEBUG_VERBOSE, "Lock file %s detected, \n",
                           lockFileName);
			if (dagman.abortDuplicates) {
				if (util_check_lock_file(lockFileName) == 1) {
        			debug_printf( DEBUG_QUIET, "Aborting because it "
							"looks like another instance of DAGMan is "
							"currently running on this DAG; if that is "
							"not the case, delete the lock file (%s) "
							"and re-submit the DAG.\n", lockFileName );
					dagman.dag->GetJobstateLog().
								WriteDagmanFinished( EXIT_RESTART );
    				dagman.CleanUp();
					DC_Exit( EXIT_ERROR );
					// We should never get to here!
				}
			}
        }

			//
			// If this DAGMan continues, it should overwrite the lock
			// file if it exists.
			//
		util_create_lock_file(lockFileName, dagman.abortDuplicates);

        debug_printf( DEBUG_VERBOSE, "Bootstrapping...\n");
        if( !dagman.dag->Bootstrap( recovery ) ) {
            dagman.dag->PrintReadyQ( DEBUG_DEBUG_1 );
            debug_error( 1, DEBUG_QUIET, "ERROR while bootstrapping\n");
        }
    }

    debug_printf( DEBUG_VERBOSE, "Registering condor_event_timer...\n" );
    daemonCore->Register_Timer( 1, dagman.m_user_log_scan_interval, 
				condor_event_timer, "condor_event_timer" );

	dagman.dag->SetPendingNodeReportInterval(
				dagman.pendingReportInterval );
}

void
print_status() {
	int total = dagman.dag->NumNodes( true );
	int done = dagman.dag->NumNodesDone( true );
	int pre = dagman.dag->PreRunNodeCount();
	int submitted = dagman.dag->NumJobsSubmitted();
	int post = dagman.dag->PostRunNodeCount();
	int ready =  dagman.dag->NumNodesReady();
	int failed = dagman.dag->NumNodesFailed();
	int unready = total - (done + pre + submitted + post + ready + failed );

	debug_printf( DEBUG_VERBOSE, "Of %d nodes total:\n", total );

	debug_printf( DEBUG_VERBOSE, " Done     Pre   Queued    Post   Ready   Un-Ready   Failed\n" );

	debug_printf( DEBUG_VERBOSE, "  ===     ===      ===     ===     ===        ===      ===\n" );

	debug_printf( DEBUG_VERBOSE, "%5d   %5d    %5d   %5d   %5d      %5d    %5d\n",
				  done, pre, submitted, post, ready, unready, failed );
	debug_printf( DEBUG_VERBOSE, "%d job proc(s) currently held\n",
				dagman.dag->NumHeldJobProcs() );
	dagman.dag->PrintDeferrals( DEBUG_VERBOSE, false );
}

void condor_event_timer () {

	ASSERT( dagman.dag != NULL );

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

	if( dagman.paused == true ) {
		debug_printf( DEBUG_DEBUG_1, "(DAGMan paused)\n" );
		return;
	}

    static int prevJobsDone = 0;
    static int prevJobs = 0;
    static int prevJobsFailed = 0;
    static int prevJobsSubmitted = 0;
    static int prevJobsReady = 0;
    static int prevScriptRunNodes = 0;
    static int prevJobsHeld = 0;

	int justSubmitted;
	justSubmitted = dagman.dag->SubmitReadyJobs(dagman);
	if( justSubmitted ) {
			// Note: it would be nice to also have the proc submit
			// count here.  wenger, 2006-02-08.
		debug_printf( DEBUG_VERBOSE, "Just submitted %d job%s this cycle...\n",
				  	justSubmitted, justSubmitted == 1 ? "" : "s" );
	}

	// If the log has grown
	if( dagman.dag->DetectCondorLogGrowth() ) {
		if( dagman.dag->ProcessLogEvents( CONDORLOG ) == false ) {
			debug_printf( DEBUG_NORMAL,
						"ProcessLogEvents(CONDORLOG) returned false\n" );
			dagman.dag->PrintReadyQ( DEBUG_DEBUG_1 );
			main_shutdown_rescue( EXIT_ERROR, Dag::DAG_STATUS_ERROR );
			return;
		}
	}

	if( dagman.dag->DetectDaPLogGrowth() ) {
		if( dagman.dag->ProcessLogEvents( DAPLOG ) == false ) {
			debug_printf( DEBUG_NORMAL,
						"ProcessLogEvents(DAPLOG) returned false\n" );
			dagman.dag->PrintReadyQ( DEBUG_DEBUG_1 );
			main_shutdown_rescue( EXIT_ERROR, Dag::DAG_STATUS_ERROR );
			return;
		}
	}

    // print status if anything's changed (or we're in a high debug level)
    if( prevJobsDone != dagman.dag->NumNodesDone( true )
        || prevJobs != dagman.dag->NumNodes( true )
        || prevJobsFailed != dagman.dag->NumNodesFailed()
        || prevJobsSubmitted != dagman.dag->NumJobsSubmitted()
        || prevJobsReady != dagman.dag->NumNodesReady()
        || prevScriptRunNodes != dagman.dag->ScriptRunNodeCount()
		|| prevJobsHeld != dagman.dag->NumHeldJobProcs()
		|| DEBUG_LEVEL( DEBUG_DEBUG_4 ) ) {
		print_status();

        prevJobsDone = dagman.dag->NumNodesDone( true );
        prevJobs = dagman.dag->NumNodes( true );
        prevJobsFailed = dagman.dag->NumNodesFailed();
        prevJobsSubmitted = dagman.dag->NumJobsSubmitted();
        prevJobsReady = dagman.dag->NumNodesReady();
        prevScriptRunNodes = dagman.dag->ScriptRunNodeCount();
		prevJobsHeld = dagman.dag->NumHeldJobProcs();
		
		if( dagman.dag->GetDotFileUpdate() ) {
			dagman.dag->DumpDotFile();
		}
	}

	dagman.dag->DumpNodeStatus( false, false );

    ASSERT( dagman.dag->NumNodesDone( true ) + dagman.dag->NumNodesFailed()
			<= dagman.dag->NumNodes( true ) );

    //
    // If DAG is complete, hurray, and exit.
    //
    if( dagman.dag->DoneSuccess( true ) ) {
        ASSERT( dagman.dag->NumJobsSubmitted() == 0 );
		dagman.dag->CheckAllJobs();
        debug_printf( DEBUG_NORMAL, "All jobs Completed!\n" );
		dagman.dag->PrintDeferrals( DEBUG_NORMAL, true );
		if ( dagman.dag->NumIdleJobProcs() != 0 ) {
			debug_printf( DEBUG_NORMAL, "Warning:  DAGMan thinks there "
						"are %d idle jobs, even though the DAG is "
						"completed!\n", dagman.dag->NumIdleJobProcs() );
			check_warning_strictness( DAG_STRICT_1 );
		}
		ExitSuccess();
		return;
    }

	//
	// DAG has failed -- dump rescue DAG.
	//
    if( dagman.dag->DoneFailed( true ) ) {
		main_shutdown_rescue( EXIT_ERROR, dagman.dag->_dagStatus );
		return;
	}

	//
	// DAG has succeeded but we haven't run final node yet, so do that.
	//
    if( dagman.dag->DoneSuccess( false ) ) {
		dagman.dag->StartFinalNode();
		return;
	}

		// If the DAG is halted, we don't want to actually exit yet if
		// jobs are still in the queue, or any POST scripts need to be
		// run (we need to run POST scripts so we don't "waste" jobs
		// that completed; on the other hand, we don't care about waiting
		// for PRE scripts because they'll be re-run when the rescue
		// DAG is run anyhow).
	if ( dagman.dag->IsHalted() && dagman.dag->NumJobsSubmitted() == 0 &&
				dagman.dag->PostRunNodeCount() == 0 &&
				!dagman.dag->RunningFinalNode() ) {
		debug_printf ( DEBUG_QUIET, "Exiting because DAG is halted "
					"and no jobs or scripts are running\n" );
		main_shutdown_rescue( EXIT_ERROR, Dag::DAG_STATUS_HALTED );
		return;
	}

    //
    // If no jobs are submitted and no scripts are running, but the
    // dag is not complete, then at least one job failed, or a cycle
    // exists.  (Note that if the DAG completed successfully, we already
	// returned from this function above.)
    // 
    if( dagman.dag->FinishedRunning( false ) ) {
		Dag::dag_status dagStatus = Dag::DAG_STATUS_OK;
		if( dagman.dag->DoneFailed( false ) ) {
			if( DEBUG_LEVEL( DEBUG_QUIET ) ) {
				debug_printf( DEBUG_QUIET,
							  "ERROR: the following job(s) failed:\n" );
				dagman.dag->PrintJobList( Job::STATUS_ERROR );
			}
			dagStatus = Dag::DAG_STATUS_NODE_FAILED;
		} else {
			// no jobs failed, so a cycle must exist
			debug_printf( DEBUG_QUIET, "ERROR: DAG finished but not all "
						"nodes are complete -- checking for a cycle...\n" );
			if( dagman.dag->isCycle() ) {
				debug_printf (DEBUG_QUIET, "... ERROR: a cycle exists "
							"in the dag, please check input\n");
				dagStatus = Dag::DAG_STATUS_CYCLE;
			} else {
				debug_printf (DEBUG_QUIET, "... ERROR: no cycle found; "
							"unknown error condition\n");
				dagStatus = Dag::DAG_STATUS_ERROR;
			}
			if ( debug_level >= DEBUG_NORMAL ) {
				dagman.dag->PrintJobList();
			}
		}

		main_shutdown_rescue( EXIT_ERROR, dagStatus );
		return;
    }
}


void
main_pre_dc_init( int, char*[] )
{
	DC_Skip_Auth_Init();
	DC_Skip_Core_Init();
#ifdef WIN32
	_setmaxstdio(2048);
#endif

		// Convert the DAGMan log file name to an absolute path if it's
		// not one already, so that we'll log things to the right file
		// if we change to a different directory.
	const char *	logFile = GetEnv( "_CONDOR_DAGMAN_LOG" );
	if ( logFile && !fullpath( logFile ) ) {
		MyString	currentDir;
		if ( condor_getcwd( currentDir ) ) {
			MyString newLogFile(currentDir);
			newLogFile += DIR_DELIM_STRING;
			newLogFile += logFile;
			SetEnv( "_CONDOR_DAGMAN_LOG", newLogFile.Value() );
		} else {
			debug_printf( DEBUG_NORMAL, "ERROR: unable to get cwd: %d, %s\n",
					errno, strerror(errno) );
		}
	}
}


int
main( int argc, char **argv )
{
	set_mySubSystem( "DAGMAN", SUBSYSTEM_TYPE_DAGMAN );

	dc_main_init = main_init;
	dc_main_config = main_config;
	dc_main_shutdown_fast = main_shutdown_fast;
	dc_main_shutdown_graceful = main_shutdown_graceful;
	dc_main_pre_dc_init = main_pre_dc_init;
	return dc_main( argc, argv );
}

