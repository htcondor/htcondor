/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
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
#include "read_multiple_logs.h"

// for DAGMAN_RUN_POST_ON_FAILURE config setting
extern bool run_post_on_failure;

// the name of the attr we insert in job ads, recording DAGMan's job id
extern const char* DAGManJobIdAttrName;

// for the Condor job id of the DAGMan job
extern char* DAGManJobId;

enum Log_source{
  CONDORLOG,
  DAPLOG
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
		@param condorLogFiles the list of log files for all of the jobs
		       in the DAG
        @param maxJobsSubmitted the maximum number of jobs to submit to Condor
               at one time
        @param maxPreScripts the maximum number of PRE scripts to spawn at
		       one time
        @param maxPostScripts the maximum number of POST scripts to spawn at
		       one time
    */

    Dag( StringList &condorLogFiles, const int maxJobsSubmitted,
		 const int maxPreScripts, const int maxPostScripts, 
		 const char *dapLogName );

    ///
    ~Dag();

    /** Prepare DAG for initial run.  Call this function ONLY ONCE.
        @param recovery specifies if this is a recovery
        @return true: successful, false: failure
    */
    bool Bootstrap (bool recovery);

    /// Add a job to the collection of jobs managed by this Dag.
    bool Add( Job& job );
  
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

    
    //    bool DetectLogGrowth();
    bool DetectCondorLogGrowth();         //<--DAP 
    bool DetectDaPLogGrowth();            //<--DAP

    /** Force the Dag to process all new events in the condor log file.
        This may cause the state of some jobs to change.

        @param recover Process Log in Recover Mode, from beginning to end
        @return true on success, false on failure
    */
    //    bool ProcessLogEvents (bool recover = false);

    bool ProcessLogEvents (int logsource, bool recovery = false); //<--DAP

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

    /** Ask whether a node name exists in the DAG
        @param nodeName the name of the node in the DAG
        @return true if the node exists, false otherwise
    */
    bool NodeExists( const char *nodeName ) const;

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

		/** Submit all ready jobs, provided they are not waiting on a
			parent job or being throttled.
			@return number of jobs successfully submitted
		*/
    int SubmitReadyJobs();
  
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

	int PreScriptReaper( const char* nodeName, int status );
	int PostScriptReaper( const char* nodeName, int status );

	void PrintReadyQ( debug_level_t level ) const;

	bool RemoveNode( const char *name, MyString &whynot );

	bool RemoveDependency( Job *parent, Job *child );
	bool RemoveDependency( Job *parent, Job *child, MyString &whynot );
	
    /* Detects cycle within dag submitted by user
	   @return true if there is cycle
	*/
	bool isCycle ();

	// max number of PRE & POST scripts to run at once (0 means no limit)
    const int _maxPreScripts;
    const int _maxPostScripts;

	void SetDotFileName(char *dot_file_name);
	void SetDotIncludeFileName(char *include_file_name);
	void SetDotFileUpdate(bool update_dot_file)       { _update_dot_file    = update_dot_file; }
	void SetDotFileOverwrite(bool overwrite_dot_file) { _overwrite_dot_file = overwrite_dot_file; }
	bool GetDotFileUpdate(void)                       { return _update_dot_file; }
	void DumpDotFile(void);
	
  protected:

    /* Prepares to submit job by running its PRE Script if one exists,
       otherwise adds job to _readyQ and calls SubmitReadyJobs()
	   @return true on success, false on failure
    */
    bool StartNode( Job *node );

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

	void PrintEvent( debug_level_t level, const char* eventName, Job* job,
					 CondorID condorID );

	void RestartNode( Job *node, bool recovery );

	/* DFS number the jobs in the DAG in order to detect cycle*/
	void DFSVisit (Job * job);
	
    // name of consolidated condor log
	StringList &_condorLogFiles;

    // Documentation on ReadUserLog is present in condor_c++_util
	ReadMultipleUserLogs _condorLog;

    //
    bool          _condorLogInitialized;

    //-->DAP
    const char* _dapLogName;
    ReadUserLog   _dapLog;
    bool          _dapLogInitialized;
    off_t         _dapLogSize;
    //<--DAP

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
    const int _maxJobsSubmitted;

	// queue of jobs ready to be submitted to Condor
	SimpleList<Job*>* _readyQ;

	// queue of submitted jobs not yet matched with submit events in
	// the Condor job log
    Queue<Job*>* _submitQ;

	ScriptQ* _preScriptQ;
	ScriptQ* _postScriptQ;
	
	int DFS_ORDER; 

	// Information for producing dot files, which can be used to visualize
	// DAG files. Dot is part of the graphviz package, which is available from
	// http://www.research.att.com/sw/tools/graphviz/
	char *_dot_file_name;
	char *_dot_include_file_name;
	bool  _update_dot_file;
	bool  _overwrite_dot_file;
	int   _dot_file_name_suffix;

	void IncludeExtraDotCommands(FILE *dot_file);
	void DumpDotFileNodes(FILE *temp_dot_file);
	void DumpDotFileArcs(FILE *temp_dot_file);
	void ChooseDotFileName(MyString &dot_file_name);
};

#endif /* #ifndef DAG_H */
