/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_config.h"
#include "condor_daemon_core.h"
#include "condor_string.h"
#include "basename.h"
#include "dag.h"
#include "debug.h"
#include "parse.h"
#include "my_username.h"
#include "condor_environ.h"
#include "dagman_main.h"
#include "dagman_commands.h"

void ExitSuccess();

//---------------------------------------------------------------------------
char* mySubSystem = "DAGMAN";         // used by Daemon Core

// the name of the attr we insert in job ads, recording DAGMan's job id
const char* DAGManJobIdAttrName = "DAGManJobID";

bool run_post_on_failure = TRUE;

static char* lockFileName = NULL;
char* DAGManJobId;

static Dagman dagman;

//---------------------------------------------------------------------------
static void Usage() {
    debug_printf( DEBUG_SILENT, "\nUsage: condor_dagman -f -t -l .\n"
            "\t\t[-Debug <level>]\n"
            "\t\t-Condorlog <NAME.dag.condor.log>\n"
	    "\t\t-Storklog <stork_userlog>\n"                       //-->DAP
            "\t\t-Lockfile <NAME.dag.lock>\n"
            "\t\t-Dag <NAME.dag>\n"
            "\t\t-Rescue <Rescue.dag>\n"
            "\t\t[-MaxJobs] <int N>\n\n"
            "\t\t[-MaxPre] <int N>\n\n"
            "\t\t[-MaxPost] <int N>\n\n"
            "\t\t[-NoPostFail]\n\n"
            "\t\t[-WaitForDebug]\n\n"
            "\t\t[-NoEventChecks]\n\n"
            "\t\t[-AllowLogError]\n\n"
            "\twherei NAME is the name of your DAG.\n"
            "\twhere N is Maximum # of Jobs to run at once "
            "(0 means unlimited)\n"
            "\tdefault -Debug is -Debug %d\n", DEBUG_NORMAL);
	DC_Exit( EXIT_ERROR );
}

//---------------------------------------------------------------------------


Dagman::Dagman() :
	dag (NULL),
	maxJobs (0),
	maxPreScripts (0),
	maxPostScripts (0),
	rescue_file (NULL),
	paused (false),
	submit_delay (0),
	max_submit_attempts (0),
	datafile (NULL),
	doEventChecks (true),
	allowLogError (false)
{
}


Dagman::~Dagman()
{
	delete dag;
}


bool
Dagman::Config()
{
	submit_delay = param_integer( "DAGMAN_SUBMIT_DELAY", 0, 0, 60 );
	max_submit_attempts =
		param_integer( "DAGMAN_MAX_SUBMIT_ATTEMPTS", 6, 1, 16 );
	startup_cycle_detect =
		param_boolean( "DAGMAN_STARTUP_CYCLE_DETECT", false );
	max_submits_per_interval =
		param_integer( "DAGMAN_MAX_SUBMITS_PER_INTERVAL", 5, 1, 1000 );
	stork_server = param( "STORK_SERVER" );
	allowExtraRuns = param_boolean(
			"DAGMAN_IGNORE_DUPLICATE_JOB_EXECUTION", false );
	retrySubmitFirst = param_boolean( "DAGMAN_RETRY_SUBMIT_FIRST", true );
	return true;
}


// NOTE: this is only called on reconfig, not at startup
int
main_config( bool is_full )
{
	dagman.Config();
	return 0;
}

// this is called by DC when the schedd is shutdown fast
int
main_shutdown_fast()
{
    DC_Exit( EXIT_ERROR );
    return FALSE;
}

// this can be called by other functions, or by DC when the schedd is
// shutdown gracefully
int main_shutdown_graceful() {
    dagman.CleanUp();
	DC_Exit( EXIT_ERROR );
    return FALSE;
}

int main_shutdown_rescue( int exitVal ) {
	debug_printf( DEBUG_QUIET, "Aborting DAG...\n" );
	if( dagman.dag ) {
		debug_printf( DEBUG_NORMAL, "Writing Rescue DAG to %s...\n",
					  dagman.rescue_file );
		dagman.dag->Rescue( dagman.rescue_file, dagman.datafile );
			// we write the rescue DAG *before* removing jobs because
			// otherwise if we crashed, failed, or were killed while
			// removing them, we would leave the DAG in an
			// unrecoverable state...
		debug_printf( DEBUG_DEBUG_1, "We have %d running jobs to remove\n",
					dagman.dag->NumJobsSubmitted() );
		if( dagman.dag->NumJobsSubmitted() > 0 ) {
			debug_printf( DEBUG_NORMAL, "Removing submitted jobs...\n" );
			dagman.dag->RemoveRunningJobs(dagman);
		}
		if ( dagman.dag->NumScriptsRunning() > 0 ) {
			debug_printf( DEBUG_NORMAL, "Removing running scripts...\n" );
			dagman.dag->RemoveRunningScripts(dagman);
		}
	}
	unlink( lockFileName ); 
    dagman.CleanUp();
	DC_Exit( exitVal );
	return false;
}

// this gets called by DC when DAGMan receives a SIGUSR1 -- which,
// assuming the DAGMan submit file was properly written, is the signal
// the schedd will send if the DAGMan job is removed from the queue
int main_shutdown_remove(Service *, int) {
    debug_printf( DEBUG_NORMAL, "Received SIGUSR1\n" );
	main_shutdown_rescue( EXIT_ABORT );
	return false;
}

void ExitSuccess() {
	unlink( lockFileName ); 
    dagman.CleanUp();
	DC_Exit( EXIT_OKAY );
}

void condor_event_timer();
void print_status();

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
int main_init (int argc, char ** const argv) {

	printf ("Executing condor dagman ... \n");

		// flag used if DAGMan is invoked with -WaitForDebug so we
		// wait for a developer to attach with a debugger...
	volatile int wait_for_debug = 0;

		// process any config vars
	dagman.Config();

#ifdef WIN32
	// on Windows, we'll EXCEPT() if we attempt to set_user_priv()
	// and we're not running as SYSTEM (aka root). So, if we're 
	// not SYSTEM, disable switching to prevent this.

	if ( is_root() == 0 ) {
		_condor_disable_uid_switching();
	}
#endif

	// The DCpermission (last parm) should probably be PARENT, if it existed
    daemonCore->Register_Signal( SIGUSR1, "SIGUSR1",
                                 (SignalHandler) main_shutdown_remove,
                                 "main_shutdown_remove", NULL,
                                 IMMEDIATE_FAMILY);

/****** FOR TESTING *******
    daemonCore->Register_Signal( SIGUSR2, "SIGUSR2",
                                 (SignalHandler) main_testing_stub,
                                 "main_testing_stub", NULL,
                                 IMMEDIATE_FAMILY );
****** FOR TESTING ********/
    debug_progname = basename(argv[0]);
    debug_level = DEBUG_NORMAL;  // Default debug level is normal output

    char *condorLogName  = NULL;
    char *dapLogName  = NULL;                      //<--DAP

	int i;
    for (i = 0 ; i < argc ; i++) {
        debug_printf( DEBUG_NORMAL, "argv[%d] == \"%s\"\n", i, argv[i] );
    }

    if (argc < 2) Usage();  //  Make sure an input file was specified

	// get dagman job id from environment
	DAGManJobId = getenv( EnvGetName( ENV_ID ) );
	if( DAGManJobId == NULL ) {
		DAGManJobId = strdup( "unknown (requires condor_schedd >= v6.3)" );
	}

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
        } else if( !strcasecmp( "-Condorlog", argv[i] ) ) {
            i++;
            if( argc <= i || strcmp( argv[i], "" ) == 0 ) {
                debug_printf( DEBUG_SILENT, "No condor log specified" );
                Usage();
           }
            condorLogName = argv[i];
		} else if( !strcasecmp( "-Storklog", argv[i] ) ) {
            i++;
            if (argc <= i) {
                debug_printf( DEBUG_SILENT, "No stork log specified" );
                Usage();
           }
            dapLogName = argv[i];        
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
            dagman.datafile = argv[i];
        } else if( !strcasecmp( "-Rescue", argv[i] ) ) {
            i++;
            if( argc <= i || strcmp( argv[i], "" ) == 0 ) {
                debug_printf( DEBUG_SILENT, "No Rescue DAG specified\n" );
                Usage();
            }
            dagman.rescue_file = argv[i];
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
        } else if( !strcasecmp( "-NoPostFail", argv[i] ) ) {
			run_post_on_failure = FALSE;

        } else if( !strcasecmp( "-NoEventChecks", argv[i] ) ) {
			dagman.doEventChecks = false;

        } else if( !strcasecmp( "-AllowLogError", argv[i] ) ) {
			dagman.allowLogError = true;

        } else if( !strcasecmp( "-WaitForDebug", argv[i] ) ) {
			wait_for_debug = 1;

        } else {
			Usage();
		}
    }
  
    //
    // Check the arguments
    //

    if( dagman.datafile == NULL ) {
        debug_printf( DEBUG_SILENT, "No DAG file was specified\n" );
        Usage();
    }
    if( !condorLogName && !dapLogName ) {
        debug_printf( DEBUG_SILENT,
					  "ERROR: no Condor or DaP log files were specified\n" );
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
    debug_printf( DEBUG_VERBOSE, "DAG Lockfile will be written to %s\n",
                   lockFileName);
    debug_printf( DEBUG_VERBOSE, "DAG Input file is %s\n",
				  dagman.datafile );

	if( dagman.rescue_file == NULL ) {
		MyString s = dagman.datafile;
		s += ".rescue";
		dagman.rescue_file = strnewp( s.Value() );
	}
	debug_printf( DEBUG_VERBOSE, "Rescue DAG will be written to %s\n",
				  dagman.rescue_file );

		// if requested, wait for someone to attach with a debugger...
	while( wait_for_debug );

    {
        char *temp;
        debug_printf( DEBUG_DEBUG_1, "Current path is %s\n",
                       (temp=getcwd(NULL, _POSIX_PATH_MAX)) ? temp : "<null>");
		if( temp ) {
			free( temp );
		}
		temp = my_username();
		debug_printf( DEBUG_DEBUG_1, "Current user is %s\n",
					   temp ? temp : "<null>" );
		if( temp ) {
			free( temp );
		}
    }
  
    //
    // Create the DAG
    //

    dagman.dag = new Dag( dagman.datafile, condorLogName, dagman.maxJobs,
						  dagman.maxPreScripts, dagman.maxPostScripts,
						  dagman.allowExtraRuns, dapLogName, //<-- DaP
						  dagman.allowLogError );

    if( dagman.dag == NULL ) {
        EXCEPT( "ERROR: out of memory!\n");
    }
    
    //
    // Parse the input file.  The parse() routine
    // takes care of adding jobs and dependencies to the DagMan
    //
    debug_printf( DEBUG_VERBOSE, "Parsing %s ...\n", dagman.datafile );
    if( !parse( dagman, dagman.datafile, dagman.dag ) ) {
        debug_error( 1, DEBUG_QUIET, "Failed to parse %s\n",
					 dagman.datafile );
    }

#ifndef NOT_DETECT_CYCLE
	if( dagman.startup_cycle_detect && dagman.dag->isCycle() )
	{
		debug_error (1, DEBUG_QUIET, "ERROR: a cycle exists in the dag, plese check input\n");
	}
#endif
    debug_printf( DEBUG_VERBOSE, "Dag contains %d total jobs\n",
				  dagman.dag->NumJobs() );

	dagman.dag->DumpDotFile();

    //------------------------------------------------------------------------
    // Bootstrap and Recovery
    //
    // If the Lockfile exists, this indicates a premature termination
    // of a previous run of Dagman. If condor log is also present,
    // we run in recovery mode
  
    // If the Daglog is not present, then we do not run in recovery
    // mode
  
    {
      bool recovery = ( (access(lockFileName,  F_OK) == 0 &&
			 access(condorLogName, F_OK) == 0) 
			|| (access(lockFileName,  F_OK) == 0 &&
			    access(dapLogName, F_OK) == 0) ) ; //--> DAP
      
        if (recovery) {
            debug_printf( DEBUG_VERBOSE, "Lock file %s detected, \n",
                           lockFileName);
        } else {
			dagman.dag->InitializeDagFiles( lockFileName );
        }

        debug_printf( DEBUG_VERBOSE, "Bootstrapping...\n");
        if( !dagman.dag->Bootstrap( dagman, recovery ) ) {
            dagman.dag->PrintReadyQ( DEBUG_DEBUG_1 );
            debug_error( 1, DEBUG_QUIET, "ERROR while bootstrapping\n");
        }
    }

	ASSERT( condorLogName || dapLogName );

    dprintf( D_ALWAYS, "Registering condor_event_timer...\n" );
    daemonCore->Register_Timer( 1, 5, (TimerHandler)condor_event_timer,
				"condor_event_timer" );

    return 0;
}

void
print_status() {
	int total = dagman.dag->NumJobs();
	int done = dagman.dag->NumJobsDone();
	int pre = dagman.dag->NumPreScriptsRunning();
	int submitted = dagman.dag->NumJobsSubmitted();
	int post = dagman.dag->NumPostScriptsRunning();
	int ready =  dagman.dag->NumJobsReady();
	int failed = dagman.dag->NumJobsFailed();
	int unready = total - (done + pre + submitted + post + ready + failed );

	debug_printf( DEBUG_VERBOSE, "Of %d nodes total:\n", total );

	debug_printf( DEBUG_VERBOSE, " Done     Pre   Queued    Post   Ready   Un-Ready   Failed\n" );

	debug_printf( DEBUG_VERBOSE, "  ===     ===      ===     ===     ===        ===      ===\n" );

	debug_printf( DEBUG_VERBOSE, "%5d   %5d    %5d   %5d   %5d      %5d    %5d\n",
				  done, pre, submitted, post, ready, unready, failed );
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
    static int prevScriptsRunning = 0;

	int justSubmitted;
	justSubmitted = dagman.dag->SubmitReadyJobs(dagman);
	if( justSubmitted ) {
		debug_printf( DEBUG_VERBOSE, "Just submitted %d job%s this cycle...\n",
					  justSubmitted, justSubmitted == 1 ? "" : "s" );
	}

    // If the log has grown
    if( dagman.dag->DetectCondorLogGrowth() ) {
		if( dagman.dag->ProcessLogEvents( dagman, CONDORLOG ) == false ) {
			dagman.dag->PrintReadyQ( DEBUG_DEBUG_1 );
			main_shutdown_rescue( EXIT_ERROR );
			return;
        }
    }

    if( dagman.dag->DetectDaPLogGrowth() ) {
      if( dagman.dag->ProcessLogEvents( dagman, DAPLOG ) == false ) {
debug_printf( DEBUG_NORMAL, "ProcessLogEvents(DAPLOG) returned false\n");
	dagman.dag->PrintReadyQ( DEBUG_DEBUG_1 );
	main_shutdown_rescue( EXIT_ERROR );
	return;
      }
    }
  
    // print status if anything's changed (or we're in a high debug level)
    if( prevJobsDone != dagman.dag->NumJobsDone()
        || prevJobs != dagman.dag->NumJobs()
        || prevJobsFailed != dagman.dag->NumJobsFailed()
        || prevJobsSubmitted != dagman.dag->NumJobsSubmitted()
        || prevJobsReady != dagman.dag->NumJobsReady()
        || prevScriptsRunning != dagman.dag->NumScriptsRunning()
		|| DEBUG_LEVEL( DEBUG_DEBUG_4 ) ) {
		print_status();
        prevJobsDone = dagman.dag->NumJobsDone();
        prevJobs = dagman.dag->NumJobs();
        prevJobsFailed = dagman.dag->NumJobsFailed();
        prevJobsSubmitted = dagman.dag->NumJobsSubmitted();
        prevJobsReady = dagman.dag->NumJobsReady();
        prevScriptsRunning = dagman.dag->NumScriptsRunning();
		
		if( dagman.dag->GetDotFileUpdate() ) {
			dagman.dag->DumpDotFile();
		}
	}

    ASSERT( dagman.dag->NumJobsDone() + dagman.dag->NumJobsFailed()
			<= dagman.dag->NumJobs() );

    //
    // If DAG is complete, hurray, and exit.
    //
    if( dagman.dag->Done() ) {
        ASSERT( dagman.dag->NumJobsSubmitted() == 0 );
		dagman.dag->CheckAllJobs(dagman);
        debug_printf( DEBUG_NORMAL, "All jobs Completed!\n" );
		ExitSuccess();
		return;
    }

    //

    // If no jobs are submitted and no scripts are running, but the
    // dag is not complete, then at least one job failed, or a cycle
    // exists.
    // 
    if( dagman.dag->NumJobsSubmitted() == 0 &&
		dagman.dag->NumJobsReady() == 0 &&
		dagman.dag->NumScriptsRunning() == 0 ) {
		if( dagman.dag->NumJobsFailed() > 0 ) {
			if( DEBUG_LEVEL( DEBUG_QUIET ) ) {
				debug_printf( DEBUG_QUIET,
							  "ERROR: the following job(s) failed:\n" );
				dagman.dag->PrintJobList( Job::STATUS_ERROR );
			}
		}
		else {
			// no jobs failed, so a cycle must exist
			debug_printf( DEBUG_QUIET, "ERROR: a cycle exists in the DAG\n" );
		}

		main_shutdown_rescue( EXIT_ERROR );
		return;
    }
}


void
main_pre_dc_init( int argc, char* argv[] )
{
	DC_Skip_Auth_Init();
}


void
main_pre_command_sock_init( )
{
}

