#include "condor_common.h"
#include "dag.h"
#include "log.h"
#include "debug.h"
#include "parse.h"

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
void touch (const char * filename) {
    int fd = open(filename, O_RDWR | O_CREAT, 0600);
    if (fd == -1) {
        debug_perror (1, DEBUG_QUIET, filename);
    }
    close (fd);
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
  
  debug_println (DEBUG_VERBOSE,
                 "This Dagman executable was compiled on %s at %s",
                 __DATE__, __TIME__);

  if (datafile == NULL) {
    debug_println (DEBUG_SILENT, "No input file was specified");
    Usage();
  }
 
  debug_println (DEBUG_VERBOSE,"Condor log will be written to %s",
                 condorLogName);
  debug_println (DEBUG_VERBOSE,"Dagman Lockfile will be written to %s",
                 lockFileName);
  debug_println (DEBUG_VERBOSE,"Input file is %s", datafile);
  
  //
  // Create the DAG
  //
  
  Dag * dag = new Dag (condorLogName, lockFileName);
  
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
      debug_println (DEBUG_VERBOSE, "Lock file %s detected, ", lockFileName);
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
    if (!dag->Bootstrap (recovery)) {
      if (DEBUG_LEVEL(DEBUG_DEBUG_1)) dag->Print_TermQ();
      debug_error (1, DEBUG_QUIET, "ERROR while bootstrapping");
    }
  }

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

  while (dag->NumJobsDone() < dag->NumJobs()) {

    debug_println (DEBUG_DEBUG_2, "%s: Jobs Done: %d/%d", __FUNCTION__,
                   dag->NumJobsDone(), dag->NumJobs());

    if (DEBUG_LEVEL(DEBUG_DEBUG_4)) dag->PrintJobList();

    // Wait for new events to be written into the condor log
    while (!dag->DetectLogGrowth());
    
    if (dag->ProcessLogEvents() == false) {
      if (DEBUG_LEVEL(DEBUG_DEBUG_1)) dag->Print_TermQ();
      debug_error (1, DEBUG_QUIET, "ERROR while processing condor log!");
	}
  }
  
  debug_println (DEBUG_NORMAL, "All jobs Completed!");
  delete dag;
  return 0;
}
