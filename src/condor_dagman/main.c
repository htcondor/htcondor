
#include "HashTable.h"
#include "DagMan.h"
#include "util.h"
#include <pwd.h>
#include <unistd.h>
#include "log.h"
#include "RecoveryLog.h"
#include "RecoveryLogConstants.h"

static const int MAX_LINE_LENGTH = 100;
static const char *COMMENT_PREFIX = "#";
static const char *DELIMITERS = " \t";

char *DEFAULT_DAG_LOG = "dagman.log";
char *DEFAULT_CONDOR_LOG = "condor.log";
char *DEFAULT_RECOVERY_LOG = NULL;
int   DEFAULT_SLEEP = 10;
bool  DO_FAKE_SUBMIT = false;
char *DEFAULT_JOB_NUMBER = "001";
static int Parse(FILE *fp, DagMan *dagman);


//
// I would like to do a select() on the condor log file to see when
// new events have arrived, but right now select() is telling me that
// there is new data when actually there is not. So I'm conditionally
// remove the code that does the select and replacing it with a 
// periodic sleep(). Uncomment the following #define to use the select().
//
//#define DO_SELECT

static void
Usage()
{
    fprintf(stderr,
	    "Usage: dagman [-condorlog file] [-daglog file] [-sleep n]\n");
    fprintf(stderr, "         [-jobnumber n] [-help] <input file> \n");
}

int main(int argc, char **argv)
{
    int i;
    DagStatus status;
    DagMan dagman;
    FILE *fp;
    char *datafile = NULL;
    int parseStatus;
    int nJobsSubmitted;
    FILE *logfp;   //Global declaration for recovery log file.
    //
    // Make sure an input file was specified
    //
    if (argc < 2)
    {
	Usage();
	exit(1);
    }
    
    //
    // Process command-line arguments
    //
    for (i = 1; i < argc; i++)
    {
	if (!strcmp("-condorlog", argv[i]))
	{
	    i++;
	    if (argc <= i)
	    {
	        printf("No condor log specified\n");
		Usage();
		exit(1);
	    }
	    DEFAULT_CONDOR_LOG = argv[i];
	}
	else if (!strcmp("-daglog", argv[i]))
	{
	    i++;
	    if (argc <= i)
	    {
		printf("No DAG log specified\n");
		Usage();
		exit(1);
	    }
	    DEFAULT_DAG_LOG = argv[i];
	}
	else if (!strcmp("-fake", argv[i]))
	{
	    DO_FAKE_SUBMIT = true;
	}
	else if (!strcmp("-sleep", argv[i]))
	{
	    i++;
	    if (argc <= i)
	    {
		printf("No sleep value specified\n");
		Usage();
		exit(1);
	    }
	    DEFAULT_SLEEP = atoi(argv[i]);
	}
	else if (!strcmp("-jobnumber", argv[i]))
	{
	    i++;
	    if (argc <= i)
	    {
		printf("No job number specified\n");
		Usage();
		exit(1);
	    }
	    DEFAULT_JOB_NUMBER = argv[i];
	}

	else if (!strcmp("-help", argv[i]))
	{
	    Usage();
	    exit(1);
	}
	else if (i == (argc - 1))
	{
	    datafile = argv[i];
	}
	else
	{
	    Usage();
	    exit(1);
	}
      }

    if (datafile == NULL)
    {
	printf("No input file was specified\n");
	Usage();
	exit(1);
    }
    
    delete [] DEFAULT_RECOVERY_LOG;
    DEFAULT_RECOVERY_LOG = new char [strlen(datafile)+ 
				     5*sizeof(char) + 
				     strlen(DEFAULT_JOB_NUMBER) + 1];

    sprintf(DEFAULT_RECOVERY_LOG,"%s.%s.log",datafile,DEFAULT_JOB_NUMBER);

    fprintf(stderr, "Dagman log will be written to %s\n", 
	    DEFAULT_DAG_LOG);
    fprintf(stderr, "Condor log will be written to %s\n", 
	    DEFAULT_CONDOR_LOG);
    fprintf(stderr, "Dagman recovery log will be written to %s\n", 
	    DEFAULT_RECOVERY_LOG);
    fprintf(stderr, "Input file is %s\n",
	    datafile);
    if (DO_FAKE_SUBMIT)
	fprintf(stderr, "Fake condor submits will be used\n");
    


    //
    // Initialize the DagMan object
    //
    struct passwd *p;
    p = getpwuid(getuid());
    if (p == NULL)
    {
	fprintf(stderr, "Error looking up username\n");
	exit(1);
    }


    status = dagman.Init(DEFAULT_CONDOR_LOG, DEFAULT_DAG_LOG,
			 DEFAULT_RECOVERY_LOG,p->pw_name);
    if (!status)
    {
	fprintf(stderr, "ERROR in DagMan initialization: ");
	status.PrintError(stderr);
	fprintf(stderr, "\n");
	return 1;
    }
    
    //
    // Open the input file, then parse it. The Parse() routine
    // takes care of adding jobs and dependencies to the DagMan
    //
    fp = fopen(datafile, "r");
    if (fp == NULL)
    {
	fprintf(stderr, "Could not open file %s for input\n", datafile);
	return 1;
    }

    //Check whether to enter Recovery or Normal Mode

    if (access(DEFAULT_RECOVERY_LOG,F_OK) != 0)
    {
	//file already exists
	printf("File already exists::Entering Recovery Mode\n");
	logfp = fopen(DEFAULT_RECOVERY_LOG,"a");
	if (logfp == NULL)
	{
	    fprintf(stderr, "Could not open file %s for recovery\n", 
	    DEFAULT_RECOVERY_LOG);
	    return 1;
	}    
     
	LogDagManInitialize *log1;
	log1 = new LogDagManInitialize(datafile);
	log1->Write(logfp);
	delete log1;
     
   
	parseStatus = Parse(fp, &dagman);
	if (parseStatus != 0)
        {
	    return 1; 
        }
     
     
	LogDagManDoneInitialize *log2;
	log2 = new LogDagManDoneInitialize("DoneInitializingDagMan");
	log2->Write(logfp);
	delete log2;
	
	Ship_ObjList *l = dagman.GetShipObjList();
	l->display();
     
	fclose(logfp);
    }
    else
    {
        printf("Recovery File does not exists::Entering Normal Mode\n");
	parseStatus = Parse(fp, &dagman);
	if (parseStatus != 0)
	{
	  return 1; 
	} 
	Ship_ObjList *l = dagman.GetShipObjList();
	l->display();
	
	printf("Calling RunRecovery .. \n");
	dagman.RunRecovery();
	printf("Done with RunRecovery...\n");
    }
    
    //
      // Loop until all jobs have finished
	//
    
	  while (dagman.NumJobsCompleted() < dagman.NumJobs())
    {
#ifdef DEBUG
	printf("DagMan state at beginning of loop:\n");
	dagman.Dump();
#endif
	
	status = dagman.RunReadyJobs(nJobsSubmitted);
	if (!status)
	{
	    fprintf(stderr, "ERROR submitting ready jobs: ");
	    status.PrintError(stderr);
	    fprintf(stderr, "\n");
	    return 1;
	}
	
#ifdef DO_SELECT
	//
	// Wait for new events to be written into the condor log
	//
	bool ready;
	while ((ready = dagman.WaitForLogEvents()) == false)
	    ;
#endif
	
	status = dagman.ProcessLogEvents();
	if (!status)
	{
	    fprintf(stderr, "ERROR processing condor log: ");
	    status.PrintError(stderr);
	    fprintf(stderr, "\n");
	    return 1;
	}
	
	// Nothing to do if all jobs have finished
	if (dagman.NumJobsCompleted() == dagman.NumJobs())
	{
	    break;
	}
	
#ifndef DO_SELECT
	//
	// Only sleep if no jobs are ready to run
	//
	if (dagman.NumJobsReady() == 0)
	{
#ifdef DEBUG
	    printf("Sleeping for %d seconds...", DEFAULT_SLEEP);
	    fflush(stdout);
#endif // DEBUG
	    sleep(DEFAULT_SLEEP);
#ifdef DEBUG
	    printf("done\n");
	    fflush(stdout);
#endif // DEBUG
	}
#endif // DO_SELECT
	
    }
    
    fprintf(stderr, "No more jobs to run\n");
    

    if(unlink(DEFAULT_RECOVERY_LOG)<0)
	fprintf(stderr,"Log file could not be deleted\n"); 

    return 0;

}

static int
Parse(FILE *fp, DagMan *dagman)
{
    char line[MAX_LINE_LENGTH + 1];
    bool done;
    JobID jobid;
    JobID parent;
    JobID child;
    bool found;
    int len;
    char *command;
    char *arg1;
    char *arg2;
    int lineNumber;
    JobInfo info;
    DagStatus status;

    //
    // The input file uses names to identify jobs, and the DagMan class
    // uses integers. This hash table will provide a mapping from a job
    // name to a job id.
    //
    HashTable<char *, JobID> ht(101, StringHash, StringCompare,
				StringDup, true);

    done = false;
    found = false;
    lineNumber = 0;

    //
    // This loop will read every line of the input file
    //
    while (!done && (len = getline(fp, line, MAX_LINE_LENGTH + 1)) >= 0)
    {
	lineNumber++;
	
	//
	// Ignore blank lines
	//
	if (len == 0)
	{
	    continue;
	}

	//
	// Ignore comments
	//
	if (!strncmp(line, COMMENT_PREFIX, strlen(COMMENT_PREFIX)))
	{
	    continue;
	}
	
	command = strtok(line, DELIMITERS);

	//
	// Handle a Job command
	//
	if (!strcasecmp(command, "Job"))
	{
	    // Next token is the job name
	    arg1 = strtok(0, DELIMITERS);
	    if (arg1 == NULL)
	    {
		fprintf(stderr, "WARNING: invalid command on line %d: %s\n",
			lineNumber, line);
		continue;
	    }
	    
	    // Next token is the condor command file
	    arg2 = strtok(0, DELIMITERS);
	    if (arg2 == NULL)
	    {
		fprintf(stderr, "WARNING: invalid command on line %d: %s\n",
			lineNumber, line);
		continue;
	    }

	    info.name = arg1;
	    info.cmdfile = arg2;
	    status = dagman->AddJob(&info, jobid);
	    if (!status)
	    {
		fprintf(stderr, "ERROR inserting job %s: ", info.name);
		status.PrintError(stderr);
		fprintf(stderr, "\n");
		return 1;
	    }
	    ht.Insert(arg1, jobid);
	    fprintf(stderr, "Created job %s. command file: %s\n",
		    arg1, arg2);

	}
	
	//
	// Handle a dependency command
	//
	else if (!strcasecmp(command, "Dependency"))
	{
	    // Next token is the name of the parent job
	    arg1 = strtok(0, DELIMITERS);
	    if (arg1 == NULL)
	    {
		continue;
	    }
	    
	    // Make sure the job has already been created by looking
	    // up its name in the hash table
	    found = ht.Lookup(arg1, parent);
	    if (!found)
	    {
		fprintf(stderr,
			"Unknown job in dependency on line %d: %s\n",
			lineNumber, arg1);
		return 1;
	    }
	    
	    // Each remaining token is the name of a child job
	    while ((arg2 = strtok(0, DELIMITERS)) != NULL)
	    {
		// Verify the job name with a hash table lookup
		found = ht.Lookup(arg2, child);
		if (!found)
		{
		    fprintf(stderr,
			    "Unknown job in dependency on line %d: %s\n",
			    lineNumber, arg2);
		    return 1;
		}
		// Now add a dependency to the DagMan
		status = dagman->AddDependency(parent, child);
		if (!status)
		{
		    fprintf(stderr, "ERROR adding dependency %s -> %s: ",
			    arg1, arg2);
		    status.PrintError(stderr);
		    fprintf(stderr, "\n");
		    return 1;
		}
		fprintf(stderr, "Created dependency %s -> %s\n",
			arg1, arg2);
	    }
	    
	}
	else
	{
	    //
	    // Ignore bad commands in the input file
	    //
	    fprintf(stderr, "WARNING: invalid command on line %d: %s\n",
		    lineNumber, line);
	    continue;
	}
	
    } // End of while loop for reading all lines in file
    
    return 0;
    
}

