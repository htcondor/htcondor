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

//---------------------------------------------------------------------------
char* mySubSystem = "DAGMAN";         // used by Daemon Core

bool run_post_on_failure;	// for DAGMAN_RUN_POST_ON_FAILURE config setting

char* DAGManJobId;

// Required for linking with condor libs
extern "C" int SetSyscalls() { return 0; }

class Global {
  public:
    inline Global ():
        dag          (NULL),
        maxJobs      (0),
        maxScripts   (0),
        rescue_file  (NULL),
        datafile     (NULL) {}
    inline void CleanUp () { delete dag; }
    Dag * dag;
    int maxJobs;  // Maximum number of Jobs to run at once
    int maxScripts;  // max. number of PRE/POST scripts to run at once
    char *rescue_file;
    char *datafile;
};

Global G;


//---------------------------------------------------------------------------
static void Usage() {
    printf ("\nUsage: condor_dagman -f -t -l .\n"
            "\t\t[-Debug <level>]\n"
            "\t\t-Condorlog <NAME.dag.condor.log>\n"
            "\t\t-Lockfile <NAME.dag.lock>\n"
            "\t\t-Dag <NAME.dag>\n"
            "\t\t-Rescue <Rescue.dag>\n"
            "\t\t[-MaxJobs] <int N>\n\n"
            "\t\t[-MaxScripts] <int N>\n\n"
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
        debug_perror (1, DEBUG_QUIET, filename);
    }
    close (fd);
}

//---------------------------------------------------------------------------

int main_config() { 
	char* s = param( "DAGMAN_RUN_POST_ON_FAILURE" );
	if( s != NULL && !strcasecmp( s, "TRUE" ) ) {
		run_post_on_failure = TRUE;
	}	
	else {
		run_post_on_failure = FALSE;
	}
	if( s != NULL ) {
		free( s );
	}
    return TRUE;
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
    debug_println (DEBUG_NORMAL, "Received SIGUSR1, removing running jobs");
    G.dag->RemoveRunningJobs();
    if (G.rescue_file != NULL) {
        debug_println (DEBUG_NORMAL, "Writing Rescue DAG file...");
        G.dag->Rescue(G.rescue_file, G.datafile);
    }
	main_shutdown_graceful();
	return FALSE;
}

void main_timer();


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
    char *lockFileName   = NULL;

    for (int i = 0 ; i < argc ; i++) {
        printf ("argv[%d] == \"%s\"\n", i, argv[i]);
    }
  
    if (argc < 2) Usage();  //  Make sure an input file was specified

	// get dagman job id from environment
	DAGManJobId = getenv( "CONDOR_ID" );
	if( DAGManJobId == NULL ) {
		DAGManJobId = strdup( "unknown (requires condor_schedd >= v6.3.1)" );
	}
  
    //
    // Process command-line arguments
    //
    for (int i = 1; i < argc; i++) {
        if (!strcmp("-Debug", argv[i])) {
            i++;
            if (argc <= i) {
                debug_println (DEBUG_SILENT, "No debug level specified");
                Usage();
            }
            debug_level = (debug_level_t) atoi (argv[i]);
        } else if (!strcmp("-Condorlog", argv[i])) {
            i++;
            if (argc <= i) {
                debug_println (DEBUG_SILENT, "No condor log specified");
                Usage();
            }
            condorLogName = argv[i];
        } else if (!strcmp("-Lockfile", argv[i])) {
            i++;
            if (argc <= i) {
                debug_println (DEBUG_SILENT, "No DagMan lockfile specified");
                Usage();
            }
            lockFileName = argv[i];
        } else if (!strcmp("-Help", argv[i])) {
            Usage();
        } else if (!strcmp("-Dag", argv[i])) {
            i++;
            if (argc <= i) {
                debug_println (DEBUG_SILENT, "No DAG specified");
                Usage();
            }
            G.datafile = argv[i];
        } else if (!strcmp("-Rescue", argv[i])) {
            i++;
            if (argc <= i) {
                debug_println (DEBUG_SILENT, "No Rescue DAG specified");
                Usage();
            }
            G.rescue_file = argv[i];
        } else if (!strcmp("-MaxJobs", argv[i])) {
            i++;
            if (argc <= i) {
                debug_println (DEBUG_SILENT, "Integer missing after -MaxJobs");
                Usage();
            }
            G.maxJobs = atoi (argv[i]);
        } else if( !strcmp( "-MaxScripts", argv[i] ) ) {
            i++;
            if( argc <= i ) {
                debug_println( DEBUG_SILENT,
							   "Integer missing after -MaxScripts" );
                Usage();
            }
            G.maxScripts = atoi( argv[i] );
        } else Usage();
    }
  
    //
    // Check the arguments
    //

    if (G.datafile == NULL) {
        debug_println (DEBUG_SILENT, "No DAG file was specified");
        Usage();
    }
    if (condorLogName == NULL) {
        debug_println (DEBUG_SILENT, "No Condor Log file was specified");
        Usage();
    }
    if (lockFileName == NULL) {
        debug_println (DEBUG_SILENT, "No DAG lock file was specified");
        Usage();
    }
    if (G.maxJobs < 0) {
        debug_println (DEBUG_SILENT, "-MaxJobs must be non-negative");
        Usage();
    }
    if( G.maxScripts < 0 ) {
        debug_println( DEBUG_SILENT, "-MaxScripts must be non-negative" );
        Usage();
    }
 
    debug_println (DEBUG_VERBOSE,"Condor log will be written to %s",
                   condorLogName);
    debug_println (DEBUG_VERBOSE,"DAG Lockfile will be written to %s",
                   lockFileName);
    debug_println (DEBUG_VERBOSE,"DAG Input file is %s", G.datafile);

    if (G.rescue_file != NULL) {
        debug_println (DEBUG_VERBOSE,"Rescue DAG will be written to %s",
                       G.rescue_file);
    }

    {
        char *temp;
        debug_println (DEBUG_DEBUG_1,"Current path is %s",
                       (temp=getcwd(NULL, _POSIX_PATH_MAX)) ? temp : "<null>");
        free(temp);
        debug_println (DEBUG_DEBUG_1,"Current user is %s",
                       (temp=getenv("USER")) ? temp : "<null>");
    }
  
    //
    // Create the DAG
    //
  
    G.dag = new Dag( condorLogName, lockFileName, G.maxJobs, G.maxScripts );

    if( G.dag == NULL ) {
        EXCEPT( "ERROR: out of memory (%s() in %s:%d)!\n",
                __FUNCTION__, __FILE__, __LINE__ );
    }
  
    //
    // Parse the input file.  The parse() routine
    // takes care of adding jobs and dependencies to the DagMan
    //
    debug_println (DEBUG_VERBOSE, "Parsing %s ...", G.datafile);
    if (!parse (G.datafile, G.dag)) {
        debug_error (1, DEBUG_QUIET, "Failed to parse %s", G.datafile);
    }

    debug_println (DEBUG_VERBOSE, "Dag contains %d total jobs",
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
            debug_println (DEBUG_VERBOSE, "Lock file %s detected, ",
                           lockFileName);
        } else {
      
            // if there is an older version of the log files,
            // we need to delete these.
      
            if ( access( condorLogName, F_OK) == 0 ) {
                debug_println (DEBUG_VERBOSE, "Deleting older version of %s",
                               condorLogName);
                if (remove (condorLogName) == -1)
                    debug_perror (1, DEBUG_QUIET, condorLogName);
            }

            touch (condorLogName);
            touch (lockFileName);
        }

        debug_println (DEBUG_VERBOSE, "Bootstrapping...");
        if (!G.dag->Bootstrap (recovery)) {
            G.dag->PrintReadyQ( DEBUG_DEBUG_1 );
            debug_error (1, DEBUG_QUIET, "ERROR while bootstrapping");
        }
    }

    daemonCore->Register_Timer (2, 2, (TimerHandler) main_timer, "main_timer");
    return 0;
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
    
    debug_printf( DEBUG_DEBUG_1,
				  "%d Jobs Total / %d Done / %d Submitted / %d Ready / "
				  "%d Failed / %d Script%s Running\n", G.dag->NumJobs(),
				  G.dag->NumJobsDone(), G.dag->NumJobsSubmitted(),
				  G.dag->NumJobsReady(), G.dag->NumJobsFailed(), 
				  G.dag->NumScriptsRunning(),
				  G.dag->NumScriptsRunning() == 1 ? "" : "s" );
    
    // If the log has grown
    if (G.dag->DetectLogGrowth()) {
        if (G.dag->ProcessLogEvents() == false) {
			G.dag->PrintReadyQ( DEBUG_DEBUG_1 );
            debug_println (DEBUG_QUIET, "Aborting DAG..."
                           "removing running jobs");
            G.dag->RemoveRunningJobs();
            debug_println (DEBUG_NORMAL, "Writing Rescue DAG file...");
            G.dag->Rescue(G.rescue_file, G.datafile);
			main_shutdown_graceful();
			return;
        }
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
			printf( "ERROR: the following job(s) failed:\n" );
			G.dag->PrintJobList( Job::STATUS_ERROR );
		}
		if( G.rescue_file != NULL ) {
			debug_println (DEBUG_NORMAL, "Writing Rescue DAG file...");
			G.dag->Rescue(G.rescue_file, G.datafile);
		}
		else {
			debug_println( DEBUG_NORMAL, "Rescue file not defined..." );
		}
		main_shutdown_graceful();
    }
}
