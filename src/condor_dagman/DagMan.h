#ifndef _DAGMAN_H_
#define _DAGMAN_H_

/* ----------------------------------------------------------------------
   May 14, 1998
   joev@cs
   ------------------------------------------------------------------- */

struct Condor_id;

static const char *COMMENT_PREFIX = "#";
static const char *DELIMITERS     = " \t";

#define MAX_EVENT_STRING 128
#define MAX_LINE_LENGTH  100

#define OK 0
#define NOT_OK -1 

// constants used for DagMan log
#define DAGMAN_SetCondorID   120372
#define DAGMAN_TerminateJob  270797
#define DAGMAN_SubmitJob     260273
#define DAGMAN_UndoJob       260873
#define DAGMAN_SubmitJobOK   170598

#include "list.h"
#include "CondorSubmit.h"
#include "user_log.c++.h"
#include "condor_constants.h"
#include "log.h"

#include <limits.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#include <unistd.h>
#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>

/// The Status of a Job
enum JobStatus {
  /** Job is ready */               READY = 0,
  /** Job has been submitted */     SUBMITTED,
  /** Job is done */                DONE,
  /** Job is not ready */           NOT_READY,
  /** Job is currently running */   RUNNING,
  /** Job had an error */           ERROR
};

///
typedef int NodeID;

///
struct Condor_id {
  int cluster;
  int proc;
  int subproc;
};

/**
   A NodeInfo instance will be used to pass job attributes to the
   AddNode() function.
*/
class NodeInfo {
public:

  ///
  NodeInfo();	
  ///
  NodeInfo(char *NewJobName, 
           char *CommandFileName, 
           char *CondorLogFileName = NULL);
  
  ///
  ~NodeInfo();
  
  ///
  int SetNodeID(NodeID id);
  ///
  int GetNodeID(NodeID &id);

  ///
  int SetCmdFile(char *NewCmdfile);
  ///
  int GetCmdFile(char *&GetCmdfile);

  ///
  int SetJobName(char *NewJobName);
  ///
  int GetJobName(char *&GetJobName);

  ///
  int SetCondorLogFileName(char *NewCondorLogFile);
  ///
  int GetCondorLogFileName(char *&GetCondorLogFile);

  ///
  int SetNodeStatus(JobStatus   Nstatus);
  ///
  int GetNodeStatus(JobStatus & Nstatus);

  ///
  int GetCondorInfo    (int & cluster, int & proc, int & subproc);
  ///
  int UpdateCondorInfo (int   cluster, int   proc, int   subproc);

  /** @name Incoming List */
  //@{
  ///
  int AddIncoming    (NodeID NewJob);
  ///
  int RemoveIncoming (NodeID OldJob);
  //@}

  /** @name Outgoing List */
  //@{
  ///
  int AddOutgoing    (NodeID NewJob);
  ///
  int RemoveOutgoing (NodeID OldJob);
  //@}

  /** @name Waiting List */
  //@{
  ///
  int AddWaiting     (NodeID NewJob);
  ///
  int RemoveWaiting  (NodeID OldJob);
  //@}

  /// check if a list is empty
  bool IsEmpty (char *ListName);
 
  /** @name List of Job Dependencies */
  //@{

  /// incoming -> dependencies coming into the Node
  List<NodeID> incoming;
  /// outgoing -> dependencies going out of the Node
  List<NodeID> outgoing;
  /// waiting -> Nodes on which the current node is waiting for output 
  List<NodeID> waiting;
  //@}

  ///
  void Dump();

private:

  ///
  NodeID NodeName;
  
  /// filename of condor submit file
  char *cmdfile;
  /// name given to the job by the user
  char *JobName;
  /// location of condor log specified by user, if any
  char *condorlog;
  
  ///
  JobStatus NodeStatus;
  

  /** Condor uses three integers to identify jobs. This structure 
      will be used to store those three numbers.  
  */
  Condor_id CondorID;
};

/** A DagMan instance stores information about a job dependency graph,
    and the state of jobs as they are running. Jobs are run by
    submitting them to condor using the API in CondorSubmit.[hc].
    
    The public interface to a DagMan allows you to add jobs, add
    dependencies, submit jobs, control the locations of log files,
    query the state of a job, and process any new events that have
    appeared in the condor log file.
    
    Previously, DagMan depended upon ZOO shipping objects to represent 
    jobs. This has been replaced with the NodeInfo class, which 
    represents dependencies and job state. The NodeList class, which
    is a linked list of NodeInfo, will represent a batch job for Condor.

    @author Joe Varghese
*/
class DagMan {
public:
  
  ///
  DagMan();
  ///
  ~DagMan();
  
  ///
  int Init(const char *condorLog, 
           const char *dagLog,
           const char *dagmanLockFile,
           const char *username);
  
  ///
  int SetCondorLogFile  (char *logfile);
  ///
  int SetDagLogFile     (char *logfile);
  
  /** Add a JobNode to the collection of jobs managed by this DagMan. If
      successful, id will hold an identifier for the new job. This
      identifier can be used later to refer to the new job.
  */
  int AddNode(NodeInfo &NewJob, NodeID &id);
  
  /** Specify a dependency between two jobs. The child job will only
      run after the parent job has finished.
  */
  int AddDependency(NodeID parent, NodeID child);
  
  /// Force a job to be submitted to condor
  int SubmitJob(NodeID id);

  /// Update the state of a job to "Done", and run child jobs if possible.
  int TerminateJob(NodeID NodeID);
 
  /// Update the Wait list of the Nodes after a Node terminates
  int ReportNodeTermination(NodeID parentID);
 
  /** Block until there is new data in the condor log file. Only assume
      that data is available if return value is true. Returning false
      means that no data is available yet.

      This function will be replace by DetectLogGrowth()
  */
  bool WaitForLogEvents();
  
  /** Blocks until the Condor Log file grows.
      This function replaces WaitForLogEvents()

      @param checkInterval Number of seconds between checks
      @return true: log file grew, false: timeout or shrinkage
  */
  bool DetectLogGrowth(int checkInterval = 2);

  /** Force all jobs in the "Ready" state to be submitted to condor
      provided they are not waiting on any input from other jobs
  */
  int RunReadyJobs(int & nJobsSubmitted);
  
  /** Force the DagMan to process all new events in the condor log file.
      This may cause the state of some jobs to change.
  */
  int ProcessLogEvents(bool RecoveryMode = FALSE, 
                       NodeID GetCondorJobID = 0);

  /// Return TRUE if Node is in the list managed by DagMan
  bool NodeLookUp(char *JobName, NodeID & JobID);

  /// Recovery Function
  void Recover_DagMan();

  /// Get the NodeInfo for a particular NodeID
  int GetNodePtr(NodeID id, NodeInfo * & job);

  /// Update internal structure if a node has been predefined as being DONE
  void CheckForFinishedJobs();
  
  /// Print some of the DagMan state to stdout
  void Dump();
  
  /// Get counts of jobs in each possible state
  int NumJobsWaiting();
  ///
  int NumJobsReady();
  ///
  int NumJobsRunning();
  ///
  int NumJobsCompleted();
  ///
  int NumJobs();
  
  ///
  void PrintNodeList();
 
protected:

  /// total number of jobs
  int           _nJobs;

  /// number of jobs waiting to be submitted
  int           _nJobsWaiting;

  /// jobs which have been submitted and are in the condor_q
  int           _nJobsReady;

  /// number of jobs being executed by condor
  int           _nJobsRunning;

  /// number of jobs which have completed
  int           _nJobsDone;
  
  /// name of consolidated condor log
  char        * _condorLogName;

  /// Last known size of condor log, used by DetectLogGrowth()
  off_t         _condorLogSize;

  /// write statistics to this file
  char        * _dagLogName;

  /** used for recovery purposes presence of file indicates
      abnormal termination
  */
  char        * _dagmanLockFile;

  /// Documentation on ReadUserLog is present in condor_c++_util
  ReadUserLog   _condorLog;

  ///
  bool          _condorLogInitialized;

  /// Documentation on UserLog is present in condor_c++_util
  UserLog       _dagLog;

  /// List of NodeInformation
  List<NodeInfo> *CondorJobs;

  ///
  NodeID NodeIDCounter;

};

/** @name Global functions needed for parsing and recovery */
//@{
///
int         Parse               (FILE *fp, DagMan *dagman);
///
NodeInfo  * FindNode            (int NodeID);
///
LogRecord * InstantiateLogEntry (int fd, int type);
//@}

/** Recovery Log Classes for DagMan
    DagMan uses the condor log for recovery
*/
class LogSubmitJob:public LogRecord {

public:

  ///
  LogSubmitJob(int JobID);
  ///
  int Play(void *);
  ///
  int ReadBody(int fd);

private:
  ///
  int WriteBody(int fd);

  ///
  int SubmittedJobID;
};

///
class LogSubmitJobOK:public LogRecord {
public:
  ///
  LogSubmitJobOK(int JobID);
  ///
  int Play(void *);
  ///
  int ReadBody(int fd);

private:
  ///
  int WriteBody(int fd);

  ///
  int SubmittedJobID;
};

///
class LogSetCondorID:public LogRecord {
public:
  ///
  LogSetCondorID(int cluster, int proc, int subproc, int JobID);
  ///
  int Play(void *);
  ///
  int ReadBody(int fd);

private:
  ///
  int WriteBody(int fd);

  ///
  int CondorCluster;
  ///
  int CondorProc;
  ///
  int CondorSubproc;
  ///
  int SubmittedJobID;
};

/** This Log operation records the fact that a given Condor job has
    successfully terminated. We can obtain this information by 
    issuing a condor_history with the Condor ID or by searching
    the condor user file. Needs to be thought out ....
    potential race condition though ....
*/
class LogTerminateJob: public LogRecord {


public:
  ///
  LogTerminateJob(int JobID);
  ///
  int Play(void *);
  ///
  int ReadBody(int fd);

private:
  ///
  int WriteBody(int fd);

  ///
  int SubmittedJobID;

};

/** This Log operation records an UNDO command which will be 
    given through an interface. The interface has not been 
    implemented though.
*/
class LogUndoJob: public LogRecord {

public:
  ///
  LogUndoJob(int JobID);
  ///
  int Play(void *);
  ///
  int ReadBody(int fd);

private:
  ///
  int WriteBody(int fd);

  ///
  int SubmittedJobID;
};

#endif /* #ifndef _DAGMAN_H_ */
