/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2003 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
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

bool run_post_on_failure = TRUE;

char* lockFileName = NULL;
char* DAGManJobId;
char* DAP_SERVER = "skywalker.cs.wisc.edu";

Global G;

//---------------------------------------------------------------------------
static void Usage() {
    debug_printf( DEBUG_SILENT, "\nUsage: condor_dagman -f -t -l .\n"
            "\t\t[-Debug <level>]\n"
            "\t\t-Condorlog <NAME.dag.condor.log>\n"
	    "\t\t-Storklog <stork_userlog>\n"                       //-->DAP
	    "\t\t-Storkserver <stork server name>\n"                //-->DAP
            "\t\t-Lockfile <NAME.dag.lock>\n"
            "\t\t-Dag <NAME.dag>\n"
            "\t\t-Rescue <Rescue.dag>\n"
            "\t\t[-MaxJobs] <int N>\n\n"
            "\t\t[-MaxPre] <int N>\n\n"
            "\t\t[-MaxPost] <int N>\n\n"
            "\t\t[-NoPostFail]\n\n"
            "\twherei NAME is the name of your DAG.\n"
            "\twhere N is Maximum # of Jobs to run at once "
            "(0 means unlimited)\n"
            "\tdefault -Debug is -Debug %d\n", DEBUG_NORMAL);
	DC_Exit( 1 );
}

//---------------------------------------------------------------------------
void touch (const char * filename) {
    int fd = open(filename, O_RDWR | O_CREAT, 0600);
    if (fd == -1) {
        debug_error( 1, DEBUG_QUIET, "Error: can't open %s\n", filename );
    }
    close (fd);
}

//---------------------------------------------------------------------------

int
main_config( bool is_full )
{
		// nothing to read in from the config files here...
	return 0;
}

int
main_shutdown_fast()
{
    DC_Exit( 1 );
    return FALSE;
}

int main_shutdown_graceful() {
	debug_printf( DEBUG_QUIET, "Aborting DAG...\n" );
	if( G.dag ) {
		debug_printf( DEBUG_NORMAL, "Writing Rescue DAG to %s...\n",
					  G.rescue_file );
		G.dag->Rescue( G.rescue_file, G.datafile );
			// we write the rescue DAG *before* removing jobs because
			// otherwise if we crashed, failed, or were killed while
			// removing them, we would leave the DAG in an
			// unrecoverable state...
		if( G.dag->NumJobsSubmitted() > 0 ) {
			debug_printf( DEBUG_NORMAL, "Removing running jobs...\n" );
			G.dag->RemoveRunningJobs();
		}
	}
	unlink( lockFileName ); 
    G.CleanUp();
	DC_Exit( 1 );
    return FALSE;
}

int main_shutdown_remove(Service *, int) {
    debug_printf( DEBUG_NORMAL, "Received SIGUSR1\n" );
	main_shutdown_graceful();
	return false;
}

void ExitSuccess() {
	unlink( lockFileName ); 
    G.CleanUp();
	DC_Exit( 0 );
}

void condor_event_timer();
void dap_event_timer();
void print_status();

/****** FOR TESTING *******
int main_testing_stub( Service *, int ) {
	if( G.paused ) {
		ResumeDag();
	}
	else {
		PauseDag();
	}
	return true;
}
****** FOR TESTING ********/

//---------------------------------------------------------------------------
int main_init (int argc, char ** const argv) {

	printf ("Executing condor dagman ... \n");

		// flag used if DAGMan is invoked with -WaitForDebug so we
		// wait for a developer to attach with a debugger...
	int wait_for_debug = 0;
		
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
        if (!strcmp("-Debug", argv[i])) {
            i++;
            if( argc <= i || strcmp( argv[i], "" ) == 0 ) {
                debug_printf( DEBUG_SILENT, "No debug level specified\n" );
                Usage();
            }
            debug_level = (debug_level_t) atoi (argv[i]);
        } else if (!strcmp("-Condorlog", argv[i])) {
            i++;
            if( argc <= i || strcmp( argv[i], "" ) == 0 ) {
                debug_printf( DEBUG_SILENT, "No condor log specified" );
                Usage();
           }
            condorLogName = argv[i];
	//-->DAP
	} else if (!strcmp("-Storklog", argv[i])) {
            i++;
            if (argc <= i) {
                debug_printf( DEBUG_SILENT, "No stork log specified" );
                Usage();
           }
            dapLogName = argv[i];        
	} else if (!strcmp("-Storkserver", argv[i])) {
	    i++;
	    if (argc <= i) {
	        debug_printf( DEBUG_SILENT, "No stork server specified" );
	        Usage();
	  }
	    DAP_SERVER = argv[i];   
	//<--DAP
        } else if (!strcmp("-Lockfile", argv[i])) {
            i++;
            if( argc <= i || strcmp( argv[i], "" ) == 0 ) {
                debug_printf( DEBUG_SILENT, "No DagMan lockfile specified\n" );
                Usage();
            }
            lockFileName = argv[i];
        } else if (!strcmp("-Help", argv[i])) {
            Usage();
        } else if (!strcmp("-Dag", argv[i])) {
            i++;
            if( argc <= i || strcmp( argv[i], "" ) == 0 ) {
                debug_printf( DEBUG_SILENT, "No DAG specified\n" );
                Usage();
            }
            G.datafile = argv[i];
        } else if (!strcmp("-Rescue", argv[i])) {
            i++;
            if( argc <= i || strcmp( argv[i], "" ) == 0 ) {
                debug_printf( DEBUG_SILENT, "No Rescue DAG specified\n" );
                Usage();
            }
            G.rescue_file = argv[i];
        } else if (!strcmp("-MaxJobs", argv[i])) {
            i++;
            if( argc <= i || strcmp( argv[i], "" ) == 0 ) {
                debug_printf( DEBUG_SILENT,
							  "Integer missing after -MaxJobs\n" );
                Usage();
            }
            G.maxJobs = atoi (argv[i]);
        } else if( !strcmp( "-MaxScripts", argv[i] ) ) {
			debug_printf( DEBUG_SILENT, "-MaxScripts has been replaced with "
						   "-MaxPre and -MaxPost arguments\n" );
			Usage();
        } else if( !strcmp( "-MaxPre", argv[i] ) ) {
            i++;
            if( argc <= i || strcmp( argv[i], "" ) == 0 ) {
                debug_printf( DEBUG_SILENT,
							  "Integer missing after -MaxPre\n" );
                Usage();
            }
            G.maxPreScripts = atoi( argv[i] );
        } else if( !strcmp( "-MaxPost", argv[i] ) ) {
            i++;
            if( argc <= i || strcmp( argv[i], "" ) == 0 ) {
                debug_printf( DEBUG_SILENT,
							  "Integer missing after -MaxPost\n" );
                Usage();
            }
            G.maxPostScripts = atoi( argv[i] );
        } else if( !strcmp( "-NoPostFail", argv[i] ) ) {
			run_post_on_failure = FALSE;
        } else if( !strcmp( "-WaitForDebug", argv[i] ) ) {
			wait_for_debug = 1;
        } else Usage();
    }
  
    //
    // Check the arguments
    //

    if (G.datafile == NULL) {
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
    if (G.maxJobs < 0) {
        debug_printf( DEBUG_SILENT, "-MaxJobs must be non-negative\n");
        Usage();
    }
    if( G.maxPreScripts < 0 ) {
        debug_printf( DEBUG_SILENT, "-MaxPre must be non-negative\n" );
        Usage();
    }
    if( G.maxPostScripts < 0 ) {
        debug_printf( DEBUG_SILENT, "-MaxPost must be non-negative\n" );
        Usage();
    }
    debug_printf( DEBUG_VERBOSE, "DAG Lockfile will be written to %s\n",
                   lockFileName);
    debug_printf( DEBUG_VERBOSE, "DAG Input file is %s\n", G.datafile);

	if( G.rescue_file == NULL ) {
		MyString s = G.datafile;
		s += ".rescue";
		G.rescue_file = strnewp( s.Value() );
	}
	debug_printf( DEBUG_VERBOSE, "Rescue DAG will be written to %s\n",
				  G.rescue_file);

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

		// Attempt to get the log file name(s) from the submit files;
		// if that doesn't work, use the value from the command-line
		// argument.
    
	MyString dagFileName(G.datafile);
	MyString jobKeyword("job");
	MyString msg = ReadMultipleUserLogs::getJobLogsFromSubmitFiles(
				dagFileName, jobKeyword, G.condorLogFiles);

		// The "&& !dapLogName" check below is kind of a kludgey fix to allow
		// DaP jobs that have no "regular" Condor jobs to run.  Kent Wenger
		// (wenger@cs.wisc.edu) 2003-09-05.
	if( (msg != "" || G.condorLogFiles.number() == 0) && !dapLogName ) {

		G.condorLogFiles.rewind();
		while( G.condorLogFiles.next() ) {
		    G.condorLogFiles.deleteCurrent();
		}

		G.condorLogFiles.append(condorLogName);
	 }


	if( G.condorLogFiles.number() > 0 ) {
		G.condorLogFiles.rewind();
		debug_printf( DEBUG_VERBOSE, "Condor log will be written to %s, etc.\n",
					  G.condorLogFiles.next() );
	}

	if( dapLogName ) {
		debug_printf( DEBUG_VERBOSE, "DaP log will be written to %s\n",
					  dapLogName );
	}

    G.dag = new Dag( G.condorLogFiles, G.maxJobs, G.maxPreScripts,
		     G.maxPostScripts, dapLogName); //<-- DaP

    if( G.dag == NULL ) {
        EXCEPT( "ERROR: out of memory!\n");
    }
    
    //
    // Parse the input file.  The parse() routine
    // takes care of adding jobs and dependencies to the DagMan
    //
    debug_printf( DEBUG_VERBOSE, "Parsing %s ...\n", G.datafile);
    if (!parse (G.datafile, G.dag)) {
        debug_error( 1, DEBUG_QUIET, "Failed to parse %s\n", G.datafile);
    }

#ifndef NOT_DETECT_CYCLE
	if (G.dag->isCycle())
	{
		debug_error (1, DEBUG_QUIET, "ERROR: a cycle exists in the dag, plese check input\n");
	}
#endif
    debug_printf( DEBUG_VERBOSE, "Dag contains %d total jobs\n",
                   G.dag->NumJobs());

	G.dag->DumpDotFile();

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
      
            // if there is an older version of the log files,
            // we need to delete these.

			// pfc: why in the world would this be a necessary or good
			// thing?  apparently b/c old submit events from a
			// previous run of the same dag will contain the same dag
			// node names as those from current run, which will
			// completely f0rk the event-processing process (dagman
			// will see the events from the first run and think they
			// are from this one)...

			// instead of deleting the old logs, which seems evil,
			// maybe what we need is to add the submitting dagman
			// daemon's job id in each of the submit events in
			// addition to the dag node name we already write, so that
			// we can differentiate between identically-named nodes
			// from different dag instances.  (to take this to the
			// extreme, we might also want a unique schedd id in there
			// too...)

			debug_printf( DEBUG_VERBOSE,
						  "Deleting any older versions of log files...\n" );
      
            ReadMultipleUserLogs::DeleteLogs(G.condorLogFiles);

	    if ( access( dapLogName, F_OK) == 0 ) {
	      debug_printf( DEBUG_VERBOSE, "Deleting older version of %s\n",
			    dapLogName);
	      if (remove (dapLogName) == -1)
		debug_error( 1, DEBUG_QUIET, "Error: can't remove %s\n",
			     dapLogName );
            }

	    if (condorLogName != NULL) touch (condorLogName);  //<-- DAP
	    if (dapLogName != NULL) touch (dapLogName);
            touch (lockFileName);
        }

        debug_printf( DEBUG_VERBOSE, "Bootstrapping...\n");
        if (!G.dag->Bootstrap (recovery)) {
            G.dag->PrintReadyQ( DEBUG_DEBUG_1 );
            debug_error( 1, DEBUG_QUIET, "ERROR while bootstrapping\n");
        }
    }

	ASSERT( condorLogName || dapLogName );
    if( condorLogName ) {
		dprintf( D_ALWAYS, "Registering condor_event_timer...\n" );
		daemonCore->Register_Timer( 1, 5, (TimerHandler)condor_event_timer,
									"condor_event_timer" );
    }
    if( dapLogName ) {
		dprintf( D_ALWAYS, "Registering dap_event_timer...\n" );
		daemonCore->Register_Timer( 1, 5, (TimerHandler)dap_event_timer,
									"dap_event_timer" );
    }

    return 0;
}

void
print_status() {
	debug_printf( DEBUG_VERBOSE, "%d/%d done, %d failed, %d submitted, "
				  "%d ready, %d pre, %d post\n", G.dag->NumJobsDone(),
				  G.dag->NumJobs(), G.dag->NumJobsFailed(),
				  G.dag->NumJobsSubmitted(), G.dag->NumJobsReady(),
				  G.dag->NumPreScriptsRunning(),
				  G.dag->NumPostScriptsRunning() );
}

void condor_event_timer () {

	ASSERT( G.dag != NULL );

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

	if( G.paused == true ) {
		debug_printf( DEBUG_VERBOSE, "(DAGMan paused)\n" );
		return;
	}

    static int prevJobsDone = 0;
    static int prevJobs = 0;
    static int prevJobsFailed = 0;
    static int prevJobsSubmitted = 0;
    static int prevJobsReady = 0;
    static int prevScriptsRunning = 0;

    // If the log has grown
    if (G.dag->DetectCondorLogGrowth()) {              //-->DAP
      if (G.dag->ProcessLogEvents(CONDORLOG) == false) { //-->DAP
			G.dag->PrintReadyQ( DEBUG_DEBUG_1 );
			main_shutdown_graceful();
			return;
        }
    }
  
    // print status if anything's changed (or we're in a high debug level)
    if( prevJobsDone != G.dag->NumJobsDone()
        || prevJobs != G.dag->NumJobs()
        || prevJobsFailed != G.dag->NumJobsFailed()
        || prevJobsSubmitted != G.dag->NumJobsSubmitted()
        || prevJobsReady != G.dag->NumJobsReady()
        || prevScriptsRunning != G.dag->NumScriptsRunning()
		|| DEBUG_LEVEL( DEBUG_DEBUG_4 ) ) {
		print_status();
        prevJobsDone = G.dag->NumJobsDone();
        prevJobs = G.dag->NumJobs();
        prevJobsFailed = G.dag->NumJobsFailed();
        prevJobsSubmitted = G.dag->NumJobsSubmitted();
        prevJobsReady = G.dag->NumJobsReady();
        prevScriptsRunning = G.dag->NumScriptsRunning();
		
		if (G.dag->GetDotFileUpdate()) {
			G.dag->DumpDotFile();
		}
	}

    ASSERT( G.dag->NumJobsDone() + G.dag->NumJobsFailed() <= G.dag->NumJobs() );

    //
    // If DAG is complete, hurray, and exit.
    //
    if( G.dag->Done() ) {
        ASSERT( G.dag->NumJobsSubmitted() == 0 );
        debug_printf( DEBUG_NORMAL, "All jobs Completed!\n" );
		ExitSuccess();
		return;
    }

    //

    // If no jobs are submitted and no scripts are running, but the
    // dag is not complete, then at least one job failed, or a cycle
    // exists.
    // 
    if( G.dag->NumJobsSubmitted() == 0 &&
		G.dag->NumScriptsRunning() == 0 ) {
		if( G.dag->NumJobsFailed() > 0 ) {
			if( DEBUG_LEVEL( DEBUG_QUIET ) ) {
				debug_printf( DEBUG_QUIET,
							  "ERROR: the following job(s) failed:\n" );
				G.dag->PrintJobList( Job::STATUS_ERROR );
			}
		}
		else {
			// no jobs failed, so a cycle must exist
			debug_printf( DEBUG_QUIET, "ERROR: a cycle exists in the DAG\n" );
		}

		main_shutdown_graceful();
		return;
    }
}

//-->DAP
void dap_event_timer () {

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

    static int prevJobsDone = 0;
    static int prevJobs = 0;
    static int prevJobsFailed = 0;
    static int prevJobsSubmitted = 0;
    static int prevJobsReady = 0;
    static int prevScriptsRunning = 0;

    // If the log has grown
    if (G.dag->DetectDaPLogGrowth()) {

      if (G.dag->ProcessLogEvents(DAPLOG) == false) {
			G.dag->PrintReadyQ( DEBUG_DEBUG_1 );
			main_shutdown_graceful();
			return;
        }
    }
  
    // print status if anything's changed (or we're in a high debug level)
    if( prevJobsDone != G.dag->NumJobsDone()
        || prevJobs != G.dag->NumJobs()
        || prevJobsFailed != G.dag->NumJobsFailed()
        || prevJobsSubmitted != G.dag->NumJobsSubmitted()
        || prevJobsReady != G.dag->NumJobsReady()
        || prevScriptsRunning != G.dag->NumScriptsRunning()
		|| DEBUG_LEVEL( DEBUG_DEBUG_4 ) ) {
		print_status();
        prevJobsDone = G.dag->NumJobsDone();
        prevJobs = G.dag->NumJobs();
        prevJobsFailed = G.dag->NumJobsFailed();
        prevJobsSubmitted = G.dag->NumJobsSubmitted();
        prevJobsReady = G.dag->NumJobsReady();
        prevScriptsRunning = G.dag->NumScriptsRunning();
	}

    ASSERT( G.dag->NumJobsDone() + G.dag->NumJobsFailed() <= G.dag->NumJobs() );

    //
    // If DAG is complete, hurray, and exit.
    //
    if( G.dag->Done() ) {
        ASSERT( G.dag->NumJobsSubmitted() == 0 );
        debug_printf( DEBUG_NORMAL, "All jobs Completed!\n" );
		ExitSuccess();
		return;
    }

    //

    // If no jobs are submitted and no scripts are running, but the
    // dag is not complete, then at least one job failed, or a cycle
    // exists.
    // 
    if( G.dag->NumJobsSubmitted() == 0 &&
		G.dag->NumScriptsRunning() == 0 ) {
		if( G.dag->NumJobsFailed() > 0 ) {
			if( DEBUG_LEVEL( DEBUG_QUIET ) ) {
				debug_printf( DEBUG_QUIET,
							  "ERROR: the following job(s) failed:\n" );
				G.dag->PrintJobList( Job::STATUS_ERROR );
			}
		}
		else {
			// no jobs failed, so a cycle must exist
			debug_printf( DEBUG_QUIET, "ERROR: a cycle exists in the DAG\n" );
		}

		main_shutdown_graceful();
		return;
    }
}
//<--DAP


void
main_pre_dc_init( int argc, char* argv[] )
{
}


void
main_pre_command_sock_init( )
{
}

