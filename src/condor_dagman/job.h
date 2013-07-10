/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


#ifndef JOB_H
#define JOB_H

#include "condor_common.h"      /* for <stdio.h> */
#include "condor_constants.h"   /* from condor_includes/ directory */
#include "simplelist.h"         /* from condor_utils/ directory */
#include "MyString.h"
#include "list.h"
#include "condor_id.h"
#include "throttle_by_category.h"
#include "read_multiple_logs.h"
#include "CondorError.h"
#include <set>

class ThrottleByCategory;
class Dag;
class DagmanMetrics;

//
// Local DAGMan includes
//
#include "debug.h"
#include "script.h"

#define JOB_ERROR_TEXT_MAXLEN 128

typedef int JobID_t;

/**  The job class represents a job in the DAG and its state in the Condor
     system.  A job is given a name, a CondorID, and three queues.  The
     parents queue is a list of parent jobs that this one depends on.  That
     queue never changes once set.  The waiting queue is the same as the
     parents queue, but shrinks to emptiness has the parent jobs complete.
     The children queue is a list of child jobs that depend on this one.
     Once set, this queue never changes.<p>

     From DAGMan's point of view, a job has six basic states in the
     Condor system (refer to the Job::status_t enumeration).  It
     starts out as READY, meaning that it has not yet been submitted.
     If the job has a PRE script defined it changes to PRERUN while
     that script executes.  The job's state changes to SUBMITTED once
     in the Condor system.  If it completes successfully and the job
     has a POST script defined it changes to POSTRUN while that script
     executes, otherwise if it completes successfully it is DONE, or
     is in the ERROR state if it completes abnormally.  Note that final
     nodes are a special case -- they are created with a state of
     NOT_READY<p>

     The DAG class will control the job by modifying and viewing its
     three queues.  Once the WAITING queue becomes empty, the job
     state either changes to PRERUN while the job's PRE script runs,
     or the job is submitted and its state changes immediately to
     SUBMITTED.  If the job completes successfully, its state is
     changed to DONE, and the children (listed in this jobs CHILDREN
     queue) are put on the DAG's ready list.  */
class Job {
  public:

        // possible kinds of job (e.g., Condor, Stork, etc.)
        // NOTE: must be kept in sync with _job_type_strings[]
        // NOTE: must be kept in sync with enum Log_source
	typedef enum {
		TYPE_CONDOR,
		TYPE_STORK,
	 } job_type_t;
  
    /** Enumeration for specifying which queue for Add() and Remove().
        If you change this enum, you *must* also update queue_t_names
    */
    enum queue_t {
        /** Parents of this job.  The list of parent jobs, which does not
            change while DAGMan is running.
        */
        Q_PARENTS,

        /** Parents of this job not finished.  The list of parents that
            have not yet finished.  Once this queue is empty, this job may
            run.
        */
        Q_WAITING,

        /** Children of this job.  The list of child jobs, which does not
            change while DAGMan is running.
        */
        Q_CHILDREN
    };

    /** The string names for the queue_t enumeration.  Use this for printing
        the names "Q_PARENTS", "Q_WAITING", or "Q_CHILDREN".  For example,
        to print out whether the children queue is empty:<p>
        <pre>
          Job * job;  // Assume this is assigned to a job
          printf ("The %s queue of job %s is %s empty\n",
                  Job::queue_t_names[Q_CHILDREN], job->GetJobName(),
                  job->IsEmpty(Q_CHILDREN) ? "", "not");
        </pre>
    */
    static const char *queue_t_names[];
  
	/** Returns how many direct parents a node has.
		@return number of parents
	*/
	int NumParents() const;


	/** Returns how many direct children a node has.
		@return number of children
	*/
	int NumChildren() const;

    /** The Status of a Job
        If you update this enum, you *must* also update status_t_names
		and the IsActive() method, etc.
    */
	// WARNING!  status_t and status_t_names must be kept in sync!!
    enum status_t {
		/** Job is not ready (for final) */ STATUS_NOT_READY,
        /** Job is ready for submission */ STATUS_READY,
        /** Job waiting for PRE script */  STATUS_PRERUN,
        /** Job has been submitted */      STATUS_SUBMITTED,
        /** Job waiting for POST script */ STATUS_POSTRUN,
        /** Job is done */                 STATUS_DONE,
        /** Job exited abnormally */       STATUS_ERROR,
    };

    /** The string names for the status_t enumeration.  Use this the same
        way you would use the queue_t_names array.
    */
	// WARNING!  status_t and status_t_names must be kept in sync!!
    static const char * status_t_names[];

	// explanation text for errors
	char error_text[JOB_ERROR_TEXT_MAXLEN];

	static int NOOP_NODE_PROCID;
  
    /** Constructor
        @param jobType Type of job in dag file.
        @param jobName Name of job in dag file.  String is deep copied.
		@param directory Directory to run the node in, "" if current
		       directory.  String is deep copied.
        @param cmdFile Path to condor cmd file.  String is deep copied.
    */
    Job( const job_type_t jobType, const char* jobName,
				const char* directory, const char* cmdFile ); 
  
    ~Job();

	void PrefixName(const MyString &prefix);
	inline const char* GetJobName() const { return _jobName; }
	inline const char* GetDirectory() const { return _directory; }
	inline const char* GetCmdFile() const { return _cmdFile; }
	inline JobID_t GetJobID() const { return _jobID; }
	inline int GetRetryMax() const { return retry_max; }
	inline int GetRetries() const { return retries; }
	const char* GetPreScriptName() const;
	const char* GetPostScriptName() const;
	const char* JobTypeString() const;
	job_type_t JobType() const;

	bool AddPreScript( const char *cmd, MyString &whynot );
	bool AddPreSkip( int exitCode, MyString &whynot );
	bool AddPostScript( const char *cmd, MyString &whynot );
	bool AddScript( bool post, const char *cmd, MyString &whynot );

	void SetFinal(bool value) { _final = value; }
	bool GetFinal() const { return _final; }
	void SetNoop( bool value ) { _noop = value; }
	bool GetNoop( void ) const { return _noop; }

	Script * _scriptPre;
	Script * _scriptPost;

    ///
    inline std::set<JobID_t> & GetQueueRef (const queue_t queue) {
        return _queues[queue];
    }

    /** Add a job to one of the queues.  Adds the job with ID jobID to
        the parents, children, or waiting queue of this job.
        @param jobID ID of the job to be added.  Should not be this job's ID
        @param queue The queue to add the job to
        @return true: success, false: failure (lack of memory)
    */
    bool Add( const queue_t queue, const JobID_t jobID );

    /** Returns true if this job is ready for submission.
        @return true if job is submittable, false if not
    */
    inline bool CanSubmit () const {
        return (IsEmpty(Job::Q_WAITING) && _Status == STATUS_READY);
    }

    /** Remove a job from one of the queues.  Removes the job with ID
        jobID from the parents, children, or waiting queue of this job.
        @param jobID ID of the job to be removed.  Should not be this job's ID
        @param queue The queue to add the job to
        @return true: success, false: failure (jobID not found in queue)
    */
    bool Remove (const queue_t queue, const JobID_t jobID);

	/** Check whether the submit file for this job has a log file
	    defined.
		@param usingDefault is true if DAGman is watching the
			default node log
		@return true iff the submit file defines a log file
	*/
	bool CheckForLogFile(bool usingDefault) const;

    /** Returns true if a queue is empty (has no jobs)
        @param queue Selects which queue to look at
        @return true: queue is empty, false: otherwise
    */
    inline bool IsEmpty (const queue_t queue) const {
        return _queues[queue].empty();
    }

    /** Returns the node's current status
        @return the node's current status
    */
	status_t GetStatus() const;

    /** Returns the node's current status string
        @return address of a string describing the node's current status
    */
	const char* GetStatusName() const;

    /** Sets the node's current status
        @return true: status change was successful, false: otherwise
    */
	bool SetStatus( status_t newStatus );

		/** Get whether this job is idle.
		    @return true if job is idle, false otherwise.
		*/
	bool GetIsIdle() const { return _isIdle; }

		/** Set whether this job is idle.
		    @param true if job is idle, false otherwise.
		*/
	void SetIsIdle(bool isIdle) { _isIdle = isIdle; }

		/** Is the specified node a child of this node?
			@param child Pointer to the node to check for childhood.
			@return true: specified node is our child, false: otherwise
		*/
	bool HasChild( Job* child );
		/** Is the specified node a parent of this node?
			@param child Pointer to the node to check for parenthood.
			@return true: specified node is our parent, false: otherwise
		*/
	bool HasParent( Job* parent );

	bool RemoveChild( Job* child );
	bool RemoveChild( Job* child, MyString &whynot );
	bool RemoveParent( Job* parent, MyString &whynot );
	bool RemoveDependency( queue_t queue, JobID_t job );
	bool RemoveDependency( queue_t queue, JobID_t job, MyString &whynot );
 
    /** Dump the contents of this Job to stdout for debugging purposes.
		@param the current DAG (used to translate node ID to node object)
	*/
    void Dump ( const Dag *dag ) const;
  
    /** Print the identification info for this Job to stdout.
        @param condorID If true, also print the job's CondorID
     */
    void Print (bool condorID = false) const;
  
		// double-check internal data structures for consistency
	bool SanityCheck() const;

	bool AddParent( Job* parent );
	bool AddParent( Job* parent, MyString &whynot );
	bool CanAddParent( Job* parent, MyString &whynot );

	bool AddChild( Job* child );
	bool AddChild( Job* child, MyString &whynot );
	bool CanAddChild( Job* child, MyString &whynot );

		// should be called when the job terminates
	bool TerminateSuccess();
	bool TerminateFailure();

    /** Returns true if the node's pre script, batch job, or post
        script are currently submitted or running.
        @return true: node is active, false: otherwise
    */
	bool IsActive() const;

	/** Sets the node's category (used for throttling by category).
		@param categoryName: the name of the node's category
		@param catThrottles: the category throttles object
		*/
	void SetCategory( const char *categoryName,
				ThrottleByCategory &catThrottles );

	/** Get the category throttle information for this node.
		@return pointer to throttle info (may be NULL)
	*/
	ThrottleByCategory::ThrottleInfo *GetThrottleInfo() {
			return _throttleInfo; }
	
	/** Interpolate any vars values with $(JOB) with the name of the job
		@return void
	*/
	void ResolveVarsInterpolations(void);

	/** Add a prefix to the Directory setting for this job. If the prefix
		is ".", then do nothing.
		@param prefix: the prefix to be joined to the directory using "/"
		@return void
	*/
	void PrefixDirectory(MyString &prefix);

	/** Set the DAG file (if any) for this node.  (This is set for nested
			DAGs defined with the "SUBDAG" keyword.)
		@param dagFile: the name of the DAG file
	*/
	void SetDagFile( const char *dagFile );

	/** Get the DAG file name (if any) for this node.  (This is set for nested
			DAGs defined with the "SUBDAG" keyword.)
		@return the DAG file name, or NULL if none
	*/
	const char *GetDagFile() const {
		return _dagFile;
	}

	/** Monitor this node's Condor or Stork log file with the
		multiple log reader.  (Must be called before this node's
		job is submitted.)
		@param logReader: the multiple log reader
		@param recovery: whether we're in recovery mode
		@param defaultNodeLog: the default log file to be used if the
			node's submit file doesn't define a log file
		@return true if successful, false if failed
	*/
	bool MonitorLogFile( ReadMultipleUserLogs &condorLogReader,
				ReadMultipleUserLogs &storkLogReader, bool nfsIsError,
				bool recovery, const char *defaultNodeLog, bool usingDefault );

	/** Unmonitor this node's Condor or Stork log file with the
		multiple log reader.  (Must be called after everything is done
		for this node.)
		@param logReader: the multiple log reader
		@return true if successful, false if failed
	*/
	bool UnmonitorLogFile( ReadMultipleUserLogs &logReader,
				ReadMultipleUserLogs &storkLogReader );

		// Whether this node is using the default node log file.
	bool UsingDefaultLog() const { return _useDefaultLog; }

	/** Get the log file for this node.
		@return the name of this node's log file.
	*/
	const char *GetLogFile() const { return _logFile; }

	/** Get whether this node's log file is XML (versus "standard"
		format).
		@return true iff the log file is XML.
	*/
	bool GetLogFileIsXml() const { return _logFileIsXml; }

	/** Get the jobstate.log job tag for this node.
		@return The job tag (can be "local"; if no tag is specified,
			the value will be "-").
	*/
	const char *GetJobstateJobTag();

	/** Get the jobstate.log sequence number for this node, assigning one
		if we haven't already.
	*/
	int GetJobstateSequenceNum();
	
	/** Reset the jobstate.log sequence number for this node, so we get a
		new sequence number for node retries, etc.
	*/
	void ResetJobstateSequenceNum() { _jobstateSeqNum = 0; }

	/** Set the master jobstate.log sequence number.
		@param The next sequence number that should be given out.
	*/
	static void SetJobstateNextSequenceNum( int nextSeqNum ) {
		_nextJobstateSeqNum = nextSeqNum;
	}

	/** Set the last event time for this job to be the time of the given
		event (this is used as the time for jobstate.log pseudo-events like
		JOB_SUCCESS).
		@param The event whose time should be saved.
	*/
	void SetLastEventTime( const ULogEvent *event );

	/** Get the time at which the most recent event occurred for this job.
		@return the last event time.
	*/
	time_t GetLastEventTime() { return _lastEventTime; }
	void FixPriority(Dag& dag);

	bool HasPreSkip() const { return _preskip != PRE_SKIP_INVALID; }
	int GetPreSkip() const;
	
	int GetCluster() const { return _CondorID._cluster; }
	int GetProc() const { return _CondorID._proc; }
	int GetSubProc() const { return _CondorID._subproc; }
	bool SetCondorID(const CondorID& cid);
	const CondorID& GetID() const { return _CondorID; }

		/** Update the DAGMan metrics for an execute event.
			@param proc The proc ID of this event.
			@param eventTime The time at which this event occurred.
			@param metrics The DagmanMetrics object to update.
		*/
	void ExecMetrics( int proc, const struct tm &eventTime,
				DagmanMetrics *metrics );

		/** Update the DAGMan metrics for a terminated or aborted event.
			@param proc The proc ID of this event.
			@param eventTime The time at which this event occurred.
			@param metrics The DagmanMetrics object to update.
		*/
	void TermAbortMetrics( int proc, const struct tm &eventTime,
				DagmanMetrics *metrics );

private:
    /** */ CondorID _CondorID;
public:

    // maximum number of times to retry this node
    int retry_max;
    // number of retries so far
    int retries;

	// Number of submit tries so far.
	int _submitTries;

    // the return code of the job
    int retval;
	
    // special return code indicating that a node shouldn't be retried
    int retry_abort_val; // UNLESS-EXIT
    // indicates whether retry_abort_val has been set
    bool have_retry_abort_val;

		// Special return code indicating that the entire DAG should be
		// aborted.
	int abort_dag_val;

		// Indicates whether abort_dag_val was set.
	bool have_abort_dag_val;

		// The exit code that this DAG will return on abort.
	int abort_dag_return_val;

		// Indicates whether abort_dag_return_val was set.
	bool have_abort_dag_return_val;

	// somewhat kludgey, but this indicates to Dag::TerminateJob()
	// whether Dag::_numJobsDone has been incremented for this node
	// yet or not (since that method can now be called more than once
	// for a given node); it should not be examined or changed
	// unless/until node is STATUS_DONE
	bool countedAsDone;

    //Node has been visited by DFS order
	bool _visited; 
	
	//DFS ordering of the node
	int _dfsOrder; 

	struct NodeVar {
		MyString _name;
		MyString _value;
	};

	List<NodeVar> *varsFromDag;

		// Count of the number of job procs currently in the batch system
		// queue for this node.
	int _queuedNodeJobProcs;

		// Whether the _nodePriority value is meaningful.
	bool _hasNodePriority;

		// Node priority.  Higher number is better priority (submit first).
	int _nodePriority;

		// The number of times this job has been held.  (Note: the current
		// implementation counts holds for all procs in a multi-proc cluster
		// together -- that should get changed eventually.)
	int _timesHeld;

		// The number of jobs procs of this node that are currently held.
		// (Note: we may need to track the hold state of each proc in a
		// cluster separately to correctly deal with multi-proc clusters.)
	int _jobProcsOnHold;
	bool UseDefaultLog() const { return append_default_log; }

		/** Mark a job with ProcId == proc as being on hold
 			Returns false if the job is already on hold
		*/
 
	bool Hold(int proc);
	
		/** Mark a job with ProcId == proc as being released
 		    Returns false if the job is not on hold
		*/
	bool Release(int proc);

private:
		/** Clean up memory that's no longer needed once a node has
			finished.  (Note that this doesn't mean that the Job object
			is not valid -- it just cleans up some temporary memory.)
			Also check that we got a consistent set of events for the
			metrics.
		*/
	void Cleanup();

		/** _onHold[proc] is nonzero if the condor job 
 			with ProcId == proc is on hold, and zero
			otherwise
		*/
	std::vector<unsigned char> _onHold;	

		// Mark this node as failed because of an error in monitoring
		// the log file.
  	void LogMonitorFailed();

        // strings for job_type_t (e.g., "Condor, "Stork", etc.)
    static const char* _job_type_names[];

		// type of job (e.g., Condor, Stork, etc.)
	job_type_t _jobType;

		// Directory to cd to before running the job or the PRE and POST
		// scripts.
	char * _directory;

    // filename of condor submit file
    char * _cmdFile;

	// Filename of DAG file (only for nested DAGs specified with "SUBDAG",
	// otherwise NULL).
	char *_dagFile;
  
    // name given to the job by the user
    char* _jobName;

    /** */ status_t _Status;
  
    /*  Job queues
	    NOTE: indexed by queue_t

		WARNING: The execution order of ready nodes is, and
		should always be, undefined. This is so noone starts
		building dag structures that rely on some order of
		execution. In practice, there is an ordering due to the
		data structures that hold the information, but we should
		never rely on that behavior cause the representational
		structures may change. This comment was written because the
		data structures *DID* change, and we had to consider the
		ramifications of it.

		parents -> dependencies coming into the Job 
		children -> dependencies going out of the Job
		waiting -> Jobs on which the current Job is waiting for output
    */ 
	
	std::set<JobID_t> _queues[3];

    /*	The ID of this job.  This serves as a primary key for Jobs, where each
		Job's ID is unique from all the rest 
	*/ 
	JobID_t _jobID;

    /*  Ensures that all jobID's are unique.  Starts at zero and increments
        by one for every Job object that is constructed
    */
    static JobID_t _jobID_counter;

		// True if the node job has been submitted and is idle.
	bool _isIdle;

		// This node's category; points to an object "owned" by the
		// ThrottleByCategory object.
	ThrottleByCategory::ThrottleInfo *_throttleInfo;

		// Whether this node's log file is currently being monitored.
	bool _logIsMonitored;

		// Whether this node uses the default user log file.
	bool _useDefaultLog;

		// The log file for this job -- it will be assigned the default
		// log file name if no log file is specified in the submit file.
	char *_logFile;

		// Whether the log file is XML.
	bool _logFileIsXml;


		// Whether this is a noop job (shouldn't actually be submitted
		// to Condor).
	bool _noop;

		// The job tag for this node ("-" if nothing is specified;
		// can also be "local").
	char *_jobTag;

		// The jobstate.log sequence number for this node (used if we are
		// writing the jobstate.log file for Pegasus or others to read).
	int _jobstateSeqNum;

		// The next jobstate.log sequence number for the entire DAG.  Note
		// that, when we run a rescue DAG, we pick up the sequence numbers
		// from where we left off when we originally ran the DAG.
	static int _nextJobstateSeqNum;

		// The time of the most recent event related to this job.
	time_t _lastEventTime;

		// Skip the rest of the node (and consider it successful) if the
		// PRE script exits with this value.  (-1 means undefined.)
	int _preskip;

	enum {
		PRE_SKIP_INVALID = -1,
		PRE_SKIP_MIN = 0,
		PRE_SKIP_MAX = 0xff
	};

	// whether this is a final job
	bool _final;
	bool append_default_log;

		//
		// For metrics reporting.
		//
	enum {
		EXEC_MASK = 0x1,
		ABORT_TERM_MASK = 0x2
	};

		// _gotEvents[proc] & EXEC_MASK is true iff we've gotten an
		// execute event for proc; _gotEvents[proc] & ABORT_TERM_MASK
		// is true iff we've gotten an aborted or terminated event
		// for proc.
	std::vector<unsigned char> _gotEvents;	
};

/** A wrapper function for Job::Print which allows a NULL job pointer.
    @param job Pointer to job object, if NULL then "(UNKNOWN)" is printed
    @param condorID If true, also print the job's CondorID
*/
void job_print (Job * job, bool condorID = false);


#endif /* ifndef JOB_H */
