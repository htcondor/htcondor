#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "basename.h"
#include "dag.h"
#include "debug.h"
#include "parse.h"

//---------------------------------------------------------------------------
static char *_FileName_ = __FILE__;   // used by EXCEPT
char* mySubSystem = "DAGMAN";         // used by Daemon Core

// Required for linking with condor libs
extern "C" int SetSyscalls() { return 0; }

class Global {
  public:
    inline Global (): dag(NULL) {}
    inline void CleanUp () { delete dag; }
    Dag * dag;
};

Global G;

//---------------------------------------------------------------------------
static void Usage() {
    debug_printf (DEBUG_SILENT, "Usage: condor_dagman -f -t -l .\n"
                  "\t\t[-debug <level>]\n"
                  "\t\t-condorlog <NAME.dag.condor.log>\n"
                  "\t\t-lockfile <NAME.dag.lock>\n"
                  "\t\t-dag <NAME.dag>\n\n"
                  "\twhere NAME is the name of your DAG.\n"
                  "\tdefault -debug is -debug %d\n", DEBUG_NORMAL);
    DC_Exit(1);
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
    return TRUE;
}

int main_shutdown_fast() {
    G.CleanUp();
    DC_Exit(0);
    return TRUE;
}

int main_shutdown_graceful() {
    G.CleanUp();
    DC_Exit(0);
    return TRUE;
}

int main_shutdown_remove(Service *, int) {
    debug_println (DEBUG_NORMAL, "Received SIGUSR1, removing running jobs");
    G.dag->RemoveRunningJobs();
    G.CleanUp();
    DC_Exit(0);
    return TRUE;
}

void main_timer();


//---------------------------------------------------------------------------
int main_init (int argc, char **argv) {

    // The DCpermission (last parm) should probably be PARENT, if it existed
    daemonCore->Register_Signal (DC_SIGUSR1, "DC_SIGUSR1",
                                 (SignalHandler) main_shutdown_remove,
                                 "main_shutdown_remove", NULL, IMMEDIATE_FAMILY);

    debug_progname = basename(argv[0]);
    debug_level = DEBUG_NORMAL;  // Default debug level is normal output

    // I assume this is not needed with Daemon Core Present
    //#if !defined(WIN32)
    //  // the following is used to avoid 'broken pipe' messages
    //  install_sig_handler (SIGPIPE, SIG_IGN);
    //#endif
  
    char *condorLogName = NULL;
    char *lockFileName = NULL;
  
    // DagMan recovery occurs if the file pointed to by 
    // lockFileName exists. This file is deleted on normal 
    // completion of condor_dagman

    // DAG_LOG is used for DagMan to maintain the order of events 
    // Recovery only uses the condor log
    // In this version, the DAG_LOG is not used

    char *datafile = NULL;

    for (int i = 0 ; i < argc ; i++) {
        printf ("argv[%d] == \"%s\"\n", i, argv[i]);
    }
  
    if (argc < 2) Usage();  //  Make sure an input file was specified
  
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
            datafile = argv[i];
        } else Usage();
    }
  
    debug_println (DEBUG_VERBOSE,
                   "This Dagman executable was compiled on %s at %s",
                   __DATE__, __TIME__);

    //
    // Check the arguments
    //

    if (datafile == NULL) {
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
 
    debug_println (DEBUG_VERBOSE,"Condor log will be written to %s",
                   condorLogName);
    debug_println (DEBUG_VERBOSE,"DAG Lockfile will be written to %s",
                   lockFileName);
    debug_println (DEBUG_VERBOSE,"DAG Input file is %s", datafile);

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
  
    G.dag = new Dag (condorLogName, lockFileName);
  
    //
    // Parse the input file.  The parse() routine
    // takes care of adding jobs and dependencies to the DagMan
    //
    debug_println (DEBUG_VERBOSE, "Parsing %s ...", datafile);
    if (!parse (datafile, G.dag)) {
        debug_error (1, DEBUG_QUIET, "Failed to parse %s", datafile);
    }

    debug_println (DEBUG_VERBOSE, "Dag contains %d total jobs",
                   G.dag->NumJobs());

    if (DEBUG_LEVEL(DEBUG_DEBUG_3)) G.dag->PrintJobList();
  
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
            if (DEBUG_LEVEL(DEBUG_DEBUG_1)) G.dag->Print_TermQ();
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
    
    debug_println (DEBUG_DEBUG_2, "%s: Jobs Done: %d/%d", __FUNCTION__,
                   G.dag->NumJobsDone(), G.dag->NumJobs());
    
    // If the log has grown
    if (G.dag->DetectLogGrowth()) {
        if (G.dag->ProcessLogEvents() == false) {
            if (DEBUG_LEVEL(DEBUG_DEBUG_1)) G.dag->Print_TermQ();
            debug_error (1, DEBUG_QUIET, "ERROR while processing condor log!");
        }
    }
  
    if (G.dag->NumJobsDone() >= G.dag->NumJobs()) {
        debug_println (DEBUG_NORMAL, "All jobs Completed!");
        G.CleanUp();
        DC_Exit(0);
    }
}
