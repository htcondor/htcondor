#include "condor_common.h"
#include "dag.h"
#include "log.h"
#include "debug.h"
#include "parse.h"

#if 0  /* sig_install.h is broken, so I will defined my own installer */
#include "sig_install.h"
#else
//typedef void (*SIG_HANDLER)(int);
//void install_sig_handler (int sig, SIG_HANDLER handler);
#endif

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

// Required for linking with condor libs
extern "C" int SetSyscalls() { return 0; }

//---------------------------------------------------------------------------
static void Usage() {
  debug_printf (DEBUG_SILENT, "Usage: condor_dagman [-debug <level>] "
                "[-condorlog <file>] \n\t\t"
                "[-lockfile <file>] <file.dag>\n");
  exit(1);
}

//---------------------------------------------------------------------------
int main(int argc, char **argv) {

  debug_progname = argv[0];
  debug_level = DEBUG_NORMAL;  // Default debug level is normal output

#if !defined(WIN32)
  // the following is used to avoid 'broken pipe' messages
  install_sig_handler (SIGPIPE, SIG_IGN);
#endif
  
  char *condorLogName = "condor.log";
  char *lockFileName = ".dagmanLock";
  
  // DagMan recovery occurs if the file pointed to by 
  // lockFileName exists. This file is deleted on normal 
  // completion of condor_dagman

  // DAG_LOG is used for DagMan to maintain the order of events 
  // Recovery only uses the condor log
  // In this version, the DAG_LOG is not used

  char *datafile = NULL;
  
  Dag * dag = new Dag();
  
  if (argc < 2) Usage();  //  Make sure an input file was specified
  
  //
  // Process command-line arguments
  //
  for (int i = 1; i < argc; i++) {
    if (!strcmp("-debug", argv[i])) {
      i++;
      if (argc <= i) {
        debug_println (DEBUG_SILENT, "No debug level specified");
        Usage();
      }
      debug_level = (debug_level_t) atoi (argv[i]);
	} else if (!strcmp("-condorlog", argv[i])) {
      i++;
      if (argc <= i) {
        debug_println (DEBUG_SILENT, "No condor log specified");
		Usage();
      }
      condorLogName = argv[i];
    } else if (!strcmp("-lockfile", argv[i])) {
      i++;
      if (argc <= i) {
		debug_println (DEBUG_SILENT, "No DagMan lockfile specified");
		Usage();
      }
      lockFileName = argv[i];
    } else if (!strcmp("-help", argv[i])) {
      Usage();
    } else if (i == (argc - 1)) {
      datafile = argv[i];
    } else Usage();
  }
  
  if (datafile == NULL) {
    debug_println (DEBUG_SILENT, "No input file was specified");
    Usage();
  }
 
  debug_println (DEBUG_VERBOSE,"Condor log will be written to %s", condorLogName);
  debug_println (DEBUG_VERBOSE,"Dagman Lockfile will be written to %s",
                 lockFileName);
  debug_println (DEBUG_VERBOSE,"Input file is %s", datafile);
  
  //
  // Initialize the DagMan object
  //
  
  if (dag->Init(condorLogName, lockFileName) != OK) {
    debug_error (1, DEBUG_QUIET, "ERROR in DagMan initialization!");
  }
  
  //
  // Parse the input file.  The parse() routine
  // takes care of adding jobs and dependencies to the DagMan
  //
  debug_println (DEBUG_VERBOSE, "Parsing %s ...", datafile);
  if (!parse (datafile, dag)) {
    debug_error (1, DEBUG_QUIET, "Failed to parse %s", datafile);
  }

  debug_println (DEBUG_VERBOSE, "Dag contains %d total jobs",
                 dag->NumJobs());

  if (DEBUG_LEVEL(DEBUG_DEBUG_3)) dag->PrintJobList();
  
  // check if any jobs have been pre-defined as being done
  // if so, report this to the other nodes
  dag->TerminateFinishedJobs();

  debug_println (DEBUG_VERBOSE, "Number of Pre-completed Jobs: %d",
                 dag->NumJobsDone());
    
  debug_println (DEBUG_NORMAL, "Starting DagMan ...");
  
  // If the Lockfile exists, this indicates a premature termination
  // of a previous run of Dagman. If condor log is also present,
  // we run in recovery mode
  
  // If the Daglog is not present, then we do not run in recovery
  // mode
  
  if (access(lockFileName,  F_OK) == 0 &&
      access(condorLogName, F_OK) == 0) {

    debug_println (DEBUG_VERBOSE, "Lock file %s detected, "
                   "running in RECOVER mode", lockFileName);
    dag->Recover();

  } else {                                      // NORMAL MODE
    
    // if there is an older version of the log files,
    // we need to delete these.
    
    if ( access( condorLogName, F_OK) == 0 ) {
      debug_println (DEBUG_VERBOSE, "Deleting older version of %s",
                     condorLogName);
      if (remove (condorLogName) == -1)
        debug_perror (1, DEBUG_QUIET, condorLogName);
    }
    open(lockFileName, O_RDWR | O_CREAT, 0600);
  }

  while (dag->NumJobsDone() < dag->NumJobs()) {

    debug_println (DEBUG_DEBUG_2, "%s: Jobs Done: %d/%d", __FUNCTION__,
                   dag->NumJobsDone(), dag->NumJobs());

    if (DEBUG_LEVEL(DEBUG_DEBUG_3)) dag->PrintJobList();

    int ran = dag->SubmitReadyJobs();
    debug_println (DEBUG_DEBUG_2, "%s: Jobs Submitted: %d",
                   __FUNCTION__, ran);
    
    //
    // Wait for new events to be written into the condor log
    //
    
    // need to put a check here for the case
    // where the condor log doesn't exist (i.e., no jobs submitted)
    
    while (!dag->DetectLogGrowth());
    
    if (dag->ProcessLogEvents() != OK) {
      debug_error (1, DEBUG_QUIET, "ERROR processing condor log!");
	}

    // Nothing to do if all jobs have finished
  }
  
  debug_println (DEBUG_NORMAL, "All jobs Completed!");
  delete dag;
  return (0);
}
