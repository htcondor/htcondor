/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
#include "setenv.h"
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

static char* lockFileName = NULL;

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
            "\t\t[-MaxIdle] <int N>\n\n"
            "\t\t[-MaxJobs] <int N>\n\n"
            "\t\t[-MaxPre] <int N>\n\n"
            "\t\t[-MaxPost] <int N>\n\n"
            "\t\t[-WaitForDebug]\n\n"
            "\t\t[-NoEventChecks]\n\n"
            "\t\t[-AllowLogError]\n\n"
            "\t\t[-UseDagDir]\n\n"
            "\twherei NAME is the name of your DAG.\n"
            "\twhere N is Maximum # of Jobs to run at once "
            "(0 means unlimited)\n"
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
	rescue_file (NULL),
	paused (false),
	condorSubmitExe (NULL),
	condorRmExe (NULL),
	storkSubmitExe (NULL),
	storkRmExe (NULL),
	submit_delay (0),
	max_submit_attempts (0),
	primaryDagFile (NULL),
	allowLogError (false),
	useDagDir (false),
	deleteOldLogs (true)
{
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


bool
Dagman::Config()
{
		// Note: debug_printfs are DEBUG_NORMAL here because when we
		// get here we haven't processed command-line arguments yet.

	submit_delay = param_integer( "DAGMAN_SUBMIT_DELAY", 0, 0, 60 );
	debug_printf( DEBUG_NORMAL, "DAGMAN_SUBMIT_DELAY setting: %d\n",
				submit_delay );
	max_submit_attempts =
		param_integer( "DAGMAN_MAX_SUBMIT_ATTEMPTS", 6, 1, 16 );
	debug_printf( DEBUG_NORMAL, "DAGMAN_MAX_SUBMIT_ATTEMPTS setting: %d\n",
				max_submit_attempts );
	startup_cycle_detect =
		param_boolean( "DAGMAN_STARTUP_CYCLE_DETECT", false );
	debug_printf( DEBUG_NORMAL, "DAGMAN_STARTUP_CYCLE_DETECT setting: %d\n",
				startup_cycle_detect );
	max_submits_per_interval =
		param_integer( "DAGMAN_MAX_SUBMITS_PER_INTERVAL", 5, 1, 1000 );
	debug_printf( DEBUG_NORMAL, "DAGMAN_MAX_SUBMITS_PER_INTERVAL setting: %d\n",
				max_submits_per_interval );

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
			CheckEvents::ALLOW_DOUBLE_TERMINATE;

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
	}

		// Now get the new DAGMAN_ALLOW_EVENTS value -- that can override
		// all of the previous stuff.
	allow_events = param_integer("DAGMAN_ALLOW_EVENTS", allow_events);

	debug_printf( DEBUG_NORMAL, "allow_events ("
				"DAGMAN_IGNORE_DUPLICATE_JOB_EXECUTION, DAGMAN_ALLOW_EVENTS"
				") setting: %d\n", allow_events );

		// ...end of event checking setup.

	retrySubmitFirst = param_boolean( "DAGMAN_RETRY_SUBMIT_FIRST", true );
	debug_printf( DEBUG_NORMAL, "DAGMAN_RETRY_SUBMIT_FIRST setting: %d\n",
				retrySubmitFirst );

	retryNodeFirst = param_boolean( "DAGMAN_RETRY_NODE_FIRST", false );
	debug_printf( DEBUG_NORMAL, "DAGMAN_RETRY_NODE_FIRST setting: %d\n",
				retryNodeFirst );

	maxIdle =
		param_integer( "DAGMAN_MAX_JOBS_IDLE", maxIdle, 0, INT_MAX );
	debug_printf( DEBUG_NORMAL, "DAGMAN_MAX_JOBS_IDLE setting: %d\n",
				maxIdle );

	maxJobs =
		param_integer( "DAGMAN_MAX_JOBS_SUBMITTED", maxJobs, 0, INT_MAX );
	debug_printf( DEBUG_NORMAL, "DAGMAN_MAX_JOBS_SUBMITTED setting: %d\n",
				maxJobs );

	mungeNodeNames = param_boolean( "DAGMAN_MUNGE_NODE_NAMES", true );
	debug_printf( DEBUG_NORMAL, "DAGMAN_MUNGE_NODE_NAMES setting: %d\n",
				mungeNodeNames );

	deleteOldLogs = param_boolean( "DAGMAN_DELETE_OLD_LOGS", deleteOldLogs );
	debug_printf( DEBUG_NORMAL, "DAGMAN_DELETE_OLD_LOGS setting: %d\n",
				  deleteOldLogs );

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
		dagman.dag->Rescue( dagman.rescue_file, dagman.primaryDagFile,
					dagman.useDagDir );
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
			dagman.dag->RemoveRunningScripts();
		}
		dagman.dag->PrintDeferrals( DEBUG_NORMAL, true );
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

		// process any config vars -- this happens before we process
		// argv[], since arguments should override config settings
	dagman.Config();

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
    debug_progname = condor_basename(argv[0]);
    debug_level = DEBUG_NORMAL;  // Default debug level is normal output

    char *condorLogName  = NULL;
    char *dapLogName  = NULL;                      //<--DAP

	int i;
    for (i = 0 ; i < argc ; i++) {
        debug_printf( DEBUG_NORMAL, "argv[%d] == \"%s\"\n", i, argv[i] );
    }

    if (argc < 2) Usage();  //  Make sure an input file was specified

		// get dagman job id from environment, if it's there
		// (otherwise it will be set to "-1.-1.-1")
	dagman.DAGManJobId.SetFromString( getenv( EnvGetName( ENV_ID ) ) );

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
			dagman.dagFiles.append( argv[i]);
        } else if( !strcasecmp( "-Rescue", argv[i] ) ) {
            i++;
            if( argc <= i || strcmp( argv[i], "" ) == 0 ) {
                debug_printf( DEBUG_SILENT, "No Rescue DAG specified\n" );
                Usage();
            }
            dagman.rescue_file = argv[i];
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
			debug_printf( DEBUG_SILENT, "Warning: -NoEventChecks is "
						"deprecated; please use the DAGMAN_ALLOW_EVENTS "
						"config parameter instead\n");

        } else if( !strcasecmp( "-AllowLogError", argv[i] ) ) {
			dagman.allowLogError = true;

        } else if( !strcasecmp( "-WaitForDebug", argv[i] ) ) {
			wait_for_debug = 1;

        } else if( !strcasecmp( "-UseDagDir", argv[i] ) ) {
			dagman.useDagDir = true;

        } else {
    		debug_printf( DEBUG_SILENT, "\nUnrecognized argument: %s\n",
						argv[i] );
			Usage();
		}
    }

	dagman.dagFiles.rewind();
	dagman.primaryDagFile = dagman.dagFiles.next();
  
    //
    // Check the arguments
    //

    if( dagman.primaryDagFile == NULL ) {
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
	if ( dagman.dagFiles.number() == 1 ) {
    	debug_printf( DEBUG_VERBOSE, "DAG Input file is %s\n",
				  	dagman.primaryDagFile );
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

	if( dagman.rescue_file == NULL ) {
		MyString s = dagman.primaryDagFile;
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

    dagman.dag = new Dag( dagman.dagFiles, condorLogName, dagman.maxJobs,
						  dagman.maxPreScripts, dagman.maxPostScripts,
						  dagman.allow_events, dapLogName, //<-- DaP
						  dagman.allowLogError, dagman.useDagDir,
						  dagman.maxIdle, dagman.retrySubmitFirst,
						  dagman.retryNodeFirst, dagman.condorRmExe,
						  dagman.storkRmExe, &dagman.DAGManJobId );

    if( dagman.dag == NULL ) {
        EXCEPT( "ERROR: out of memory!\n");
    }

    //
    // Parse the input files.  The parse() routine
    // takes care of adding jobs and dependencies to the DagMan
    //
	if ( dagman.dagFiles.number() < 2 ) dagman.mungeNodeNames = false;
	parseSetDoNameMunge( dagman.mungeNodeNames );
	dagman.dagFiles.rewind();
	char *dagFile;
	while ( (dagFile = dagman.dagFiles.next()) != NULL ) {
    	debug_printf( DEBUG_VERBOSE, "Parsing %s ...\n",
					dagFile );
    	if( !parse( dagman.dag, dagFile, dagman.useDagDir ) ) {
				// Note: debug_error calls DC_Exit().
        	debug_error( 1, DEBUG_QUIET, "Failed to parse %s\n",
					 	dagFile );
    	}
	}
    
#ifndef NOT_DETECT_CYCLE
	if( dagman.startup_cycle_detect && dagman.dag->isCycle() )
	{
		debug_error (1, DEBUG_QUIET, "ERROR: a cycle exists in the dag, plese check input\n");
	}
#endif
    debug_printf( DEBUG_VERBOSE, "Dag contains %d total jobs\n",
				  dagman.dag->NumNodes() );

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
			dagman.dag->InitializeDagFiles( lockFileName,
 											dagman.deleteOldLogs );
        }

        debug_printf( DEBUG_VERBOSE, "Bootstrapping...\n");
        if( !dagman.dag->Bootstrap( recovery ) ) {
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
	int total = dagman.dag->NumNodes();
	int done = dagman.dag->NumNodesDone();
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
			dagman.dag->PrintReadyQ( DEBUG_DEBUG_1 );
			main_shutdown_rescue( EXIT_ERROR );
			return;
        }
    }

    if( dagman.dag->DetectDaPLogGrowth() ) {
      if( dagman.dag->ProcessLogEvents( DAPLOG ) == false ) {
	debug_printf( DEBUG_NORMAL,
			"ProcessLogEvents(DAPLOG) returned false\n");
	dagman.dag->PrintReadyQ( DEBUG_DEBUG_1 );
	main_shutdown_rescue( EXIT_ERROR );
	return;
      }
    }
  
    // print status if anything's changed (or we're in a high debug level)
    if( prevJobsDone != dagman.dag->NumNodesDone()
        || prevJobs != dagman.dag->NumNodes()
        || prevJobsFailed != dagman.dag->NumNodesFailed()
        || prevJobsSubmitted != dagman.dag->NumJobsSubmitted()
        || prevJobsReady != dagman.dag->NumNodesReady()
        || prevScriptRunNodes != dagman.dag->ScriptRunNodeCount()
		|| DEBUG_LEVEL( DEBUG_DEBUG_4 ) ) {
		print_status();
        prevJobsDone = dagman.dag->NumNodesDone();
        prevJobs = dagman.dag->NumNodes();
        prevJobsFailed = dagman.dag->NumNodesFailed();
        prevJobsSubmitted = dagman.dag->NumJobsSubmitted();
        prevJobsReady = dagman.dag->NumNodesReady();
        prevScriptRunNodes = dagman.dag->ScriptRunNodeCount();
		
		if( dagman.dag->GetDotFileUpdate() ) {
			dagman.dag->DumpDotFile();
		}
	}

    ASSERT( dagman.dag->NumNodesDone() + dagman.dag->NumNodesFailed()
			<= dagman.dag->NumNodes() );

    //
    // If DAG is complete, hurray, and exit.
    //
    if( dagman.dag->Done() ) {
        ASSERT( dagman.dag->NumJobsSubmitted() == 0 );
		dagman.dag->CheckAllJobs();
        debug_printf( DEBUG_NORMAL, "All jobs Completed!\n" );
		dagman.dag->PrintDeferrals( DEBUG_NORMAL, true );
		if ( dagman.dag->NumIdleJobProcs() != 0 ) {
			debug_printf( DEBUG_NORMAL, "Warning:  DAGMan thinks there "
						"are %d idle jobs, even though the DAG is "
						"completed!\n", dagman.dag->NumIdleJobProcs() );
		}
		ExitSuccess();
		return;
    }

    //

    // If no jobs are submitted and no scripts are running, but the
    // dag is not complete, then at least one job failed, or a cycle
    // exists.
    // 
    if( dagman.dag->NumJobsSubmitted() == 0 &&
		dagman.dag->NumNodesReady() == 0 &&
		dagman.dag->ScriptRunNodeCount() == 0 ) {
		if( dagman.dag->NumNodesFailed() > 0 ) {
			if( DEBUG_LEVEL( DEBUG_QUIET ) ) {
				debug_printf( DEBUG_QUIET,
							  "ERROR: the following job(s) failed:\n" );
				dagman.dag->PrintJobList( Job::STATUS_ERROR );
			}
		}
		else {
			// no jobs failed, so a cycle must exist
			debug_printf( DEBUG_QUIET, "ERROR: a cycle exists in the DAG\n" );
			if ( debug_level >= DEBUG_NORMAL ) {
				dagman.dag->PrintJobList();
			}
		}

		main_shutdown_rescue( EXIT_ERROR );
		return;
    }
}


void
main_pre_dc_init( int argc, char* argv[] )
{
	DC_Skip_Auth_Init();

		// Convert the DAGMan log file name to an absolute path if it's
		// not one already, so that we'll log things to the right file
		// if we change to a different directory.
	const char *	logFile = GetEnv( "_CONDOR_DAGMAN_LOG" );
	if ( logFile && !fullpath( logFile ) ) {
		char	currentDir[_POSIX_PATH_MAX];
		if ( getcwd( currentDir, sizeof(currentDir) ) ) {
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


void
main_pre_command_sock_init( )
{
}

