/***************************************************************
 *
 * Copyright (C) 1990-2024, Condor Team, Computer Sciences Department,
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

#ifndef NODE_H
#define NODE_H

#include "condor_common.h"      /* for <stdio.h> */
#include "condor_id.h"
#include "throttle_by_category.h"
#include "read_multiple_logs.h"
#include "CondorError.h"
#include "submit_utils.h"
#include "dag_commands.h"

#include <deque>
#include <algorithm>
#include <functional>
#include <set>

class ThrottleByCategory;
class Dag;

// Local DAGMan includes
#include "debug.h"
#include "script.h"
#include "edge.h"

// Node Type
enum NodeType {
	JOB,
	FINAL,
	PROVISIONER,
	SERVICE
};

// Job proc event masks
const int EXEC_MASK = (1 << 0);
const int ABORT_TERM_MASK = (1 << 1);
const int IDLE_MASK = (1 << 2); // Set if proc is idle
const int HOLD_MASK = (1 << 3); // Set if proc is held

// Constant success value
const int SUCCESS = 0;
const short int JOB_EXIT_UNKNOWN = std::numeric_limits<short int>::min();
const short int JOB_EXIT_ABORT = std::numeric_limits<short int>::max();

// Information tracked for each job associated with this node
struct JobDetails {
	unsigned char events{0}; // State of job tracked via job proc event masks
	short int exitVal{JOB_EXIT_UNKNOWN}; // Exit code of the job (or aborted value)
};

/*
* Node Class:
*     Represents and manages a single unit of DAG work (pre script,
*     job(s), post script). This keeps track of necessary information
*     regarding jobs placed to the AP, the state of the node, scripts
*     to be run, and parent/child relations to other nodes.
*
*     Note: Each node has a unique name for identification
*/
class Node {
public:

	// Status of node (may need update IsActive() upon changing)
	// WARNING!  status_t and status_t_names must be kept in sync!!
	// WARNING!  Don't change the values of existing enums -- the
	// node status file relies on the values staying the same.
	enum status_t {
		/** Node is not ready (for final) */ STATUS_NOT_READY = 0,
		/** Node is ready for submission */  STATUS_READY = 1,
		/** Node waiting for PRE script */   STATUS_PRERUN = 2,
		/** Node has been submitted */       STATUS_SUBMITTED = 3,
		/** Node waiting for POST script */  STATUS_POSTRUN = 4,
		/** Node is done */                  STATUS_DONE = 5,
		/** Node exited abnormally */        STATUS_ERROR = 6,
		/** Node Ancestor Failure cant run*/ STATUS_FUTILE = 7,
	};

	static const std::set<status_t> ACTIVE_STATES;
	static const char* status_t_names[];
	static int NOOP_NODE_PROCID;

	Node(const char* jobName, const char* directory, const char* cmdFile);
	~Node() {
		delete _scriptPre;
		delete _scriptPost;
		delete _scriptHold;
	}

	// Dump condensed node information to debug log
	void Dump(const Dag *dag) const;
	// double-check internal data structures for consistency
	bool SanityCheck() const;

	// Get this nodes unique ID number
	inline node_id_t GetNodeID() const { return _nodeID; }
	// Get this nodes edge ID reference number
	inline edge_id_t GetEdgeID() const { return m_children; }
	// Set this nodes Edge ID reference number
	inline void SetEdgeID(edge_id_t id) { m_children = id; }
	// Returns true if node has exactly one parent (stored inline)
	inline bool HasSingleParent() const { return !m_multiple_parents && m_parents != NO_ID; }
	// Returns true if node has more than one parent (wait edge allocated)
	inline bool HasMultipleParents() const { return m_multiple_parents; }
	// Get raw parents ID (node_id_t when single parent, edge_id_t when multiple)
	inline connect_id_t GetParentsID() const { return m_parents; }
	// Set single parent node ID (clears multiple-parents flag)
	inline void SetSingleParent(node_id_t id) { m_parents = id; m_multiple_parents = false; }
	// Set wait edge ID (sets multiple-parents flag)
	inline void SetWaitEdge(edge_id_t id) { m_parents = id; m_multiple_parents = true; }
	// Set Type of Node
	void SetType(NodeType type) { _type = type; }
	// Get Type of Node
	NodeType GetType() const { return _type; }

	// Check that this node can have a parent node(s)
	bool CanAddParent(std::string &whynot);
	// check to see if we can add this as a child, and it allows us as a parent..
	bool CanAddChildren(std::string &whynot);

	// Node has no parent nodes
	bool NoParents() const { return !m_multiple_parents && m_parents == NO_ID; }
	// Node has no children nodes
	bool NoChildren() const { return m_children == NO_EDGE_ID; }

	// returns true if the job is waiting for other jobs to finish
	bool IsWaiting() const { return (m_multiple_parents || m_parents != NO_ID) && !_parents_done; };
	// visit child nodes calling the given function for each
	int VisitChildren(Dag& dag, const std::function<int(Dag& dag, Node* me, Node* child)>& fn);
	// notify children of parent completion, and call the optional callback for each child that is no longer waiting
	void NotifyChildren(Dag& dag, const std::function<bool(Dag& dag, Node* child)>& fn);
	// Recursively set descendants to status FUTILE (strong deps), or -- for a direct child
	// reached via a weak dependency -- treat this exactly like a normal parent-completion
	// notification instead. Returns the number of nodes newly set to FUTILE.
	int SetDescendantsToFutile(Dag& dag, const std::function<bool(Dag& dag, Node* child)>& on_weak_unblocked = nullptr);
	// True if this node has at least one child and every child dependency is weak
	// (used by Dag::WriteRescue to skip re-running a failed node on rescue).
	bool AllChildrenWeak(const Dag* dag) const;

	// append parent node names into the given buffer using the given printf format string
	int PrintParents(std::string & buf, size_t bufmax, const Dag* dag, const char* sep = " ") const;
	// append child node names into the given buffer using the given printf format string
	int PrintChildren(std::string & buf, size_t bufmax, const Dag* dag, const char* sep = " ") const;

	//Mark that the node is set to preDone meaning user defined done node
	inline void SetPreDone() { _preDone = true; }
	//Return if the node was set to Done by User
	inline bool IsPreDone() const { return _preDone; }
	// Indicate that at this point the node is considered failed
	void MarkFailed() { isSuccessful = false; }
	// Check if the node is considered failed/success at point in time
	bool IsSuccessful() const { return isSuccessful; }
	// Mark node as already counted as done
	void MarkDone() { countedAsDone = true; }
	// Check if node has already been counted as done
	bool AlreadyDone() const { return countedAsDone; }
	// Node is done: Success
	bool TerminateSuccess() { SetStatus(STATUS_DONE); Cleanup(); return true; }
	// Node is done: Failure
	bool TerminateFailure() { SetStatus(STATUS_ERROR); Cleanup(); return true; }
	// Return whter or not node is active (running pre script, job(s), or post script)
	bool IsActive() const { return ACTIVE_STATES.contains(_Status); };

	// Get current node status
	inline status_t GetStatus() const { return _Status; }
	// Get current node status string
	const char* GetStatusName() const { return status_t_names[_Status]; }
	// Set node status
	bool SetStatus(status_t newStatus);
	// Set Node return value
	void SetReturnValue(int val) { retval = val; }
	// Get Node return value
	int GetReturnValue() const { return retval; }

	// Check if node should cause DAG to abort
	bool DoAbort() const { return have_abort_dag_val && retval == abort_dag_val; }
	// Set abort DAG on condition
	void SetAbortDagOn(int abortVal, int returnVal) {
		have_abort_dag_val = true;
		abort_dag_val = abortVal;
		abort_dag_return_val = returnVal; // Limited range of 0-255
	}
	bool HasAbortCode() const { return have_abort_dag_val; } // Has abort DAG on code
	int GetAbortCode() const { return abort_dag_val; } // Get specified abort DAG on code
	bool HasAbortReturnValue() const { return abort_dag_return_val != std::numeric_limits<int>::max(); } // Has abort return code
	int GetAbortReturnValue() const { return abort_dag_return_val; } // Get abort return code

	// Check if node should be retried
	bool DoRetry() const { return ! DoAbort() && GetRetries() < GetRetryMax(); }
	// Abort doing retry of node -> faliure
	bool AbortRetry() const { return have_retry_abort_val && retval == retry_abort_val; }
	bool HasAbortRetry(int& val) const { val = retry_abort_val; return have_retry_abort_val; }
	// Set parsed retry information
	void SetMaxRetries(int max, int unless_exit) {
		retry_max = max;
		if (unless_exit != 0) {
			have_retry_abort_val = true;
			retry_abort_val = unless_exit;
		}
	}
	inline int GetRetryMax() const { return retry_max; } // Get Max Retry attempts
	inline int GetRetries() const { return retries; } // Get Current Retry attempt
	void Retried() { retries++; } // Node is being retried: increment retries
	void AddRetry() { retry_max++; } // Add another possible attempt to retry max
	void PoisonRetries() { retries = retry_max; } // Short circut retries

	// Inform whether or not this job is ready to place job(s) to AP
	inline bool CanSubmit() const { return (_Status == STATUS_READY) && ! IsWaiting(); }
	void AttemptedSubmit() { _submitTries++; } // Increment attempts to place job list to AP
	int GetSubmitAttempts() const { return _submitTries; } // Get job list placement attempts
	// Check if specified job proc is idle
	bool GetProcIsIdle(int proc);
	// Set whether the specified job proc is idle or not
	void SetProcIsIdle(int proc, bool isIdle);
	// Set an event for the specified job proce
	void SetProcEvent( int proc, int event );
	// Mark specified job proc as held (return false if proc was already in hold state)
	bool Hold(int proc);
	// Mark specified job proc as released (return false if proc wasn't in hold state)
	bool Release(int proc, bool warn=true);
	// Check if internally all tracked job procs are done
	inline bool AllProcsDone() const { return !is_factory && _queuedNodeJobProcs == 0; }

	// Set & check if we have lost track of jobs when compared to AP information
	void SetMissingJobs(bool missing) { missingJobs = missing; }
	bool IsMissingJobs() const { return missingJobs; }
	// Check internal Job states against returned queue query results
	bool VerifyJobStates(std::set<int>& queuedJobs);
	// Get Shared Node time of the last state change
	static time_t GetLastStateChange() { return lastStateChangeTime; }

	// Get Internal count of job procs in hold state
	int GetJobsOnHold() const { return _jobProcsOnHold; }
	// Increment internal count of queued job procs
	void JobQueued() { _queuedNodeJobProcs++;  }
	// Decrement internal count of queued job procs
	void JobLeftQueue() { _queuedNodeJobProcs--; }
	// Return internal count of queued job procs
	int GetQueuedJobs() const { return _queuedNodeJobProcs; }
	// Set the number of jobs placed to AP at submit time
	void SetNumSubmitted(int num) { numJobsSubmitted = num; SetStateChangeTime(); }
	// Get the number of jobs placed to AP at submit time
	int NumSubmitted() const { return numJobsSubmitted; }
	// Increment count of job failures
	void JobFailure() { totalJobsFailed++; }
	// Get total number of failed jobs (terminate failure + abort)
	int TotalJobsFailed() const { return totalJobsFailed; }
	// Check if the # job failures have passed tolerance
	bool CheckBatchFailed(int tolerance);

	// Record a specific job exit code
	void RecordJobExitCode(int proc, int code) {
		CheckTrackingJob(proc);
		jobs[proc].exitVal = code;
	}
	// Record a specific job was aborted (no exit code)
	void RecordJobAbort(int proc) {
		CheckTrackingJob(proc);
		jobs[proc].exitVal = JOB_EXIT_ABORT;
	}
	// Get vector of job information
	const std::vector<JobDetails>& GetJobInfo() const { return jobs; }

	// Check whether node has not tracked a job proc already
	bool IsFirstProc() {
		bool first = !readFirstProc;
		readFirstProc = true;
		return first;
	}

	// Set and check if the associated job list is late materialization
	void SetIsFactory() { is_factory = true; }
	bool IsFactory() const { return is_factory; }

	// Set the job list full job ID
	bool SetCondorID(const CondorID& cid);
	// Get the current job list full job ID
	const CondorID& GetID() const { return _CondorID; }

	// Get the job list job ID parts
	int GetCluster() const { return _CondorID._cluster; }
	int GetProc() const { return _CondorID._proc; }
	int GetSubProc() const { return _CondorID._subproc; }

	inline const char* GetNodeName() const { return _nodeName.c_str(); }
	inline const char* GetDirectory() const { return _directory.data(); }
	inline const char* GetCmdFile() const { return _cmdFile.data(); }
	void SetDagFile(const char *dagFile) { if (dagFile) { _dagFile = DAG::STRING_SPACE::DEDUP(dagFile); } }
	const char* GetDagFile() const { return _dagFile.data(); }
	// Prefix node name with splice scope
	void PrefixName(const std::string &prefix) { _nodeName = prefix + _nodeName; }
	// Prefix directory path with splice path for relative directory
	void PrefixDirectory(const std::string &prefix);

	bool AddScript(Script* script);
	const char* GetPreScriptName() const { return _scriptPre ? _scriptPre->GetCmd() : nullptr; }
	const char* GetPostScriptName() const { return _scriptPost ? _scriptPost->GetCmd() : nullptr; }
	const char* GetHoldScriptName() const { return _scriptHold ? _scriptHold->GetCmd() : nullptr; }

	bool AddPreSkip(int exitCode, std::string &whynot);
	bool HasPreSkip() const { return _preskip != PRE_SKIP_INVALID; }
	int GetPreSkip() const { return _preskip; };

	void SetNoop( bool value ) { _noop = value; }
	bool GetNoop() const { return _noop; }

	void SetHold(bool value) { _hold = value; }
	bool GetHold() const { return _hold; }

	void SetSaveFile(const std::string& saveFile) { _saveFile = DAG::STRING_SPACE::DEDUP(saveFile); _isSavePoint = true; }
	inline std::string GetSaveFile() const { return std::string(_saveFile.data()); }
	inline bool IsSavePoint() const { return _isSavePoint; }

	struct NodeVar {
		std::string_view _name;
		std::string_view _value;
		bool _prepend; //bool to determine if variable is prepended or appended
		NodeVar(std::string_view& n, std::string_view& v, bool p) : _name(n), _value(v), _prepend(p) {}
	};

	const std::vector<NodeVar>& GetVars() const { return varsFromDag; }
	bool AddVar(const std::string& name, const std::string& value, bool prepend);
	bool HasVars() const { return ! varsFromDag.empty(); }

	void SetPrio(int prio) { _explicitPriority = _effectivePriority = prio; }
	void AddDagPrio(int prio) { _effectivePriority += prio; }
	void setSubPrio(int prio) { subPriority = prio;}
	int GetExplicitPrio() const { return _explicitPriority; }
	int GetEffectivePrio() const { return _effectivePriority; }
	int GetSubPrio() const { return subPriority; }

	// Set node category
	void SetCategory(const char *categoryName, ThrottleByCategory &catThrottles);
	// Get node throttle category
	ThrottleByCategory::ThrottleInfo* GetThrottleInfo() { return _throttleInfo; }

	// Get the jobstate.log job tag for this node.
	const char* GetJobstateJobTag();
	// Get the jobstate.log sequence number for this node, assigning one if we haven't already.
	int GetJobstateSequenceNum();
	// Reset the jobstate.log sequence number for this node
	void ResetJobstateSequenceNum() { _jobstateSeqNum = 0; }
	// Set the master jobstate.log sequence number.
	static void SetJobstateNextSequenceNum(int nextSeqNum) { _nextJobstateSeqNum = nextSeqNum; }
	// Set the last event time for associated job(s)
	void SetLastEventTime(const ULogEvent *event) { _lastEventTime = event->GetEventclock(); }
	// Get the time at which the most recent event occurred for the job(s)
	time_t GetLastEventTime() const { return _lastEventTime; }

	bool WasVisited() const { return _visited; }
	void MarkVisited() { _visited = true; }
	void SetDfsOrder(int order) { _dfsOrder = order; }
	int GetDfsOrder() const { return _dfsOrder; }

	// Set specific node error message
	void SetErrorMsg(const char* fmt, ...) CHECK_PRINTF_FORMAT(2,3) {
		va_list args;
		va_start(args, fmt);
		vformatstr(error_text, fmt, args);
		va_end(args);
	}
	// Append to node specific error message
	void AppendErrorMsg(const char* fmt, ...) CHECK_PRINTF_FORMAT(2,3) {
		va_list args;
		va_start(args, fmt);
		vformatstr_cat(error_text, fmt, args);
		va_end(args);
	}
	// Return node specific error message
	const char* GetErrorMsg() const { return error_text.c_str(); }

	// Reset internal variables upon retry
	void ResetInfo() {
		numJobsSubmitted = 0;
		totalJobsFailed = 0;
		isSuccessful = true;
		readFirstProc = false;
		is_factory = false;
		error_text.clear();
		jobs.clear();
	}

	void SetTolerance(const int tol, const DAG::ToleranceMode mode, const bool percentage = false) {
		m_failure_tolerance = tol;
		m_tolerance_mode = mode;
		m_tolerance_is_percentage = percentage;
	}

	bool RemoveOnBatchFailure(bool config_rm_on_fail);

	Script* _scriptPre{nullptr};
	Script* _scriptPost{nullptr};
	Script* _scriptHold{nullptr};

private:
	// Print the list of which procs are idle/not idle for this node
	void PrintProcIsIdle();
	// Set last state change time
	static void SetStateChangeTime() { time(&lastStateChangeTime); }
	// Cleanup node memory (Note: does not invalidate node)
	void Cleanup();
	// Record that `parent` has finished (regardless of outcome); returns true once
	// this was the last outstanding parent, i.e. the child is fully unblocked.
	static bool MarkParentDone(Dag& dag, Node* child, node_id_t parent);
	// Set `child` to STATUS_FUTILE unless already FUTILE/preDone; increments count on change.
	// Returns false only when child was already FUTILE (caller should stop recursing that branch).
	static bool InvalidateChild(Node* child, int& count);
	// Unconditionally cascade FUTILE to all descendants -- used once a node itself
	// never executed, so even its weak children can't be satisfied (non-transitive).
	int CascadeFutile(Dag& dag);
	// Check if a proc is already being tracked if not start (also translate NOOP proc id -> 0)
	void CheckTrackingJob(int& proc) {
		if (GetNoop()) { proc = 0; }
		if ((size_t)proc >= jobs.size()) {
			jobs.resize(proc + 1);
		}
	}

	static node_id_t _nodeID_counter; // Counter to give nodes unique ID's
	static int _nextJobstateSeqNum; // The next jobstate log sequnce number
	static time_t lastStateChangeTime; // Last time a node had a state change

	// PRE Skip check values
	enum {
		PRE_SKIP_INVALID = -1,
		PRE_SKIP_MIN = 0,
		PRE_SKIP_MAX = 0xff
	};

	CondorID _CondorID{}; // Associated job ID (24 bytes)

	ThrottleByCategory::ThrottleInfo *_throttleInfo{nullptr}; // Node category throttle (node doesn't own pointer)
	std::vector<JobDetails> jobs{}; // Vector of information pertaining to one job
	std::vector<NodeVar> varsFromDag{}; // Variables add to job list at submit time

	std::string _nodeName{}; // Unique node name provided by user
	std::string error_text{}; // Node error message
	std::string_view _saveFile{}; // Filename to write save point rescue file
	std::string_view _directory{}; // Node directory to run within... Do not malloc or free!
	std::string_view _cmdFile{}; // Job list submit file... Do not malloc or free!
	std::string_view _dagFile{}; // SubDAG filename
	std::string_view _jobTag{}; // Nodes job tag for the jobstate log ("-" if not specified)

	time_t _lastEventTime{0}; // The time of the most recent event related to this job.

	DAG::ToleranceMode m_tolerance_mode{DAG::ToleranceMode::AUTO}; // What to do when job list is considered failed
	NodeType _type{NodeType::JOB}; // Node type (job, final, provisioner...)
	status_t _Status{STATUS_READY}; // Current node status

	node_id_t _nodeID{NO_ID}; // This node's unique node ID
	edge_id_t m_children{NO_EDGE_ID}; // Reference to children Edge/DagArc
	connect_id_t m_parents{NO_ID};   // single parent: node_id_t; multiple parents: edge_id_t into m_wait_edges

	int retry_max{0}; // Maximum number of retry attempts
	int retries{0}; // Current number of retries
	int retry_abort_val{std::numeric_limits<int>::max()}; // Return code that short circuts doing retry

	int _submitTries{0}; // Number of attempts to place job list to AP
	int numJobsSubmitted{0}; // Number of submitted jobs
	int _queuedNodeJobProcs{0}; // Number of job procs currently tracked in queue
	int _jobProcsOnHold{0}; // Number of tracked job procs currently in hold state
	int _timesHeld{0}; // Total number of times jobs in the job list went on hold
	int totalJobsFailed{0}; // Count of jobs that failed (terminate failure or abort)

	int retval{-1}; // Nodes return code

	int _preskip{PRE_SKIP_INVALID}; // Nodes preskip code (to skip rest of node)

	int abort_dag_val{-1}; // Return code to notify DAG to abort
	int abort_dag_return_val{std::numeric_limits<int>::max()}; // Specific value to exit with upon abort

	int m_failure_tolerance{-1}; // How many jobs in list can fail before declaring failure

	int _jobstateSeqNum{0}; // This nodes job state sequence number

	int _dfsOrder{-1}; // DFS ordering of the node

	int _explicitPriority{0}; // User specified node priority (higher is better)
	int _effectivePriority{0}; // Actual node priority (modified by DAG priority)
	int subPriority{0}; // Internal priority for sorting same effective prio nodes based on recent failure

	bool have_retry_abort_val{false}; // Indicate node has retry abort condition value
	bool have_abort_dag_val{false}; // Indicate node has an abort condition value

	bool m_tolerance_is_percentage{false}; // Failure tolerance is a percentage

	bool is_factory{false}; // Indicate placed job list is late materialization factory
	bool _visited{false}; // Node has been visited by DFS order

	bool countedAsDone{false}; // Indicate whether DAG has already terminated this node
	bool isSuccessful{true}; // Is Node currently successful or not
	bool readFirstProc{false}; // DAG has already read a job event (submit) for this node
	bool missingJobs{false}; // Node counts found missing jobs during verifying state with AP

	bool _parents_done{false}; // set to true when all of the parents of this node are done
	bool m_multiple_parents{false};  // disambiguates m_parents interpretation

	bool _preDone{false}; // true when user defines node as done in *.dag file
	bool _isSavePoint{false}; // Indicates that this node is going to write a save point file.
	bool _noop{false}; // Indicate this is a noop node (job list is not placed to AP)
	bool _hold{false}; // Indicate this node should place jobs on hold
};

#endif /* ifndef NODE_H */
