/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2001 CONDOR Team, Computer Sciences Department, 
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
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "basename.h"
#include "dag.h"
#include "debug.h"
#include "parse.h"
#include "my_username.h"

//---------------------------------------------------------------------------
char* mySubSystem = "DAGMAN";         // used by Daemon Core

bool run_post_on_failure = TRUE;

char* lockFileName = NULL;
char* DAGManJobId;

// Required for linking with condor libs
extern "C" int SetSyscalls() { return 0; }

class Global {
  public:
    inline Global ():
        dag          (NULL),
        maxJobs      (0),
        maxPreScripts (0),
        maxPostScripts (0),
        rescue_file  (NULL),
        datafile     (NULL) {}
    inline void CleanUp () { delete dag; }
    Dag * dag;
    int maxJobs;  // Maximum number of Jobs to run at once
    int maxPreScripts;  // max. number of PRE scripts to run at once
    int maxPostScripts;  // max. number of POST scripts to run at once
    char *rescue_file;
    char *datafile;
};

Global G;


//---------------------------------------------------------------------------
static void Usage() {
    debug_printf( DEBUG_SILENT, "\nUsage: condor_dagman -f -t -l .\n"
            "\t\t[-Debug <level>]\n"
            "\t\t-Condorlog <NAME.dag.condor.log>\n"
            "\t\t-Lockfile <NAME.dag.lock>\n"
            "\t\t-Dag <NAME.dag>\n"
            "\t\t-Rescue <Rescue.dag>\n"
            "\t\t[-MaxJobs] <int N>\n\n"
            "\t\t[-MaxPre] <int N>\n\n"
            "\t\t[-MaxPost] <int N>\n\n"
            "\t\t[-NoPostFail]\n\n"
            "\twhere NAME is the name of your DAG.\n"
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
main_config()
{
	return 0;
}

int
main_shutdown_fast()
{
    DC_Exit( 1 );
    return FALSE;
}

int main_shutdown_graceful() {
    G.CleanUp();
	DC_Exit( 1 );
    return FALSE;
}

int main_shutdown_remove(Service *, int) {
    debug_printf( DEBUG_NORMAL, "Received SIGUSR1, removing running jobs\n" );
    G.dag->RemoveRunningJobs();
    if (G.rescue_file != NULL) {
        debug_printf( DEBUG_NORMAL, "Writing Rescue DAG file...\n" );
        G.dag->Rescue(G.rescue_file, G.datafile);
    }
	unlink( lockFileName ); 
	main_shutdown_graceful();
	return FALSE;
}

void main_timer();
void print_status();


//---------------------------------------------------------------------------
int main_init (int argc, char **argv) {

    // The DCpermission (last parm) should probably be PARENT, if it existed
    daemonCore->Register_Signal (DC_SIGUSR1, "DC_SIGUSR1",
                                 (SignalHandler) main_shutdown_remove,
                                 "main_shutdown_remove", NULL,
                                 IMMEDIATE_FAMILY);

    debug_progname = basename(argv[0]);
    debug_level = DEBUG_NORMAL;  // Default debug level is normal output

    char *condorLogName  = NULL;

    for (int i = 0 ; i < argc ; i++) {
        debug_printf( DEBUG_NORMAL, "argv[%d] == \"%s\"\n", i, argv[i] );
    }
  
    if (argc < 2) Usage();  //  Make sure an input file was specified

	// get dagman job id from environment
	DAGManJobId = getenv( "CONDOR_ID" );
	if( DAGManJobId == NULL ) {
		DAGManJobId = strdup( "unknown (requires condor_schedd >= v6.3)" );
	}
  
    //
    // Process command-line arguments
    //
    for (int i = 1; i < argc; i++) {
        if (!strcmp("-Debug", argv[i])) {
            i++;
            if (argc <= i) {
                debug_printf( DEBUG_SILENT, "No debug level specified\n" );
                Usage();
            }
            debug_level = (debug_level_t) atoi (argv[i]);
        } else if (!strcmp("-Condorlog", argv[i])) {
            i++;
            if (argc <= i) {
                debug_printf( DEBUG_SILENT, "No condor log specified" );
                Usage();
            }
            condorLogName = argv[i];
        } else if (!strcmp("-Lockfile", argv[i])) {
            i++;
            if (argc <= i) {
                debug_printf( DEBUG_SILENT, "No DagMan lockfile specified\n" );
                Usage();
            }
            lockFileName = argv[i];
        } else if (!strcmp("-Help", argv[i])) {
            Usage();
        } else if (!strcmp("-Dag", argv[i])) {
            i++;
            if (argc <= i) {
                debug_printf( DEBUG_SILENT, "No DAG specified\n" );
                Usage();
            }
            G.datafile = argv[i];
        } else if (!strcmp("-Rescue", argv[i])) {
            i++;
            if (argc <= i) {
                debug_printf( DEBUG_SILENT, "No Rescue DAG specified\n" );
                Usage();
            }
            G.rescue_file = argv[i];
        } else if (!strcmp("-MaxJobs", argv[i])) {
            i++;
            if (argc <= i) {
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
            if( argc <= i ) {
                debug_printf( DEBUG_SILENT,
							  "Integer missing after -MaxPre\n" );
                Usage();
            }
            G.maxPreScripts = atoi( argv[i] );
        } else if( !strcmp( "-MaxPost", argv[i] ) ) {
            i++;
            if( argc <= i ) {
                debug_printf( DEBUG_SILENT,
							  "Integer missing after -MaxPost\n" );
                Usage();
            }
            G.maxPostScripts = atoi( argv[i] );
        } else if( !strcmp( "-NoPostFail", argv[i] ) ) {
			run_post_on_failure = FALSE;
        } else Usage();
    }
  
    //
    // Check the arguments
    //

    if (G.datafile == NULL) {
        debug_printf( DEBUG_SILENT, "No DAG file was specified\n" );
        Usage();
    }
    if (condorLogName == NULL) {
        debug_printf( DEBUG_SILENT, "No Condor Log file was specified\n" );
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
 
    debug_printf( DEBUG_VERBOSE, "Condor log will be written to %s\n",
                   condorLogName);
    debug_printf( DEBUG_VERBOSE, "DAG Lockfile will be written to %s\n",
                   lockFileName);
    debug_printf( DEBUG_VERBOSE, "DAG Input file is %s\n", G.datafile);

    if (G.rescue_file != NULL) {
        debug_printf( DEBUG_VERBOSE, "Rescue DAG will be written to %s\n",
                       G.rescue_file);
    }

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
  
    G.dag = new Dag( condorLogName, G.maxJobs, G.maxPreScripts,
					 G.maxPostScripts );

    if( G.dag == NULL ) {
        EXCEPT( "ERROR: out of memory (%s() in %s:%d)!\n",
                __FUNCTION__, __FILE__, __LINE__ );
    }
  
    //
    // Parse the input file.  The parse() routine
    // takes care of adding jobs and dependencies to the DagMan
    //
    debug_printf( DEBUG_VERBOSE, "Parsing %s ...\n", G.datafile);
    if (!parse (G.datafile, G.dag)) {
        debug_error( 1, DEBUG_QUIET, "Failed to parse %s\n", G.datafile);
    }

    debug_printf( DEBUG_VERBOSE, "Dag contains %d total jobs\n",
                   G.dag->NumJobs());

    //------------------------------------------------------------------------
    // Bootstrap and Recovery
    //
    // If the Lockfile exists, this indicates a premature termination
    // of a previous run of Dagman. If condor log is also present,
    // we run in recovery mode
  
    // If the Daglog is not present, then we do not run in recovery
    // mode
  
    {
        bool recovery = (access(lockFileName,  F_OK) == 0 &&
                         access(condorLogName, F_OK) == 0);
    
        if (recovery) {
            debug_printf( DEBUG_VERBOSE, "Lock file %s detected, \n",
                           lockFileName);
        } else {
      
            // if there is an older version of the log files,
            // we need to delete these.
      
            if ( access( condorLogName, F_OK) == 0 ) {
                debug_printf( DEBUG_VERBOSE, "Deleting older version of %s\n",
                               condorLogName);
                if (remove (condorLogName) == -1)
                    debug_error( 1, DEBUG_QUIET, "Error: can't remove %s\n",
								 condorLogName );
            }

            touch (condorLogName);
            touch (lockFileName);
        }

        debug_printf( DEBUG_VERBOSE, "Bootstrapping...\n");
        if (!G.dag->Bootstrap (recovery)) {
            G.dag->PrintReadyQ( DEBUG_DEBUG_1 );
            debug_error( 1, DEBUG_QUIET, "ERROR while bootstrapping\n");
        }
    }

    daemonCore->Register_Timer( 1, 5, (TimerHandler)main_timer,
								"main_timer" );
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

void main_timer () {

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
    if (G.dag->DetectLogGrowth()) {
        if (G.dag->ProcessLogEvents() == false) {
			G.dag->PrintReadyQ( DEBUG_DEBUG_1 );
            debug_printf( DEBUG_QUIET, "Aborting DAG...\n"
                           "removing running jobs");
            G.dag->RemoveRunningJobs();
            debug_printf( DEBUG_NORMAL, "Writing Rescue DAG file...\n");
            G.dag->Rescue(G.rescue_file, G.datafile);
			unlink( lockFileName );
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

    assert (G.dag->NumJobsDone() + G.dag->NumJobsFailed() <= G.dag->NumJobs());

    //
    // If DAG is complete, hurray, and exit.
    //
    if( G.dag->Done() ) {
        assert (G.dag->NumJobsSubmitted() == 0);
        debug_printf( DEBUG_NORMAL, "All jobs Completed!\n" );
		G.CleanUp();
		DC_Exit( 0 );
    }

    //
    // If no jobs are submitted, but the dag is not complete,
    // then at least one job failed, or a cycle exists.
    // 
    if( G.dag->NumJobsSubmitted() == 0 &&
		G.dag->NumScriptsRunning() == 0 ) {
		if( DEBUG_LEVEL( DEBUG_QUIET ) ) {
			debug_printf( DEBUG_QUIET, "ERROR: the following job(s) failed:\n" );
			G.dag->PrintJobList( Job::STATUS_ERROR );
		}
		if( G.rescue_file != NULL ) {
			debug_printf( DEBUG_NORMAL, "Writing Rescue DAG file...\n");
			G.dag->Rescue(G.rescue_file, G.datafile);
		}
		else {
			debug_printf( DEBUG_NORMAL, "Rescue file not defined...\n" );
		}
		unlink( lockFileName );
		main_shutdown_graceful();
    }
}
