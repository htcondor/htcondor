#include "DagMan.h"
#include "log.h"

#include <pwd.h>
#include <unistd.h>
#include <iostream.h>

char *DAG_LOG = "dagman.log";
char *CONDOR_LOG = "condor.log";
char *DAGMAN_LOCKFILE = ".dagmanLock";
int   DEFAULT_SLEEP = 10;

int DagLog_fd;

DagMan *dagman;

static void Usage() {
  fprintf(stderr, "Usage: condor_dagman [-condorlog file] [-daglog file] "
          "[-lockfile file] <input filename>\n");
  exit(1);
}

int main(int argc, char **argv) {
  // DagMan recovery occurs if the file pointed to by 
  // DAGMAN_LOCKFILE exists. This file is deleted on normal 
  // completion of condor_dagman

  // DAG_LOG is used for DagMan to maintain the order of events 
  // Recovery only uses the condor log
  // In this version, the DAG_LOG is not used

  char *datafile = NULL;
  
  int parseStatus;
  int nJobsSubmitted;

  dagman = new DagMan();
  
  if (argc < 2) Usage();  //  Make sure an input file was specified
  
  //
  // Process command-line arguments
  //
  for (int i = 1; i < argc; i++) {
	if (!strcmp("-condorlog", argv[i])) {
      i++;
      if (argc <= i) {
        printf("No condor log specified\n");
		Usage();
      }
      CONDOR_LOG = argv[i];
    } else if (!strcmp("-daglog", argv[i])) {
      i++;
      if (argc <= i) {
		printf("No DagMan logfile name specified\n");
		Usage();
      }
      DAG_LOG = argv[i];
    } else if (!strcmp("-lockfile", argv[i])) {
      i++;
      if (argc <= i) {
		printf("No DagMan lockfile specified\n");
		Usage();
      }
      DAGMAN_LOCKFILE = argv[i];
    } else if (!strcmp("-help", argv[i])) {
      Usage();
    } else if (i == (argc - 1)) {
      datafile = argv[i];
    } else Usage();
  }
  
  if (datafile == NULL) {
    printf("No input file was specified\n");
    Usage();
  }
 

#ifdef VERBOSE
  fprintf(stderr, "Dagman log will be written to %s\n", 
          DAG_LOG);
  fprintf(stderr, "Condor log will be written to %s\n", 
          CONDOR_LOG);
  fprintf(stderr, "Dagman Lockfile will be written to %s\n", 
          DAGMAN_LOCKFILE);
  fprintf(stderr, "Input file is %s\n",
          datafile);
#endif
  
  //
  // Initialize the DagMan object
  //
  struct passwd *p;
  p = getpwuid(getuid());
  if (p == NULL) {
    fprintf(stderr, "Error looking up username\n");
    return (1);
  }
  
  int status = dagman->Init(CONDOR_LOG, 
                        DAG_LOG,
                        DAGMAN_LOCKFILE,
                        p->pw_name);
#ifdef DEBUG
  fprintf(stderr, "finished dagman->Init");
#endif

  if (status != OK) {
    fprintf(stderr, "ERROR in DagMan initialization: ");
    fprintf(stderr, "\n");
    return (1);
  }

  //
  // Open the input file, then parse it. The Parse() routine
  // takes care of adding jobs and dependencies to the DagMan
  //
  FILE *fp = fopen(datafile, "r");
  if (fp == NULL) {
    fprintf(stderr, "Could not open file %s for input\n", datafile);
    return (1);
  }

  parseStatus = Parse(fp, dagman);

  if (parseStatus != 0) {
    printf("Error Parsing\n");
    return (1);
  } 

#ifdef DEBUG
  printf("After Parsing ...\n");
  dagman->PrintNodeList();
#endif

#ifdef VERBOSE
  fprintf(stderr, "Starting DagMan ... \n");
#endif

  // check if any jobs have been pre-defined as being done
  // if so, report this to the other nodes

#ifdef DEBUG
  cout << "# of Jobs" << dagman->NumJobs() << endl;
  cout << "# of Jobs completed" << dagman->NumJobsCompleted() << endl;
#endif

  dagman->CheckForFinishedJobs();

  // If the Lockfile exists, this indicates a premature termination
  // of a previous run of Dagman. If condor log is also present,
  // we run in recovery mode

  // If the Daglog is not present, then we do not run in recovery
  // mode

  if ( (access(DAGMAN_LOCKFILE, F_OK) == 0) &&
       (access(CONDOR_LOG,      F_OK) == 0) &&
       (access(DAG_LOG,         F_OK) == 0) ) {     // RECOVERY MODE

    DagLog_fd = open(DAG_LOG, O_RDWR, 0600);
    dagman->Recover_DagMan();
  } else {                                          // NORMAL MODE

    // if there is an older version of the log files,
    // we need to delete these.
    
    if ( access( CONDOR_LOG, F_OK) == 0 ) {
#ifdef DEBUG
	  fprintf(stderr, "Deleting an older version of %s\n", CONDOR_LOG); 
#endif

	  FILE *fp;
	  char *DeleteCommand;
	  DeleteCommand = new char[strlen(CONDOR_LOG) + 10];

	  sprintf(DeleteCommand, "rm %s", CONDOR_LOG);
	  
	  fp = popen(DeleteCommand, "r");
	  if (fp == NULL) {
        fprintf(stderr, "popen in main() failed\n");
        return (1);
      }
	  
	  pclose(fp);
	}
    
    if ( access( DAG_LOG, F_OK) == 0 ) {
#ifdef DEBUG
	  fprintf(stderr, "Deleting an older version of %s\n", DAG_LOG); 
#endif
      
	  FILE *fp;
	  char *DeleteCommand;
	  DeleteCommand = new char[strlen(DAG_LOG) + 10];
      
	  sprintf(DeleteCommand, "rm %s", DAG_LOG);
	  
	  fp = popen(DeleteCommand, "r");
	  if (fp == NULL) {
        fprintf(stderr, "popen in main() failed\n");
        return (1);
      }
	  
	  pclose(fp);
	}
    
    DagLog_fd = open(DAG_LOG, O_RDWR | O_CREAT, 0600);
    open(DAGMAN_LOCKFILE, O_RDWR | O_CREAT, 0600);
  }

#ifdef DEBUG
  cout << "# of Jobs " << dagman->NumJobs() << endl;
  cout << "# of Jobs completed " << dagman->NumJobsCompleted() << endl;
#endif

  while (dagman->NumJobsCompleted() < dagman->NumJobs()) {

    status = dagman->RunReadyJobs(nJobsSubmitted);

    if (status != OK) {
	  fprintf(stderr, "ERROR submitting ready jobs: ");
	  fprintf(stderr, "\n");
	  return (1);
	}
      
    //
    // Wait for new events to be written into the condor log
    //
    
    // need to put a check here for the case
    // where the condor log doesnt exist (i.e., no jobs submitted)
    
    bool ready;
    do {
      // fprintf (stderr, "dagman->DetectLogGrowth(): "); fflush(stderr);
      ready = dagman->DetectLogGrowth();

      // fprintf (stderr, "dagman->WaitForLogEvents(): "); fflush(stderr);
      // ready = dagman->WaitForLogEvents();

      // fprintf (stderr, "%s\n", ready ? "true" : "false"); fflush(stderr);
    } while (!ready);

    fprintf (stderr, "dagman->ProcessLogEvents()\n"); fflush(stderr);
    status = dagman->ProcessLogEvents();
    if (status != OK) {
	  fprintf(stderr, "ERROR processing condor log.\n");
	  return (1);
	}
      
    // Nothing to do if all jobs have finished
    //
    // (This is not needed, look at the while loop condition above!)
    // if (dagman->NumJobsCompleted() == dagman->NumJobs()) break;

  }

#ifdef DEBUG  
  fprintf(stderr, "No more jobs to run\n"); 
#endif
  delete dagman;
  close(DagLog_fd);  

  return (0);
}
