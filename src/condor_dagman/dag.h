#ifndef DAG_H
#define DAG_H

#include "condor_common.h"
#include "list.h"
#include "job.h"
#include "scriptQ.h"
#include "user_log.c++.h"          /* from condor_c++_util/ directory */
#include "condor_constants.h"      /* from condor_includes/ directory */
#include "HashTable.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

// for DAGMAN_RUN_POST_ON_FAILURE config setting
extern bool run_post_on_failure;

// Termination Queue Item (TQI).
class TQI {
  public:
    inline TQI () : parent(NULL) {}
    inline TQI (Job * p) : parent(p) {}
    inline TQI (Job * p, const SimpleList<JobID_t> & c) :
        parent(p), children(c) {}

    void Print () const;

    Job                 * parent;   // The job that terminated
    SimpleList<JobID_t>   children; // Children net yet seen in log
};


//------------------------------------------------------------------------
/** A Dag instance stores information about a job dependency graph,
    and the state of jobs as they are running. Jobs are run by
    submitting them to condor using the UserLog API<p>
    
    The public interface to a Dag allows you to add jobs, add
    dependencies, submit jobs, control the locations of log files,
    query the state of a job, and process any new events that have
    appeared in the condor log file.
*/
class Dag {
  public:
  
    /** Create a DAG
        @param condorLog the condor log where job events are being written to
        @param lockFileName the name of the lock file
        @param maxJobsSubmitted the maximum number of jobs to submit to Condor
               at one time
        @param maxScriptsRunning the maximum number of scripts to spawn at one
               time
    */
    Dag (const char *condorLog, const char *lockFileName,
         const int maxJobsSubmitted, const int maxScriptsRunning );

    ///
    ~Dag();

    /** Prepare DAG for initial run.  Call this function ONLY ONCE.
        @param recovery specifies if this is a recovery
        @return true: successful, false: failure
    */
    bool Bootstrap (bool recovery);

    /// Add a job to the collection of jobs managed by this Dag.
    inline bool Add (Job & job) { return _jobs.Append(job); }
  
    /** Specify a dependency between two jobs. The child job will only
        run after the parent job has finished.
        @param parent The parent job
        @param child The child job (depends on the parent)
        @return true: successful, false: failure
    */
    bool AddDependency (Job * parent, Job * child);
  
    /** Blocks until the Condor Log file grows.
        @return true: log file grew, false: timeout or shrinkage
    */
    bool DetectLogGrowth();

    /** Force the Dag to process all new events in the condor log file.
        This may cause the state of some jobs to change.

        @param recover Process Log in Recover Mode, from beginning to end
        @return true on success, false on failure
    */
    bool ProcessLogEvents (bool recover = false);

    /** Get pointer to job with id jobID
        @param the handle of the job in the DAG
        @return address of Job object, or NULL if not found
    */
    Job * GetJob (const JobID_t jobID) const;

    /** Get pointer to job with name jobName
        @param jobName the name of the job in the DAG
        @return address of Job object, or NULL if not found
    */
    Job * GetJob (const char * jobName) const;

    /** Get pointer to job with condor ID condorID
        @param condorID the CondorID of the job in the DAG
        @return address of Job object, or NULL if not found
    */
    Job * GetJob (const CondorID condorID) const;

    /// Print the list of jobs to stdout (for debugging).
    void PrintJobList() const;
    void PrintJobList( Job::status_t status ) const;

    //
    void Print_TermQ () const;

    /** @return the total number of jobs in the DAG
     */
    inline int NumJobs() const { return _jobs.Number(); }

    /** @return the number of jobs completed
     */
    inline int NumJobsDone() const { return _numJobsDone; }

    /** @return the number of jobs that failed in the DAG
     */
    inline int NumJobsFailed() const { return _numJobsFailed; }

    /** @return the number of jobs currently submitted to Condor
     */
    inline int NumJobsSubmitted() const { return _numJobsSubmitted; }

    /** @return the number of PRE/POST scripts currently running
     */
    inline int NumScriptsRunning() const
		{ return _scriptQ->NumScriptsRunning(); }

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
        @param rescue_file The name of the rescue file to generate
        @param datafile The original DAG config file to read from
    */
    void Rescue (const char * rescue_file, const char * datafile) const;

	int PreScriptReaper( Job* job, int status );
	int PostScriptReaper( Job* job, int status );

	// max number of PRE & POST scripts to run at once (0 means no limit)
    int _maxScriptsRunning;

  protected:

    /* Prepares to submit job by running its PRE Script if one exists,
       otherwise calls SubmitCondor() directly
	   @return true on success, false on failure
    */
    bool Submit (Job * job);

    /*  Submit job with ID jobID to Condor
        @return true on success, false on failure
    */
    // bool Submit (JobID_t jobID);

    /*  Submit job to Condor.  If job is not submittable (!job->CanSubmit())
        then function will return false for failure
        @return true on success, false on failure
    */
	bool SubmitCondor( Job* job );

    // add job to termination queue and report termination to all
    // child jobs by removing job ID from each child's waiting queue
    void TerminateJob( Job* job, bool bootstrap = false );
  
    /*  Get the first appearing job in the termination queue marked SUBMITTED.
        This function is called by ProcessLogEvents when a SUBMIT log
        entry is read.  The challenge is to correctly match the condorID
        found in the SUBMIT log event written by Condor with the Job object
        (currently with condorID (-1,-1,-1) that was recently submitted
        with condor_submit.<p>

        @return address of Job object, or NULL if not found
    */
    Job * GetSubmittedJob (bool recovery);

    /*  Submit all ready jobs, provided they are not waiting on a parent job.
        @return true: success, false: fatal error
    */
    bool SubmitReadyJobs ();
  
    // name of consolidated condor log
    char        * _condorLogName;

    // Documentation on ReadUserLog is present in condor_c++_util
    ReadUserLog   _condorLog;

    //
    bool          _condorLogInitialized;

    //  Last known size of condor log, used by DetectLogGrowth()
    off_t         _condorLogSize;

    /*  used for recovery purposes presence of file indicates
        abnormal termination
    */
    char        * _lockFileName;

    /// List of Job objects
    List<Job>     _jobs;

    //
    List<TQI>    _termQ;

    // Number of Jobs that are done (completed execution)
    int _numJobsDone;
    
    // Number of Jobs that failed (or their PRE or POST script failed).
    int _numJobsFailed;

    // Number of Jobs currently running (submitted to Condor)
    int _numJobsSubmitted;

    /*  Maximum number of jobs to submit at once.  Non-negative.  Zero means
        unlimited
    */
    int _maxJobsSubmitted;

	// list of submitted jobs not yet matched with submit events in
	// the Condor job log
    Queue<Job*>* _submitQ;

	ScriptQ* _scriptQ;
};

#endif /* #ifndef DAG_H */
