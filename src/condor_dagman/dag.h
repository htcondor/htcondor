/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2001 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
 ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

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

// for the Condor job id of the DAGMan job
extern char* DAGManJobId;

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
        @param maxJobsSubmitted the maximum number of jobs to submit to Condor
               at one time
        @param maxPreScripts the maximum number of PRE scripts to spawn at
		       one time
        @param maxPostScripts the maximum number of POST scripts to spawn at
		       one time
    */
    Dag( const char* condorLog, const int maxJobsSubmitted,
		 const int maxPreScripts, const int maxPostScripts );

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

    /** @return the number of jobs ready to submit to Condor
     */
    inline int NumJobsReady() const { return _readyQ->Number(); }

    /** @return the number of PRE scripts currently running
     */
    inline int NumPreScriptsRunning() const
		{ return _preScriptQ->NumScriptsRunning(); }

    /** @return the number of POST scripts currently running
     */
    inline int NumPostScriptsRunning() const
		{ return _postScriptQ->NumScriptsRunning(); }

    /** @return the total number of PRE/POST scripts currently running
     */
    inline int NumScriptsRunning() const
		{ return NumPreScriptsRunning() + NumPostScriptsRunning(); }

	inline bool Done() const { return NumJobsDone() == NumJobs(); }

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

	void PrintReadyQ( debug_level_t level ) const;

	// max number of PRE & POST scripts to run at once (0 means no limit)
    int _maxPreScripts;
    int _maxPostScripts;

  protected:

    /* Prepares to submit job by running its PRE Script if one exists,
       otherwise adds job to _readyQ and calls SubmitReadyJobs()
	   @return true on success, false on failure
    */
    bool Submit (Job * job);

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
        @return number of jobs successfully submitted
    */
    int SubmitReadyJobs();
  
	void PrintEvent( debug_level_t level, const char* eventName, Job* job );

    // name of consolidated condor log
    char        * _condorLogName;

    // Documentation on ReadUserLog is present in condor_c++_util
    ReadUserLog   _condorLog;

    //
    bool          _condorLogInitialized;

    //  Last known size of condor log, used by DetectLogGrowth()
    off_t         _condorLogSize;

    /// List of Job objects
    List<Job>     _jobs;

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

	// queue of jobs ready to be submitted to Condor
	SimpleList<Job*>* _readyQ;

	// queue of submitted jobs not yet matched with submit events in
	// the Condor job log
    Queue<Job*>* _submitQ;

	ScriptQ* _preScriptQ;
	ScriptQ* _postScriptQ;
};

#endif /* #ifndef DAG_H */
