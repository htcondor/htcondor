#ifndef DAG_H
#define DAG_H

#include "condor_common.h"
#include "list.h"
#include "job.h"
#include "user_log.c++.h"          /* from condor_c++_util/ directory */
#include "condor_constants.h"      /* from condor_includes/ directory */

/** Termination Queue Item (TQI).  EXPLANATION NEEDED HERE!
 */
class TQI {
  public:
    ///
    inline TQI () : parent(NULL) {}
    ///
    inline TQI (Job * p) : parent(p) {}
    ///
    inline TQI (Job * p, const SimpleList<JobID_t> & c) :
        parent(p), children(c) {}

    ///
    void Print () const;

    /** The job that terminated      */   Job                 * parent;
    /** Children net yet seen in log */   SimpleList<JobID_t>   children;
};


//------------------------------------------------------------------------
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
  
    /** Create a DAG
        @param condorLog
        @param dagLockFile
    */
    Dag (const char *condorLog, const char *lockFileName,
         const int  numJobsRunningMax);

    ///
    ~Dag();

    /// Prepare DAG for initial run.  Call this function ONLY ONCE.
    bool Bootstrap (bool recovery);

    /// Add a job to the collection of jobs managed by this Dag.
    inline bool Add (Job & job) { return _jobs.Append(job); }
  
    /** Specify a dependency between two jobs. The child job will only
        run after the parent job has finished.
    */
    bool AddDependency (Job * parent, Job * child);
  
    /** Blocks until the Condor Log file grows.
        @param checkInterval Number of seconds between checks
        @return true: log file grew, false: timeout or shrinkage
    */
    bool DetectLogGrowth (int checkInterval = 2);

    /** Force the Dag to process all new events in the condor log file.
        This may cause the state of some jobs to change.

        @param recover Process Log in Recover Mode, from beginning to end
        @param job The next log event should be for this job
        @return true on success, false on failure
    */
    bool ProcessLogEvents (bool recover = false);

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
    Job * GetJob (const CondorID condorID) const;

    ///
    void PrintJobList() const;

    ///
    void Print_TermQ () const;

    ///
    inline int NumJobs() const { return _jobs.Number(); }

    ///
    inline int NumJobsDone() const { return _numJobsDone; }

    ///
    inline int NumJobsFailed() const { return _numJobsFailed; }

    ///
    inline int NumJobsRunning() const { return _numJobsRunning; }

    /** Remove all jobs (using condor_rm) that are currently running.
        All jobs currently marked Job::STATUS_SUBMITTED will be fed
        as arguments to condor_rm via popen.  This function is called
        when the Dagman Condor job itself is removed by the user via
        condor_rm.  This function <b>is not</b> called when the schedd
        kills Dagman.
    */
    void RemoveRunningJobs () const;

    /** Creates a DAG file based on the DAG in memory, except all
        completed jobs are premarked as DONE.
    */
    void Rescue (const char * rescue_file, const char * datafile) const;

  protected:

    /** Submit job to Condor.  If job is not submittable (!job->CanSubmit())
        then function will return false for failure
        @return true on success, false on failure
    */
    bool Submit (Job * job);

    /** Submit job with ID jobID to Condor
        @return true on success, false on failure
    */
    // bool Submit (JobID_t jobID);

    /// Update the state of a job to "Done", and run child jobs if possible.
    void TerminateJob (Job * job);
  
    /** Get the first appearing job in the termination queue marked SUBMITTED.
        This function is called by ProcessLogEvents when a SUBMIT log
        entry is read.  The challenge is to correctly match the condorID
        found in the SUBMIT log event written by Condor with the Job object
        (currently with condorID (-1,-1,-1) that was recently submitted
        with condor_submit.<p>

        @return address of Job object, or NULL if not found
    */
    Job * GetSubmittedJob (bool recovery);

    /** Submit all ready jobs, provided they are not waiting on a parent job.
        @return true: success, false: fatal error
    */
    bool SubmitReadyJobs ();
  
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

    ///
    List<TQI>    _termQ;

    /// For debugging
    bool         _termQLock;

    /// Number of Jobs that are done (completed execution)
    int _numJobsDone;
    
    /// Number of Jobs that failed (or their PRE or POST script failed).
    int _numJobsFailed;

    /// Number of Jobs currently running (submitted to Condor)
    int _numJobsRunning;

    /** Maximum number of jobs to run at once.  Non-negative.  Zero means
        unlimited
    */
    int _numJobsRunningMax;
};

#endif /* #ifndef DAG_H */
