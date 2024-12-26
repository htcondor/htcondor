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
#include "stringSpace.h"
#include "submit_utils.h"

#include <deque>
#include <forward_list>
#include <algorithm>
#include <set>

class ThrottleByCategory;
class Dag;

// Local DAGMan includes
#include "debug.h"
#include "script.h"

// Node Type
enum NodeType {
	JOB,
	FINAL,
	PROVISIONER,
	SERVICE
};

typedef int NodeID_t;
typedef int EdgeID_t;

const NodeID_t NO_ID = -1;
const EdgeID_t NO_EDGE_ID = -1;

// Job proc event masks
const int EXEC_MASK = (1 << 0);
const int ABORT_TERM_MASK = (1 << 1);
const int IDLE_MASK = (1 << 2);
const int HOLD_MASK = (1 << 3);

// Constant success value
const int SUCCESS = 0;

/*
*	Node Class:
*	    Represents and manages a single unit of DAG work (pre script,
*	    job(s), post script). This keeps track of necessary information
*	    regarding jobs placed to the AP, the state of the node, scripts
*	    to be run, and parent/child relations to other nodes.
*
*	    Note: Each node has a unique name for identification
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

	static const char * status_t_names[];
	static int NOOP_NODE_PROCID;

	Node(const char* jobName, const char* directory, const char* cmdFile);
	~Node();

	// Cleanup node memory (Note: does not invalidate node)
	void Cleanup();
	// Dumb condensed node information to debug log
	void Dump(const Dag *dag) const;
	// double-check internal data structures for consistency
	bool SanityCheck() const;

	// Get this nodes unique ID number
	inline NodeID_t GetNodeID() const { return _nodeID; }
	// Set Type of Node
	void SetType(NodeType type) { _type = type; }
	// Get Type of Node
	NodeType GetType() const { return _type; }

	// Check that this node can have a parent node(s)
	bool CanAddParent(Node* parent, std::string &whynot);
	// Check that this node can have children node(s)
	bool CanAddChild(Node* child, std::string &whynot) const;
	// check to see if we can add this as a child, and it allows us as a parent..
	bool CanAddChildren(std::forward_list<Node*> & children, std::string &whynot);
	// Add SORTED list of UNIQUE children (caller responsible for sort & unique).
	bool AddChildren(std::forward_list<Node*> & children, std::string &whynot);

	// Node has no parent nodes
	bool NoParents() const { return _parent == NO_ID && _numparents == 0; }
	// Node has no children nodes
	bool NoChildren() const { return _child == NO_ID; }
	// Check if specified node is a child of this node
	bool HasChild(Node* child);
	// Check if specified node is a parent of this node
	bool HasParent(Node* parent);

	// Returns the number of children.  NOTE: this is not guaranteed to be fast!
	int CountChildren() const;
	// returns true if the job is waiting for other jobs to finish
	bool IsWaiting() const { return (_parent != NO_ID) && ! _parents_done; };
	// remove this parent from the waiting collection, and ! IsWaiting
	bool ParentComplete(Node* parent);
	// visit child nodes calling the given function for each
	int VisitChildren(Dag& dag, int(*fn)(Dag& dag, Node* me, Node* child, void* args), void* args);
	// notify children of parent completion, and call the optional callback for each child that is no longer waiting
	int NotifyChildren(Dag& dag, bool(*fn)(Dag& dag, Node* child));
	// Recursively set all descendant nodes to status FUTILE & return the number of nodes set to status
	int SetDescendantsToFutile(Dag& dag);

	// append parent node names into the given buffer using the given printf format string
	int PrintParents(std::string & buf, size_t bufmax, const Dag* dag, const char * fmt) const;
	// append child node names into the given buffer using the given printf format string
	int PrintChildren(std::string & buf, size_t bufmax, const Dag* dag, const char * fmt) const;

	// called after the DAG has been parsed to build the parent and waiting edge lists
	void BeginAdjustEdges(Dag* dag);
	void AdjustEdges(Dag* dag);
	void FinalizeAdjustEdges(Dag* dag);

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
	bool TerminateSuccess();
	// Node is done: Failure
	bool TerminateFailure();
	// Return whter or not node is active (running pre script, job(s), or post script)
	bool IsActive() const;

	// Get current node status
	inline status_t GetStatus() const { return _Status; }
	// Get current node status string
	const char* GetStatusName() const;
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
	bool HasAbortReturnValue() const { return abort_dag_return_val != INT_MAX; } // Has abort return code
	int GetAbortReturnValue() const { return abort_dag_return_val; } // Get abort return code

	// Check if node should be retried
	bool DoRetry() const { return ! DoAbort() && GetRetries() < GetRetryMax(); }
	// Abort doing retry of node -> faliure
	bool AbortRetry() const { return have_retry_abort_val && retval == retry_abort_val; }
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
	bool Release(int proc);
	// Current current number of tracked job procs
	inline int GetProcEventsSize() const { return (int)_gotEvents.size(); }
	// Get job event mask for specified job proc
	inline unsigned char GetProcEvent(int proc) const { return _gotEvents[proc]; }
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
	// Increment the count of job procs that got abort event
	void IncrementJobsAborted() { numJobsAborted++; }
	// Get number of job procs that got abort event
	int JobsAborted() const { return numJobsAborted; }
	// Count each job proc exit code
	void CountJobExitCode(int code) {
		if (exitCodeCounts.contains(code)) {
			exitCodeCounts[code]++;
		} else {
			exitCodeCounts[code] = 1;
		}
	}
	// Get map of job proc exit codes count
	const std::map<int, int>& JobExitCodes() const { return exitCodeCounts; }

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

	inline const char* GetNodeName() const { return _nodeName; }
	inline const char* GetDirectory() const { return _directory; }
	inline const char* GetCmdFile() const { return _cmdFile; }
	void SetDagFile(const char *dagFile);
	const char* GetDagFile() const { return _dagFile; }
	// Prefix node name with splice scope
	void PrefixName(const std::string &prefix);
	// Prefix directory path with splice path for relative directory
	void PrefixDirectory(std::string &prefix);

	bool AddScript(Script* script);
	const char* GetPreScriptName() const;
	const char* GetPostScriptName() const;
	const char* GetHoldScriptName() const;

	bool AddPreSkip(int exitCode, std::string &whynot);
	bool HasPreSkip() const { return _preskip != PRE_SKIP_INVALID; }
	int GetPreSkip() const;

	void SetNoop( bool value ) { _noop = value; }
	bool GetNoop() const { return _noop; }

	void SetHold(bool value) { _hold = value; }
	bool GetHold() const { return _hold; }

	void SetSaveFile(const std::string& saveFile) { _saveFile = saveFile; _isSavePoint = true; }
	inline std::string GetSaveFile() const { return _saveFile; }
	inline bool IsSavePoint() const { return _isSavePoint; }

	struct NodeVar {
		const char * _name; // stringspace string, not owned by this struct
		const char * _value; // stringspace string, not owned by this struct
		bool _prepend; //bool to determine if variable is prepended or appended
		NodeVar(const char * n, const char * v, bool p) : _name(n), _value(v), _prepend(p) {}
	};

	std::forward_list<NodeVar> GetVars() const { return varsFromDag; }
	bool AddVar(const char * name, const char * value, const char* filename, int lineno, bool prepend);
	bool HasVars() const { return ! varsFromDag.empty(); }
	int PrintVars(std::string &vars);

	void SetPrio(int prio) { _explicitPriority = _effectivePriority = prio; }
	void AddDagPrio(int prio) { _effectivePriority += prio; }
	void setSubPrio(int prio) { subPriority = prio;}
	int GetExplicitPrio() const { return _explicitPriority; }
	int GetEffectivePrio() const { return _effectivePriority; }
	int GetSubPrio() const { return subPriority; }

	bool HasInlineDesc() const { return !inline_desc.empty(); }
	void SetInlineDesc(std::string& desc) { inline_desc = desc; }
	std::string_view& GetInlineDesc() { return inline_desc; }

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
	void SetLastEventTime(const ULogEvent *event);
	// Get the time at which the most recent event occurred for the job(s)
	time_t GetLastEventTime() const { return _lastEventTime; }

	bool WasVisited() const { return _visited; }
	void MarkVisited() { _visited = true; }
	void SetDfsOrder(int order) { _dfsOrder = order; }
	int GetDfsOrder() const { return _dfsOrder; }

	// Set specific node error message
	void SetErrorMsg(const char* fmt, ...) {
		va_list args;
		va_start(args, fmt);
		formatstr(error_text, fmt, args);
		va_end(args);
	}
	// Append to node specific error message
	void AppendErrorMsg(const char* fmt, ...) {
		va_list args;
		va_start(args, fmt);
		formatstr_cat(error_text, fmt, args);
		va_end(args);
	}
	// Return node specific error message
	const char* GetErrorMsg() const { return error_text.c_str(); }

	// Reset internal variables upon retry
	void ResetInfo() {
		numJobsSubmitted = 0;
		numJobsAborted = 0;
		isSuccessful = true;
		readFirstProc = false;
		is_factory = false;
		exitCodeCounts.clear();
		error_text.clear();
	}

	Script* _scriptPre{nullptr};
	Script* _scriptPost{nullptr};
	Script* _scriptHold{nullptr};

	static const char * dedup_str(const char* str) { return stringSpace.strdup_dedup(str); }
	static const char* JobTypeString() { return "HTCondor"; }
	void WriteRetriesToRescue(FILE *fp, bool reset_retries);

	size_t NodeVarSize() const { return sizeof(std::forward_list<NodeVar>); }

private:
	// private methods for use by AdjustEdges
	void AdjustEdges_AddParentToChild(Dag* dag, NodeID_t child_id, Node* parent);
	// Print the list of which procs are idle/not idle for this node
	void PrintProcIsIdle();
	// Set last state change time
	static void SetStateChangeTime() { time(&lastStateChangeTime); }


	static StringSpace stringSpace; // Shared strings accross nodes to prevent memory bloating
	static NodeID_t _nodeID_counter; // Counter to give nodes unique ID's
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
	std::map<int, int> exitCodeCounts{}; // Exit Code : Number of jobs that returned code
	std::vector<unsigned char> _gotEvents{}; // Job event mask for each tracked job proc
	std::forward_list<NodeVar> varsFromDag{}; // Variables add to job list at submit time

	const char* _directory{nullptr}; // Node directory to run within... Do not malloc or free!
	const char* _cmdFile{nullptr}; // Job list submit file... Do not malloc or free!
	char* _dagFile{nullptr}; // SubDAG filename
	char* _nodeName{nullptr}; // Unique node name provided by user
	char* _jobTag{nullptr}; // Nodes job tag for the jobstate log ("-" if not specified)

	std::string _saveFile{}; // Filename to write save point rescue file
	std::string error_text{}; // Node error message
	std::string_view inline_desc{}; // DAG file inline submit description to use at submit time

	time_t _lastEventTime{0}; // The time of the most recent event related to this job.

	NodeType _type{NodeType::JOB}; // Node type (job, final, provisioner...)
	status_t _Status{STATUS_READY}; // Current node status

	NodeID_t _nodeID{-1}; // This node's unique node ID
	NodeID_t _parent{NO_ID}; // Either the single parent nodes ID or associated edge ID
	NodeID_t _child{NO_ID}; // Either the single child nodes ID or associated edge ID

	int _numparents{0}; // Count of parent nodes (counted as child edges are added)

	int retry_max{0}; // Maximum number of retry attempts
	int retries{0}; // Current number of retries
	int retry_abort_val{INT_MAX}; // Return code that short circuts doing retry

	int _submitTries{0}; // Number of attempts to place job list to AP
	int numJobsSubmitted{0}; // Number of submitted jobs
	int _queuedNodeJobProcs{0}; // Number of job procs currently tracked in queue
	int _jobProcsOnHold{0}; // Number of tracked job procs currently in hold state
	int numJobsAborted{0}; // Number of jobs with abort events
	int _timesHeld{0}; // Total number of times jobs in the job list went on hold

	int retval{-1}; // Nodes return code

	int _preskip{PRE_SKIP_INVALID}; // Nodes preskip code (to skip rest of node)

	int abort_dag_val{-1}; // Return code to notify DAG to abort
	int abort_dag_return_val{INT_MAX}; // Specific value to exit with upon abort

	int _jobstateSeqNum{0}; // This nodes job state sequence number

	int _dfsOrder{-1}; // DFS ordering of the node

	int _explicitPriority{0}; // User specified node priority (higher is better)
	int _effectivePriority{0}; // Actual node priority (modified by DAG priority)
	int subPriority{0}; // Internal priority for sorting same effective prio nodes based on recent failure

	bool have_retry_abort_val{false}; // Indicate node has retry abort condition value
	bool have_abort_dag_val{false}; // Indicate node has an abort condition value
	bool is_factory{false}; // Indicate placed job list is late materialization factory
	bool _visited{false}; // Node has been visited by DFS order

	bool countedAsDone{false}; // Indicate whether DAG has already terminated this node
	bool isSuccessful{true}; // Is Node currently successful or not
	bool readFirstProc{false}; // DAG has already read a job event (submit) for this node
	bool missingJobs{false}; // Node counts found missing jobs during verifying state with AP

	bool _multiple_parents{false}; // true when _parent is a EdgeID rather than a NodeID
	bool _multiple_children{false}; // true when _child is an EdgeID rather than a NodeID
	bool _parents_done{false}; // set to true when all of the parents of this node are done

	bool _preDone{false}; // true when user defines node as done in *.dag file
	bool _isSavePoint{false}; // Indicates that this node is going to write a save point file.
	bool _noop{false}; // Indicate this is a noop node (job list is not placed to AP)
	bool _hold{false}; // Indicate this node should place jobs on hold
};

struct SortNodesById
{
	bool operator ()(const Node* a, const Node* b) {
		return a->GetNodeID() < b->GetNodeID();
	}
	//bool operator ()(const Node & a, const Node & b) { return a.GetNodeID() < b.GetNodeID(); }
};

struct EqualNodesById
{
	bool operator ()(const Node* a, const Node* b) {
		return a->GetNodeID() == b->GetNodeID();
	}
	// bool operator ()(const Node & a, const Node & b) { return a.GetNodeID() == b.GetNodeID(); }
};


// This class holds multiple NodeId entries in a sorted vector, use it to hold
// either the parent or child list for a Node.
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
	Edge(std::vector<NodeID_t> & in) : _ary(in) {};
public:
	virtual ~Edge() {};

	std::vector<NodeID_t> _ary; // sorted array  of jobid's, either parent or child edge list
	//std::set<NodeID_t> _check; // used to double check the correctness of the edge list.
	bool Add(NodeID_t id) {
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
	// then it is a NodeID and an Edge needs to be allocated. 
	// 
	// On exit, multi will be true.  id will be the Id of the Edge (possibly newly created)
	// and first_id will be the former value of id IFF it was a NodeID and not an EdgeID.
	// in most cases the caller will want to insert first_id into the Edge if it is not NO_ID
	//
	static Edge* PromoteToMultiple(NodeID_t & id, bool & multi, NodeID_t & first_id) {
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

	bool MarkDone(NodeID_t id, bool & already_done) {
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


#endif /* ifndef NODE_H */
