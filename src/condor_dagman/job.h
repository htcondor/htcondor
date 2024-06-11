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
#include "condor_id.h"
#include "throttle_by_category.h"
#include "read_multiple_logs.h"
#include "CondorError.h"
#include "stringSpace.h"
#include "submit_utils.h"

#include <deque>
#include <forward_list>
#include <algorithm>
#include <set>

class ThrottleByCategory;
class Dag;
class DagmanMetrics;

//
// Local DAGMan includes
//
#include "debug.h"
#include "script.h"


typedef int JobID_t;
#define NO_ID -1

typedef int EdgeID_t;
#define NO_EDGE_ID -1

enum NodeType {
	JOB,
	FINAL,
	PROVISIONER,
	SERVICE
};

#define EXEC_MASK 0x1
#define ABORT_TERM_MASK 0x2
#define IDLE_MASK 0x4 // set when proc is idle, formerly a separate _isIdle vector
#define HOLD_MASK 0x8 // set when proc is held, formerly a separate _onHold vector

/**  The job class represents a job in the DAG and its state in the HTCondor
     system.  A job is given a name, a CondorID, and three queues.  The
     parents queue is a list of parent jobs that this one depends on.  That
     queue never changes once set.  The waiting queue is the same as the
     parents queue, but shrinks to emptiness has the parent jobs complete.
     The children queue is a list of child jobs that depend on this one.
     Once set, this queue never changes.<p>

     From DAGMan's point of view, a job has six basic states in the
     HTCondor system (refer to the Job::status_t enumeration).  It
     starts out as READY, meaning that it has not yet been submitted.
     If the job has a PRE script defined it changes to PRERUN while
     that script executes.  The job's state changes to SUBMITTED once
     in the HTCondor system.  If it completes successfully and the job
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
  
	// true when node has no parents at this time
	bool NoParents() const {
		// Once we are done with AdjustEdges this should agree
		// but some code checks for parents during parse time (see the splice code)
		// so we check _numparents (set at parse time) and also _parent (set by AdjustEdges)
		return _parent == NO_ID && _numparents == 0;
	}

	// true when node has no children at this time
	bool NoChildren() const {
		return _child == NO_ID;
	}

	// returns the number of children.  NOTE: this is not guaranteed to be fast!
	int CountChildren() const;

    /** The Status of a Job
        If you update this enum, you *must* also update status_t_names
		and the IsActive() method, etc.
    */
	// WARNING!  status_t and status_t_names must be kept in sync!!
	// WARNING!  Don't change the values of existing enums -- the
	// node status file relies on the values staying the same.
    enum status_t {
		/** Job is not ready (for final) */ STATUS_NOT_READY = 0,
        /** Job is ready for submission */ STATUS_READY = 1,
        /** Job waiting for PRE script */  STATUS_PRERUN = 2,
        /** Job has been submitted */      STATUS_SUBMITTED = 3,
        /** Job waiting for POST script */ STATUS_POSTRUN = 4,
        /** Job is done */                 STATUS_DONE = 5,
        /** Job exited abnormally */       STATUS_ERROR = 6,
        /** Job Ancestor Failure cant run*/ STATUS_FUTILE = 7,
    };

    /** The string names for the status_t enumeration.  Use this the same
        way you would use the queue_t_names array.
    */
	// WARNING!  status_t and status_t_names must be kept in sync!!
	static const char * status_t_names[];

	// explanation text for errors
	std::string error_text;

	static int NOOP_NODE_PROCID;
  
    /** Constructor
        @param jobName Name of job in dag file.  String is deep copied.
		@param directory Directory to run the node in, "" if current
		       directory.  String is deep copied.
        @param cmdFile Path to condor cmd file.  String is deep copied.
		@param submitDesc SubmitHash of all submit parameters (optional, alternative
		    to cmdFile)
    */
    Job( const char* jobName,
				const char* directory, const char* cmdFile );
  
    ~Job();

		/** Clean up memory that's no longer needed once a node has
			finished.  (Note that this doesn't mean that the Job object
			is not valid -- it just cleans up some temporary memory.)
			Also check that we got a consistent set of events for the
			metrics.
		*/
	void Cleanup();

	void PrefixName(const std::string &prefix);
	inline const char* GetJobName() const { return _jobName; }
	inline const char* GetDirectory() const { return _directory; }
	inline const char* GetCmdFile() const { return _cmdFile; }
	inline SubmitHash* GetSubmitDesc() const { return _submitDesc; }
	void setSubmitDesc( SubmitHash *submitDesc ) { _submitDesc = submitDesc; }
	inline JobID_t GetJobID() const { return _jobID; }
	inline int GetRetryMax() const { return retry_max; }
	void SetRetryMax( int new_max ) { retry_max = new_max; }
	inline int GetRetries() const { return retries; }
	const char* GetPreScriptName() const;
	const char* GetPostScriptName() const;
	const char* GetHoldScriptName() const;
	static const char* JobTypeString() { return "HTCondor"; }
	inline int GetProcEventsSize() const { return _gotEvents.size(); }
	inline unsigned char GetProcEvent( int proc ) const { return _gotEvents[proc]; }
	//If this is a factory/late materialization cluster we have to wait for a cluster
	//remove event. Otherwise if queued nodes is 0 then all job procs are done
	inline bool AllProcsDone() const { return !is_factory && _queuedNodeJobProcs == 0; }

	bool AddScript(Script* script);
	bool AddPreSkip( int exitCode, std::string &whynot );

	void SetType( NodeType type ) { _type = type; }
	NodeType GetType() const { return _type; }
	void SetNoop( bool value ) { _noop = value; }
	bool GetNoop( void ) const { return _noop; }
	void SetHold( bool value ) { _hold = value; }
	bool GetHold( void ) const { return _hold; }

	Script * _scriptPre;
	Script * _scriptPost;
	Script * _scriptHold;


	//Mark that the node is set to preDone meaning user defined done node
	inline void SetPreDone() { _preDone = true; }
	//Return if the node was set to Done by User
	inline bool IsPreDone() const { return _preDone; }
	// returns true if the job is waiting for other jobs to finish
  	bool IsWaiting() const { return (_parent != NO_ID) && ! _parents_done; };
 	// remove this parent from the waiting collection, and ! IsWaiting
	bool ParentComplete(Job * parent);
	// append parent node names into the given buffer using the given printf format string
	int PrintParents(std::string & buf, size_t bufmax, const Dag* dag, const char * fmt) const;
	// append child node names into the given buffer using the given printf format string
	int PrintChildren(std::string & buf, size_t bufmax, const Dag* dag, const char * fmt) const;

	// append child node names to the given file using the given printf format string
	//int PrintChildren(FILE* fp, const Dag* dag, const char * fmt) const;

	// visit child nodes calling the given function for each
	int VisitChildren(Dag& dag, int(*fn)(Dag& dag, Job* me, Job* child, void* args), void* args);

	// notify children of parent completion, and call the optional callback for each
	// child that is no longer waiting
	int NotifyChildren(Dag& dag, bool(*fn)(Dag& dag, Job* child));

	// Recursively set all descendant nodes to status FUTILE
	// @return the number of nodes set to status
	int SetDescendantsToFutile(Dag& dag);

	/** Returns true if this job is ready for submission.
		@return true if job is submittable, false if not
	*/
	inline bool CanSubmit() const { return (_Status == STATUS_READY) && ! IsWaiting(); }

    /** Returns the node's current status
        @return the node's current status
    */
	inline status_t GetStatus() const { return _Status; }

    /** Returns the node's current status string
        @return address of a string describing the node's current status
    */
	const char* GetStatusName() const;

    /** Sets the node's current status
        @return true: status change was successful, false: otherwise
    */
	bool SetStatus( status_t newStatus );
	
	/** Get whether the specified proc is idle.
		@param proc The proc for which we're getting idle status
		@return true iff the specified proc is idle; false otherwise
	*/
	bool GetProcIsIdle( int proc );

	/** Set whether the specified proc is idle.
		@param proc The proc for which we're setting idle status
		@param isIdle True iff the specified proc is idle; false otherwise
	*/
	void SetProcIsIdle( int proc, bool isIdle );

	/** Set an event for a proc
		@param proc The proc for which we're setting
		@param event The event
	*/
	void SetProcEvent( int proc, int event );

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

    /** Dump the contents of this Job to stdout for debugging purposes.
		@param the current DAG (used to translate node ID to node object)
	*/
    void Dump ( const Dag *dag ) const;
  
		// double-check internal data structures for consistency
	bool SanityCheck() const;

	bool CanAddParent(Job* parent, std::string &whynot);
	bool CanAddChild(Job* child, std::string &whynot) const;
	// check to see if we can add this as a child, and it allows us as a parent..
	bool CanAddChildren(std::forward_list<Job*> & children, std::string &whynot);

	// insert a SORTED list of UNIQUE children.
	// the caller is responsible for calling sort() and unique() on the list if needed
	// before passing it to this function
	bool AddChildren(std::forward_list<Job*> & children, std::string &whynot);

	bool AddVar(const char * name, const char * value, const char* filename, int lineno, bool prepend);
	void ShrinkVars() { /*varsFromDag.shrink_to_fit();*/ }
	bool HasVars() const { return ! varsFromDag.empty(); }
	int PrintVars(std::string &vars);

	// called after the DAG has been parsed to build the parent and waiting edge lists
	void BeginAdjustEdges(Dag* dag);
	void AdjustEdges(Dag* dag);
	void FinalizeAdjustEdges(Dag* dag);

		// should be called when the job terminates
	bool TerminateSuccess();
	bool TerminateFailure();

		/** Should this node abort the DAG?
        	@return true: node should abort the DAG; false node should
				not abort the DAG
		*/
	bool DoAbort() const { return have_abort_dag_val &&
				( retval == abort_dag_val ); }

		/** Should we retry this node (if it failed)?
			@return true: retry the node; false: don't retry
		*/
	bool DoRetry() const { return !DoAbort() &&
				( GetRetries() < GetRetryMax() ); }

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

	/** Add a prefix to the Directory setting for this job. If the prefix
		is ".", then do nothing.
		@param prefix: the prefix to be joined to the directory using "/"
		@return void
	*/
	void PrefixDirectory( std::string &prefix );

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
	time_t GetLastEventTime() const { return _lastEventTime; }

	bool HasPreSkip() const { return _preskip != PRE_SKIP_INVALID; }
	int GetPreSkip() const;
	
	int GetCluster() const { return _CondorID._cluster; }
	int GetProc() const { return _CondorID._proc; }
	int GetSubProc() const { return _CondorID._subproc; }
	bool SetCondorID(const CondorID& cid);
	const CondorID& GetID() const { return _CondorID; }

	void SetSaveFile(const std::string& saveFile) { _saveFile = saveFile; _isSavePoint = true; }
	inline std::string GetSaveFile() const { return _saveFile; }
	inline bool IsSavePoint() const { return _isSavePoint; }

	void setSubPrio(int subPrio) { this->subPriority = subPrio;}

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

		// Special return code indicating that the entire DAG should be
		// aborted.
	int abort_dag_val;

		// The exit code that this DAG will return on abort.
	int abort_dag_return_val;

	//DFS ordering of the node
	int _dfsOrder;

/// bool variables are collected together to reduce memory usage of the Job class

	//Node has been visited by DFS order
	bool _visited;

	// indicates whether retry_abort_val has been set
	bool have_retry_abort_val;
		// Indicates whether abort_dag_val was set.
	bool have_abort_dag_val;

		// Indicates whether abort_dag_return_val was set.
	bool have_abort_dag_return_val;

		// Indicates if this is a factory submit cluster in terms of late materialization
	bool is_factory;

	// somewhat kludgey, but this indicates to Dag::TerminateJob()
	// whether Dag::_numJobsDone has been incremented for this node
	// yet or not (since that method can now be called more than once
	// for a given node); it should not be examined or changed
	// unless/until node is STATUS_DONE
	bool countedAsDone;

	// Indicates that this node is going to write a save point file.
	bool _isSavePoint;

	void SetNumSubmitted(int num) { numJobsSubmitted = num; }
	int NumSubmitted() const { return numJobsSubmitted; }

	void IncrementJobsAborted() { numJobsAborted++; }
	int JobsAborted() const { return numJobsAborted; }

	// Indicate that at this point the node is considered failed
	void MarkFailed() { isSuccessful = false; }
	// Check if the node is considered failed/success at point in time
	bool IsSuccessful() const { return isSuccessful; }

	void CountJobExitCode(int code) {
		if (exitCodeCounts.contains(code)) {
			exitCodeCounts[code]++;
		} else {
			exitCodeCounts[code] = 1;
		}
	}

	const std::map<int, int>& JobExitCodes() const { return exitCodeCounts; }

	void ResetJobInfo() {
		_numSubmittedProcs = 0;
		numJobsSubmitted = 0;
		numJobsAborted = 0;
		isSuccessful = true;
		exitCodeCounts.clear();
	}

private:
		// Whether this is a noop job (shouldn't actually be submitted
		// to HTCondor).
	bool _noop;
		// Whether this is a hold job (should be submitted on hold)
	bool _hold;
		// What type of node (job, final, provisioner)
	NodeType _type;

	bool isSuccessful{true}; // Is Node currently successful or not

	int numJobsSubmitted{0}; // Number of submitted jobs
	int numJobsAborted{0}; // Number of jobs with abort events

public:

	struct NodeVar {
		const char * _name; // stringspace string, not owned by this struct
		const char * _value; // stringspace string, not owned by this struct
		bool _prepend; //bool to determine if variable is prepended or appended
		NodeVar(const char * n, const char * v, bool p) : _name(n), _value(v), _prepend(p) {}
	};
	std::forward_list<NodeVar> varsFromDag;

		// Count of the number of job procs currently in the batch system
		// queue for this node.
	int _queuedNodeJobProcs;

		// Count of the number of overall procs submitted for this job
	int _numSubmittedProcs;

		// Node priority.  Higher number is better priority (submit first).
		// Explicit priority is the priority actually set in the DAG
		// file (0 if not set).
	int _explicitPriority;
		// Effective priority is the priority at which we're going to
		// actually submit the job (explicit priority adjusted
		// according to the DAG priority algorithm).
	int _effectivePriority;

	// We sort the nodes by effective priority above, but we often want
	// to further sort nodes of the same effective priority by another
	// criteria, e.g. to shuffle a recently-failed node to the end
	// of the set of nodes of the same priority.  This field does
	// that.
	int subPriority;

		// The number of times this job has been held.  (Note: the current
		// implementation counts holds for all procs in a multi-proc cluster
		// together -- that should get changed eventually.)
	int _timesHeld;

		// The number of jobs procs of this node that are currently held.
		// (Note: we may need to track the hold state of each proc in a
		// cluster separately to correctly deal with multi-proc clusters.)
	int _jobProcsOnHold;

		/** Mark a job with ProcId == proc as being on hold
 			Returns false if the job is already on hold
		*/
 
	bool Hold(int proc);
	
		/** Mark a job with ProcId == proc as being released
 		    Returns false if the job is not on hold
		*/
	bool Release(int proc);

	// Get Shared Node time of the last state change
	static time_t GetLastStateChange() { return lastStateChangeTime; }

	// Check internal Job states against returned queue query results
	bool VerifyJobStates(std::set<int>& queuedJobs);

	bool missingJobs{false};

	static const char * dedup_str(const char* str) { return stringSpace.strdup_dedup(str); }

private:
	// private methods for use by AdjustEdges
	void AdjustEdges_AddParentToChild(Dag* dag, JobID_t child_id, Job* parent);

	// propagate parent completion to the children as part of AdjustEdges.
	// NOT USED at present because bootstrap assumes that none of the children
	// have been marked ready when the dag has finished loading.
	//void AdjustEdges_NotifyChild(Dag* dag, JobID_t child_id, Job* parent);

        // StringSpace class de-dups _directory and _cmdFile strings, since
        // these two string may be repeated thousands of times in a large DAG
    static StringSpace stringSpace;

		// Directory to cd to before running the job or the PRE and POST
		// scripts.
        // Do not malloc or free! _directory is managed in a StringSpace!
	const char * _directory;

        // filename of condor submit file
        // Do not malloc or free! _directory is managed in a StringSpace!
    const char * _cmdFile;

		// SubmitHash of submit desciption
		// Alternative submission method to _cmdFile above.
	SubmitHash* _submitDesc;

	// Filename of DAG file (only for nested DAGs specified with "SUBDAG",
	// otherwise NULL).
	char *_dagFile;
  
    // name given to the job by the user
    char* _jobName;

	// Filename to write save point rescue file as
	std::string _saveFile;

	std::map<int, int> exitCodeCounts; // Exit Code : Number of jobs that returned code

    /** */ status_t _Status;

	// these may be job ids when there is a single dependency
	// they will be edge ids when there are multiple dependencies
	JobID_t _parent;
	JobID_t _child;
	// we count the parents as we add the child edges
	// then in AdjustEdges, we build the parent lists from the child lists
	// this allows us to allocate the vectors for parent and waiting up front
	int _numparents;

	bool _multiple_parents;	 // true when _parent is a EdgeID rather than a JobID
	bool _multiple_children; // true when _child is an EdgeID rather than a JobID
	bool _parents_done;      // set to true when all of the parents of this node are done
	bool _spare;
	bool _preDone;           // true when user defines node as done in *.dag file

    /*	The ID of this job.  This serves as a primary key for Jobs, where each
		Job's ID is unique from all the rest 
	*/ 
	JobID_t _jobID;

    /*  Ensures that all jobID's are unique.  Starts at zero and increments
        by one for every Job object that is constructed
    */
    static JobID_t _jobID_counter;

		// The jobstate.log sequence number for this node (used if we are
		// writing the jobstate.log file for Pegasus or others to read).
	int _jobstateSeqNum;

		// The next jobstate.log sequence number for the entire DAG.  Note
		// that, when we run a rescue DAG, we pick up the sequence numbers
		// from where we left off when we originally ran the DAG.
	static int _nextJobstateSeqNum;

		// Skip the rest of the node (and consider it successful) if the
		// PRE script exits with this value.  (-1 means undefined.)
	int _preskip;

	enum {
		PRE_SKIP_INVALID = -1,
		PRE_SKIP_MIN = 0,
		PRE_SKIP_MAX = 0xff
	};

		// The time of the most recent event related to this job.
	time_t _lastEventTime;

		// This node's category; points to an object "owned" by the
		// ThrottleByCategory object.
	ThrottleByCategory::ThrottleInfo *_throttleInfo;

		// The job tag for this node ("-" if nothing is specified;
		// can also be "local").
	char *_jobTag;

		// _gotEvents[proc] & EXEC_MASK is true iff we've gotten an
		// execute event for proc; _gotEvents[proc] & ABORT_TERM_MASK
		// is true iff we've gotten an aborted or terminated event
		// for proc.
	std::vector<unsigned char> _gotEvents;	

	/** Print the list of which procs are idle/not idle for this node
	 *  (for debugging).
	*/
	void PrintProcIsIdle();

	static void SetStateChangeTime() { time(&lastStateChangeTime); }
	static time_t lastStateChangeTime;
};

struct SortJobsById
{
	bool operator ()(const Job* a, const Job* b) {
		return a->GetJobID() < b->GetJobID();
	}
	//bool operator ()(const Job & a, const Job & b) { return a.GetJobID() < b.GetJobID(); }
};

struct EqualJobsById
{
	bool operator ()(const Job* a, const Job* b) {
		return a->GetJobID() == b->GetJobID();
	}
	// bool operator ()(const Job & a, const Job & b) { return a.GetJobID() == b.GetJobID(); }
};


// This class holds multiple JobId entries in a sorted vector, use it to hold
// either the parent or child list for a Job node.
//
// The collection of all Edge data structures is owned by the static _edgeTable
// member of this class, which can be used to lookup a Edge by id;
// An EdgeID is essentially an index into this table.
//
// An edge cannot be freed individually once allocated, but it can be resized.
//
class Edge {
protected:
	Edge() {};
	Edge(std::vector<JobID_t> & in) : _ary(in) {};
public:
	virtual ~Edge() {};

	std::vector<JobID_t> _ary; // sorted array  of jobid's, either parent or child edge list
	//std::set<JobID_t> _check; // used to double check the correctness of the edge list.
	bool Add(JobID_t id) {
		//_check.insert(id);
		auto it = std::lower_bound(_ary.begin(), _ary.end(), id);
		if ((it != _ary.end()) && (*it == id)) {
			return false;
		}
		_ary.insert(it, id);
		return true;
	}
	inline size_t size() const {
		return _ary.size();
	}
	inline bool empty() const {
		return _ary.empty();
	}

	// this table holds all of the allocated edges, stored by EdgeId (which is the index into the table)
	static std::deque<std::unique_ptr<Edge>> _edgeTable;

	static Edge * ById(EdgeID_t id) {
		if (id >= 0 && id < (EdgeID_t)_edgeTable.size())
			return _edgeTable.at(id).get();
		return NULL;
	}
	// create a new, empty edge returning it's ID
	static EdgeID_t NewEdge(Edge* & edge) {
		EdgeID_t id = (EdgeID_t)_edgeTable.size();
		edge = new Edge();
		_edgeTable.push_back(std::unique_ptr<Edge>(edge));
		return id;
	}
	// create an edge as a copy of an existing edge, returning the new EdgeID
	static EdgeID_t NewCopy(EdgeID_t id) {
		Edge* from = ById(id);
		if ( ! from) return NO_ID;
		id = (EdgeID_t)_edgeTable.size();
		_edgeTable.push_back(std::unique_ptr<Edge>(new Edge(from->_ary)));
		return id;
	}

	// helper method for job methods. When called this will look at incoming multi flag
	// and id flag.  If multi is true, then id is actually an EdgeID.  If it is false
	// then it is a JobID and an Edge needs to be allocated. 
	// 
	// On exit, multi will be true.  id will be the Id of the Edge (possibly newly created)
	// and first_id will be the former value of id IFF it was a JobID and not an EdgeID.
	// in most cases the caller will want to insert first_id into the Edge if it is not NO_ID
	//
	static Edge* PromoteToMultiple(JobID_t & id, bool & multi, JobID_t & first_id) {
		Edge* edge = NULL;
		if (multi && (id != NO_ID)) {
			first_id = NO_ID; // already multiple so no need to save id as first_id
			edge = ById(id);
		} else {
			first_id = id; // we are promoting so save id as first id
			id = NewEdge(edge);
			multi = true;
		}
		return edge;
	}
};

// Add a waiting count and bit array to the Edge array.
// The _wait bit array should be the same size as the Edge::_ary
// it's entries correspond to the entries in the Edge:_ary
// because of this. inserting or appending entries in the _ary will
// invalidate both the _wait and _num_waiting fields.
// this structure should only be initialized after the _ary is fully populated
//
class WaitEdge : public Edge {
protected:
	WaitEdge(int num) : Edge() {
		_ary.reserve(num);
		_wait.resize(num, true);
		_num_waiting = num;
	};
	std::vector<bool> _wait;  // a bit vector where true=waiting, the same size and order as _ary
	int _num_waiting;         // the number of true bits in the _wait vector
public:
	virtual ~WaitEdge() {};

	static EdgeID_t NewWaitEdge(int num) {
		EdgeID_t id = (EdgeID_t)_edgeTable.size();
		_edgeTable.push_back(std::unique_ptr<Edge>(new WaitEdge(num)));
		return id;
	}

	static WaitEdge * ById(EdgeID_t id) {
		if (id >= 0 && id < (EdgeID_t)_edgeTable.size())
			return static_cast<WaitEdge*>(_edgeTable.at(id).get());
		return NULL;
	}

	int Waiting() const { return _num_waiting; }

	void MarkAllWaiting() {
		_num_waiting = (int)_wait.size();
		_wait.clear();
		_wait.resize(_num_waiting, true);
	}

	bool MarkDone(JobID_t id, bool & already_done) {
		auto it = std::lower_bound(_ary.begin(), _ary.end(), id);
		if ((it != _ary.end()) && (*it == id)) {
			size_t index = it - _ary.begin();
			already_done = true;
			if (_wait[index]) {
				_wait[index] = false;
				already_done = false;
				_num_waiting -= 1;
			}
			return true;
		}
		return false;
	}

};


// return true if a collection has more than a single item in it
template<class T> bool more_than_one(T & lst)
{
	auto it = lst.cbegin();
	return (it != lst.cend()) && (++it != lst.cend());
}


#endif /* ifndef JOB_H */
