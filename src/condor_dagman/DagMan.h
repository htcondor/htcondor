
#ifndef _DAGMAN_H_
#define _DAGMAN_H_

/* ----------------------------------------------------------------------
   
    DagMan.h - declaration of the DagMan class.

    A DagMan instance stores information about a job dependency graph,
    and the state of jobs as they are running. Jobs are run by
    submitting them to condor using the API in CondorSubmit.[hc].

    The public interface to a DagMan allows you to add jobs, add
    dependencies, submit jobs, control the locations of log files,
    query the state of a job, and process any new events that have
    appeared in the condor log file.

    Currently the dependency graph and all job state is stored in a
    data structure called a shipping object list. This structure is a
    dynamic array of shipping objects, and it allows named
    relationships to be established between arbitrary list elements.
    Each shipping object has a type, a value, and can be related to
    other objects in the same list using named relationships.

    For example, a job is represented by a single shipping object, and
    this object has a set of other job objects to which it is related
    via relationships named "Children" and "Parents". These
    relationships allow the dependency information to be stored inside
    the shipping object list. Job attributes are represented by
    shipping objects also. Each job is related to another shipping
    object by the relationship "Name". This related object has type
    "string" and its value is the name of the job that appeared in the
    DagMan input file.

    NOTE: The shipping object list was chosen for storing the DAG
    because code exists that makes it easy for separate processes to
    exchange shipping lists over a TCP socket. The structure is slow
    and bulky, but that may not be a bottleneck if the jobs that are
    being submitted must run for a long time.

    -----------------------------------------------------------------------
    */

#include "shipobject.h"
#include "ShipFns.h"
#include "util.h"
#include "user_log.c++.h"
#include "CondorSubmit.h"
#include "HashTable.h"
#include <string.h>

typedef int JobID;
static const int   MAX_EVENT_STRING = 128;
static const char *DEFAULT_CONDOR_SERVER = "andvari";
static const int   DEFAULT_CONDOR_PORT = 9100;

//
// Condor uses three integers to identify jobs. This structure will
// be used to store those three numbers.
//
struct CondorID
{
    int cluster;
    int proc;
    int subproc;
};

//
// A JobInfo instance will be used to pass job attributes to the
// AddJob() function.
//
class JobInfo
{
  public:
    char *name;
    char *cmdfile;
};

//
// The DagStatus class stores information about errors that can occur
// with the DagMan member functions.
//
class DagStatus
{
  public:

    enum Code
    {
	Ok = 0,
	JobNotFound,
	BadJobState,
	AttrNotFound,
	LogInitError,
	LogReadError,
	LogWriteError,
	SubmitFailed,
	OutOfMemory
    };

    DagStatus(DagStatus::Code c);
    DagStatus();
    bool IsOK();
    const char *GetString();
    void PrintError(FILE *fp);
    
    Code code;
    
    // For some errors, the hard-wired descriptive string is not
    // sufficient. Use this field to add more detailed information
    // about the error. Example: the AttrNotFound error can be
    // augmented with the name of the unknown attribute.
    char *extra;
    
};

bool operator ! (const DagStatus &s);

//
// The DagMan class
//
class DagMan
{
  public:

    DagMan();
    ~DagMan();
    
    // Specify locations of log files. condorLog is where condor events
    // will appear as jobs are running. dagLog is where DagMan events are
    // written as jobs begin and end. username must have permission to
    // read the condor log and write the daglog.
    DagStatus Init(const char *condorLog, const char *dagLog,
		   const char *recoveryLog,const char *username);
    DagStatus SetCondorLogFile(char *logfile);
    DagStatus SetDagLogFile(char *logfile);

    // Add a job to the collection of jobs managed by this DagMan. If
    // successful, id will hold an identifier for the new job. This
    // identifier can be used later to refer to the new job.
    DagStatus AddJob(JobInfo *j, JobID &id);
    
    // Specify a dependency between two jobs. The parent job will only
    // run after the child job has finished.
    DagStatus AddDependency(JobID parent, JobID child);

    // Get attributes of a single job. If successful, the second argument
    // is updated with the attribute value.
    DagStatus GetJobState(JobID id, char *&state);
    DagStatus GetJobName(JobID id, char *&name);
    
    // Force a job to be submitted to condor
    DagStatus SubmitJob(JobID id);

    //SubmitJob event at the time of Recovery.
    DagStatus RecoverySubmitJob(JobID jobID, CondorID condorID);

    // Block until there is new data in the condor log file. Only assume
    // that data is available if return value is true. Returning false
    // means that no data is available yet.
    bool WaitForLogEvents();
    
    // Force all jobs in the "Ready" state to be submitted to condor
    DagStatus RunReadyJobs(int &nJobsSubmitted);
    
    // Force the DagMan to process all new events in the condor log file.
    // This may cause the state of some jobs to change.
    DagStatus ProcessLogEvents();

    // Returns a pointer to the shipping object list storing the
    // dependency graph and all job attributes.
    Ship_ObjList *GetShipObjList();
    
    // Print some of the DagMan state to stdout
    void Dump();
    
    // Get counts of jobs in each possible state
    int NumJobsNotReady();
    int NumJobsReady();
    int NumJobsRunning();
    int NumJobsCompleted();
    int NumJobs();
    int RunRecovery();
    
  protected:
    
    int           _nJobs;    
    int           _nJobsNotReady;
    int           _nJobsReady;
    int           _nJobsRunning;
    int           _nJobsDone;
    Ship_ObjList  _list;
    
    char         *_condorLogName;
    char         *_dagLogName;
    char         *_recoveryLogName;
    UserLog       _dagLog;
    ReadUserLog   _condorLog;
    bool          _condorLogInitialized;
    
    //
    // Need a hash table for mapping condor job numbers to Job id numbers
    //
    HashTable<CondorID, JobID> *_jobHT;
    
    // Update the state of a job to "Done", and run parent jobs if possible.
    DagStatus TerminateJob(JobID id);

    DagStatus RecoveryTerminateJob(JobID id);
    
    // Helper function used to see if a parent job is ready to run. If 
    // the parent is ready, its state will be changed to "Ready"
    DagStatus ReportChildTermination(JobID parent, JobID child);
    
    // Mapping from JobID to shipping object pointer
    DagStatus GetJobPtr(JobID id, Ship_ComplexObj *&job);
    
    // Update the state of a job
    DagStatus SetJobState(JobID id, const char *state);
    
    // Set the name of a job
    DagStatus SetJobName(JobID id, const char *name);
    
    // Write a string to the DagMan log file
    DagStatus WriteLogEvent(const char *info);
    
    // Get/Set a job attribute
    DagStatus GetJobAttr(JobID id, const char *attrName, char *&attrValue);
    DagStatus SetJobAttr(JobID id, const char *attrName,
			 const char *attrValue);

    //
    // Functions for retrieving lists of children and parents. Maybe
    // we should replace them by functions that do not depend on 
    // shipping?
    //
    
    // Get a list of all parent/child jobs.
    // See shipobj_private.h for the List class interface.
    // NOTE: if no parents/children exist, the return value will
    // be OK and list will be NULL
    DagStatus GetChildList(JobID id, List<OBJECTINDEX> *&list);
    DagStatus GetParentList(JobID id, List<OBJECTINDEX> *&list);

    // Get pointers to the parent/child JobSet objects
    // NOTE: if no parents/children exist, the return value will
    // be OK and pSet will be NULL
    DagStatus GetChildSet(JobID id, Ship_ComplexObj *&pSet);
    DagStatus GetParentSet(JobID id, Ship_ComplexObj *&pSet);
    
};

#endif



