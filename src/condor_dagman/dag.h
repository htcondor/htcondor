#ifndef DAG_H
#define DAG_H

#include "condor_common.h"

#include "job.h"
#include "user_log.c++.h"          /* from condor_c++_util/ directory */
#include "condor_constants.h"      /* from condor_includes/ directory */
#include "list.h"

// #include <limits.h>
// #include <string.h>
// #include <ctype.h>
// #include <fcntl.h>
// #include <errno.h>
// #include <signal.h>

// #include <unistd.h>
// #include <sys/types.h>

// #include <stdio.h>
// #include <stdlib.h>

/** A Dag instance stores information about a job dependency graph,
    and the state of jobs as they are running. Jobs are run by
    submitting them to condor using the API in CondorSubmit.[hc].
    
    The public interface to a Dag allows you to add jobs, add
    dependencies, submit jobs, control the locations of log files,
    query the state of a job, and process any new events that have
    appeared in the condor log file.
*/
class Dag {
public:
  
  ///
  Dag();
  ///
  ~Dag();
  
  ///
  bool Init (const char *condorLog, const char *DagLockFile);
  
  /// Add a job to the collection of jobs managed by this Dag.
  inline bool Add (Job & job) { return _jobs.Append(job); }
  
  /** Specify a dependency between two jobs. The child job will only
      run after the parent job has finished.
  */
  bool AddDependency (Job * parent, Job * child);
  
  /** Submit job to Condor
      @return true on success, false on failure
  */
  bool Submit (Job * job);

  /** Submit job with ID jobID to Condor
      @return true on success, false on failure
  */
  bool Submit (JobID_t jobID);

  /** Blocks until the Condor Log file grows.
      @param checkInterval Number of seconds between checks
      @return true: log file grew, false: timeout or shrinkage
  */
  bool DetectLogGrowth (int checkInterval = 2);

  /** Force all jobs in the "Ready" state to be submitted to condor
      provided they are not waiting on any input from other jobs
      @return number of jobs submitted by this calling of the function
  */
  int SubmitReadyJobs ();
  
  /** Force the Dag to process all new events in the condor log file.
      This may cause the state of some jobs to change.

      @param recover Process Log in Recover Mode, from beginning to end
      @param job The next log event should be for this job
      @return true on success, false on failure
  */
  bool ProcessLogEvents (bool recover = false, Job * getJob = NULL);

  /** Get pointer to job with id jobID
      @return address of Job object, or NULL if not found
  */
  Job * GetJob (const JobID_t jobID) const;

  /** Get pointer to job with name jobName
      @return address of Job object, or NULL if not found
  */
  Job * GetJob (const char * jobName) const;

  /** Get pointer to job with condor ID condorID
      @return address of Job object, or NULL if not found
  */
  Job * GetJob (const CondorID_t condorID) const;

  /// Recovery Function
  inline bool Recover() { return ProcessLogEvents(true); }

  /// Update internal structure if a node has been predefined as being DONE
  void TerminateFinishedJobs();
  
  ///
  void PrintJobList() const;

  ///
  inline int NumJobs() const { return _jobs.Number(); }

  ///
  inline int NumJobsDone() const { return _numJobsDone; }

protected:

  /// Update the state of a job to "Done", and run child jobs if possible.
  void TerminateJob (Job * job);
 
  /** Get pointer to next ready job.  A ready job is one which has an
      empty WAITING queue, indicating all incoming jobs have completed.
      @return address of Job object, or NULL if not found
  */
  Job * GetReadyJob () const;

  /// name of consolidated condor log
  char        * _condorLogName;

  /// Documentation on ReadUserLog is present in condor_c++_util
  ReadUserLog   _condorLog;

  ///
  bool          _condorLogInitialized;

  /// Last known size of condor log, used by DetectLogGrowth()
  off_t         _condorLogSize;

  /** used for recovery purposes presence of file indicates
      abnormal termination
  */
  char        * _lockFileName;

  /// List of Job objects
  List<Job>     _jobs;

  /// Number of Jobs that are done (completed execution)
  int _numJobsDone;
};

#endif /* #ifndef DAG_H */
