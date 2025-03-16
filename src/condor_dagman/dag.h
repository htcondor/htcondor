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


#ifndef DAG_H
#define DAG_H

#include "condor_common.h"
#include "node.h"
#include "scriptQ.h"
#include "condor_daemon_core.h"
#include "read_multiple_logs.h"
#include "check_events.h"
#include "condor_id.h"
#include "throttle_by_category.h"
#include "../condor_utils/dagman_utils.h"
#include "jobstate_log.h"
#include "dagman_classad.h"
#include "dag_priority_q.h"
#include <filesystem>

#include <queue>

// Which layer of splices do we want to lift?
enum SpliceLayer {
	SELF,
	DESCENDENTS,
};


// non-exe failure codes for return value integers -- we
// represent DAGMan, batch-system, or other external errors
// with the integer return-code space *below* -64.
//   0:255 are normal exe return codes
//   -64:-1 represent catching exe signals 1 to 64
//   -1000 and below represent DAGMan, batch-system, or other external errors
const int DAG_ERROR_CONDOR_SUBMIT_FAILED = -1001;
const int DAG_ERROR_CONDOR_JOB_ABORTED = -1002;
const int DAG_ERROR_LOG_MONITOR_ERROR = -1003;
const int DAG_ERROR_JOB_SKIPPED = -1004;


//NOTE: this must be kept in sync with the DagStatus enum
static const char * DAG_STATUS_NAMES[] = {
	"DAG_STATUS_OK",
	"DAG_STATUS_ERROR",
	"DAG_STATUS_NODE_FAILED",
	"DAG_STATUS_ABORT",
	"DAG_STATUS_RM",
	"DAG_STATUS_CYCLE",
	"DAG_STATUS_HALTED"
};

namespace DAG {
	extern const char *ALL_NODES;
}

class Dagman;
class DagmanMetrics;
class CondorID;
class DagmanConfig;

// used for RelinquishNodeOwnership and AssumeOwnershipofNodes
// This class owns the containers with which it was constructed, but
// not the memory in those containers. Even though there is one thing
// in here, the design of the overall codebase say that eventually more
// things will probably end up in this class when lifting a splice into
// a container dag.
class OwnedMaterials
{
	public:
		// this structure owns the containers passed to it, but not the memory 
		// contained in the containers...
		OwnedMaterials(std::vector<Node*> *a, ThrottleByCategory *tr,
				bool reject, std::string firstRejectLoc ) :
				nodes (a), throttles (tr), _reject(reject),
				_firstRejectLoc(firstRejectLoc) {};
		~OwnedMaterials() 
		{
			delete nodes;
		};

	std::vector<Node*> *nodes;
	ThrottleByCategory *throttles;
	bool _reject;
	std::string _firstRejectLoc;
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
friend class DagmanMetrics;
friend class DagmanMetricsV1;
friend class DagmanMetricsV2;

public:
	// DAG constructor: Take reference to DAGMan class object, if splice, and splice scope
	Dag(const Dagman& dm, bool isSplice = false, const std::string &spliceScope = "root");
	~Dag();

	int PreScriptReaper(Node *node, int status);
	int PostScriptReaper(Node *node, int status);
	int HoldScriptReaper(Node *node);

	// Add a node to be managed by this DAG
	bool Add(Node& node);
	void PrefixAllNodeNames(const std::string &prefix);
	// Defer setting node status to DONE when parsed from node line in file
	void AddPreDoneNode(Node* node) { m_userDefinedDoneNodes.push_back(node); }
	void SetPreDoneNodes(); // Mark pre-set done nodes as such
	void SetReject(const std::string &location); // Mark a DAG as rejected
	bool GetReject(std::string &firstLocation); // Check if DAG was rejected
	// Set the DAGs working directory from DIR subcommand
	void SetDirectory(std::string &dir) { m_directory = dir; }
	// Set nodes effective priotities
	void SetNodePriorities();

	// Remove duplicate edges between nodes
	void AdjustEdges();
	// Prepare DAG for initial run. ONLY CALL FUNCTION ONCE!
	bool Bootstrap(bool recovery);
	// Check to see if there is a cycle in the DAG
	bool isCycle();
	// Verify Categories contain nodes and throttling information
	void CheckThrottleCats();

	int SubmitReadyNodes(const Dagman &dm); // Submit node jobs in the ready queue
	bool StartFinalNode(); // Start the Final node
	bool StartServiceNodes(); // Start service nodes
	bool RemoveServiceNodes(); // Remove running service nodes
	bool StartProvisionerNode(); // Start the provisioner node
	int GetProvisionerJobAdState(); // Get the provisioner state from the provisioner nodes job Ad

	void RunWaitingScripts(); // Run waiting scripts
	void RemoveRunningScripts() const; // Remove running scripts
	// Remove all running jobs associated with a node from the Schedd Queue
	void RemoveBatchJob(Node *node, const std::string& reason = "Removed by DAGMan");

	void EnforceNewJobsLimit(); // Enforce new MaxJobs limit newly qedit value to DAGMan Job Ad

	// Get the current status of the condor log file
	ReadUserLog::FileStatus GetCondorLogStatus(time_t checkQInterval);
	// Process all new events in the shared node log file
	bool ProcessLogEvents(bool recovery = false);
	// Get the current DAG status string
	const char *GetStatusName() const { return DAG_STATUS_NAMES[_dagStatus]; }

	// Functions to find Nodes
	Node* FindNodeByNodeID(const NodeID_t nodeID) const;
	Node* FindNodeByName(const char * nodeName) const;
	Node* FindNodeByEventID(const CondorID condorID) const;
	Node* FindAllNodesByName(const char* nodeName, const char *finalSkipMsg, const char *file, int line) const;
	bool NodeExists(const char *nodeName) const; // Check if node with provided name exists in DAG

	inline bool Recovery() const { return _recovery; }
	bool IsHalted() const { return _dagIsHalted; }

	const CondorID* DAGManJobId(void) const { return _DAGManJobId; } // Get DAGMan job ID
	std::string DefaultNodeLog(void) { return _defaultNodeLog.string(); }

	inline bool HasFinalNode() const { return _final_node != nullptr; }
	inline bool FinalNodeRun() const { return _finalNodeRun; }
	inline bool FinalNodeFinished() const {
		return _final_node && _finalNodeRun &&
		       (_final_node->GetStatus() == Node::STATUS_DONE || _final_node->GetStatus() == Node::STATUS_ERROR);
	}

	inline bool HasProvisionerNode() const { return _provisioner_node != nullptr; }

	int NumNodes(bool includeFinal) const;
	int NumNodesDone(bool includeFinal) const;
	inline int NumNodesSubmitted() const { return _numNodesSubmitted; }
	inline int NumNodesFailed() const { return _numNodesFailed; }
	inline int NumNodesFutile() const { return _numNodesFutile; }
	inline int NumNodesReady() const { return _readyQ->size() - NumReadyServiceNodes(); }
	inline int NumNodesUnready(bool includeFinal) const {
		return (NumNodes(includeFinal) - (NumNodesDone(includeFinal) + PreRunNodeCount() + NumNodesSubmitted() +
		        PostRunNodeCount() + NumNodesReady() + NumNodesFailed() + NumNodesFutile()));
	}
	// Count of all nodes w/ submitted jobs regardless of type
	inline int TotalSubmittedNodes() const { return _numServiceNodesSubmitted + _numNodesSubmitted; }

	inline int NumPreScriptsRunning() const {
		ASSERT(_isSplice == false);
		return _preScriptQ->NumScriptsRunning();
	}

	inline int NumPostScriptsRunning() const {
		ASSERT(_isSplice == false);
		return _postScriptQ->NumScriptsRunning();
	}

	inline int NumScriptsRunning() const {
		ASSERT(_isSplice == false);
		return NumPreScriptsRunning() + NumPostScriptsRunning();
	}

	inline int NumReadyServiceNodes() const {
		auto IsReady = [](const Node* n) -> bool { return n->GetStatus() == Node::STATUS_READY; };
		return std::count_if(_service_nodes.begin(), _service_nodes.end(), IsReady);
	}

	inline int PreRunNodeCount() const { return _preRunNodeCount; }
	inline int PostRunNodeCount() const { return _postRunNodeCount; }
	inline int HoldRunNodeCount() const { return _holdRunNodeCount; }
	inline int ScriptRunNodeCount() const { return _preRunNodeCount + _postRunNodeCount; }

	inline int TotalJobsSubmitted() const { return _totalJobsSubmitted; }
	inline int TotalJobsSuccessful() const { return _totalJobsSuccessful; }
	inline int TotalJobsCompleted() const { return _totalJobsCompleted; }

	/** Count number of Job Procs throughout the entire DAG
		in states held, idle, running, and terminated.
		@param n_held: pointer to set number of held job processes
		@param n_idle: pointer to set number of idle job processes
		@param n_running: pointer to set number of 'running' job processes
		@param n_terminated: pointer to set number of terminated/aborted job processes
	
		Note: running job process = number of processes not idle, held, or terminated
	*/
	void NumJobProcStates(int* n_held=NULL, int* n_idle=NULL, int* n_running=NULL, int* n_terminated=NULL);
	int NumIdleJobProcs() const { return _numIdleJobProcs; }
	int NumHeldJobProcs();

	bool FinishedRunning(bool includeFinalNode) const; // Determine if DAG is finished running (successful or failed)
	bool DoneSuccess(bool includeFinalNode) const;
	bool DoneFailed(bool includeFinalNode) const;
	bool DoneCycle(bool includeFinalNode) const;

	void CheckAllJobs(); // Verify all job events good once DAG is finished

	void Rescue(const char * dagFile, bool multiDags, int maxRescueDagNum, bool overwrite,
	            bool parseFailed = false, bool isPartial = false);
	void WriteRescue(const char * rescue_file, const char * headerInfo, bool parseFailed = false,
	                 bool isPartial = false, bool isSavePoint = false);

	// Print various node information
	void PrintNodeList() const;
	void PrintNodeList(Node::status_t status) const;
	void PrintReadyQ(debug_level_t level) const;
	void PrintDeferrals(debug_level_t level, bool force) const;
	void PrintPendingNodes() const;

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// Splicing
	bool InsertSplice(std::string spliceName, Dag *splice_dag);
	Dag* LookupSplice(std::string name);

	std::vector<Node*>* InitialRecordedNodes(void);
	std::vector<Node*>* FinalRecordedNodes(void);
	void RecordInitialAndTerminalNodes(void);

	OwnedMaterials* LiftSplices(SpliceLayer layer); // Recursively lift all lower splices up to this DAG
	OwnedMaterials* RelinquishNodeOwnership(void);
	void AssumeOwnershipofNodes(const std::string &spliceName, OwnedMaterials *om);

	void PropagateDirectoryToAllNodes(void); // Prefix DAG dir to all relative path node DIRs
	bool SetPinInOut(bool isPinIn, const char *nodeName, int pinNum);
	static bool ConnectSplices(Dag *parentSplice, Dag *childSplice); // Connect two splices via out/in pins
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	// Dot file helper functions
	char* GetDotFileName(void) { return _dot_file_name; }
	void SetDotFileName(const char *dot_file_name);
	void SetDotIncludeFileName(const char *include_file_name);
	void SetDotFileUpdate(bool update_dot_file) { _update_dot_file = update_dot_file; }
	void SetDotFileOverwrite(bool overwrite_dot_file) { _overwrite_dot_file = overwrite_dot_file; }
	bool GetDotFileUpdate(void) const { return _update_dot_file; }
	void DumpDotFile(void);

	// Node Status file helper functions
	void SetNodeStatusFileName(const char *statusFileName, int minUpdateTime, bool alwaysUpdate = false);
	void DumpNodeStatus(bool held, bool removed);

	// JobState log helper functions
	void SetJobstateLogFileName(const char *logFileName);
	JobstateLog &GetJobstateLog() { return _jobstateLog; }

	const DagmanOptions &dagOpts; // DAGMan command line options
	const DagmanConfig &config; // DAGMan configuration values
	std::map<std::string, std::string> InlineDescriptions{}; // Internal job submit descriptions
	ThrottleByCategory _catThrottles;

	DagStatus _dagStatus{DAG_STATUS_OK};
	const int MAX_SIGNAL{64}; // Maximum signal number we can deal with in error handling

protected:
	mutable std::vector<Node*> _nodes; // List of all 'normal' and SubDAG nodes
	std::vector<Node*> _service_nodes{}; // List of Service nodes
private:
	typedef enum {
		SUBMIT_RESULT_OK,
		SUBMIT_RESULT_FAILED,
		SUBMIT_RESULT_NO_SUBMIT,
	} submit_result_t;

	using PinNodes = std::vector<Node *>;
	using PinList = std::vector<PinNodes *>;

	// Verify schedd q contains expected jobs
	void VerifyJobsInQueue(const std::uintmax_t before_size);
	// Process event read from shared nodes log
	bool ProcessOneEvent(ULogEventOutcome outcome, const ULogEvent *event, bool recovery, bool &done);
	bool EventSanityCheck(const ULogEvent* event, const Node* node, bool* result);
	bool SanityCheckSubmitEvent(const CondorID condorID, const Node* node) const;
	void ProcessAbortEvent(const ULogEvent *event, Node *node, bool recovery); // Job abort or executable error
	void ProcessTerminatedEvent(const ULogEvent *event, Node *node, bool recovery); // Job terminated (success or fail)
	void ProcessJobProcEnd(Node *node, bool recovery, bool failed); // Common Work for all jobs that have left queue
	void ProcessPostTermEvent(const ULogEvent *event, Node *node, bool recovery); // POST Script Terminate
	void ProcessSubmitEvent(Node *node, bool recovery, bool &submitEventIsSane); // Job Submit event
	void ProcessIsIdleEvent(Node *node, int proc); // Job is not actively running
	void ProcessNotIdleEvent(Node *node, int proc); // Job is actively running
	void ProcessHeldEvent(Node *node, const ULogEvent *event); // Job held
	void ProcessReleasedEvent(Node *node, const ULogEvent *event); // Job released
	void ProcessClusterSubmitEvent(Node *node); // Cluster submit (late materialization)
	void ProcessClusterRemoveEvent(Node *node, bool recovery); // Cluster removed (late materialization)
	void ProcessSuccessfulSubmit(Node *node, const CondorID &condorID); // Post process of successful submit of node jobs
	void ProcessFailedSubmit(Node *node, int max_submit_attempts); // Post process of failed submit of node jobs

	void DecrementProcCount(Node *node);
	void UpdateNodeCounts(Node *node, int change);

	bool StartNode(Node *node, bool isRetry); // Begin executing node (PRE Script -> ready queue -> POST Script)
	void RestartNode(Node *node, bool recovery); // Restart a failed node w/ retries
	submit_result_t SubmitNodeJob(const Dagman &dm, Node *node, CondorID &condorID); // Submit a nodes job to Schedd queue
	void TerminateNode(Node* node, bool recovery, bool bootstrap = false); // Final actions once node is completed successfully

	bool RunPostScript(Node *node, bool ignore_status, int status, bool incrementRunCount = true);
	bool RunHoldScript(Node *node, bool incrementRunCount = true);

	bool CheckForDagAbort(Node *node, const char *type); // Check ABORT-DAG-ON value for node

	// Check if node in a no-operation (NOOP) node
	static bool NodeIsNoop(const CondorID &id) {
		return id._cluster == 0 && id._proc == Node::NOOP_NODE_PROCID;
	}
	// Get Part of HTCondor ID for hash indexing
	// (ClusterId for normal nodes & SubProcID for NOOP nodes)
	static int GetIndexID(const CondorID &id) {
		return NodeIsNoop(id) ? id._subproc : id._cluster; 
	}
	std::map<int, Node*>* GetEventIDHash(bool isNoop);
	const std::map<int, Node*>* GetEventIDHash(bool isNoop) const;
	Node* LogEventNodeLookup(const ULogEvent* event, bool &submitEventIsSane);

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// Splicing functions
	void LiftSplice(Dag *parent, Dag *splice);
	int GetPinCount(bool isPinIn);
	bool SetPinInOut(PinList &pinList, Node *node, int pinNum);
	const PinNodes *GetPinInOut(bool isPinIn, int pinNum) const;
	const static PinNodes *GetPinInOut(const PinList &pinList, const char *inOutStr, int pinNum);
	static void DeletePinList(PinList &pinList) {
		for (auto pn : pinList) { delete pn; }
	}
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	void WriteSavePoint(Node* node); // Write a node save point file
	void WriteNodeToRescue(FILE *fp, Node *node, bool reset_retries_upon_rescue, bool isPartial);
	static void WriteScriptToRescue( FILE *fp, Script *script );

	void PrintDagFiles(const std::list<std::string> &dagFiles);
	void PrintEvent(debug_level_t level, const ULogEvent* event, Node* node, bool recovery);

	size_t TotalLogFileCount() { return CondorLogFileCount(); }
	size_t CondorLogFileCount() { return _condorLogRdr.totalLogFileCount(); }
	bool MonitorLogFile();
	bool UnmonitorLogFile();

	void DFSVisit(Node * node, int depth); // DFS visit and number nodes to detect cycle

	const char *EscapeClassadString(const char* strIn);

	// Private DOT file helper functions
	void IncludeExtraDotCommands(FILE *dot_file);
	void DumpDotFileNodes(FILE *temp_dot_file);
	void DumpDotFileArcs(FILE *temp_dot_file);
	void ChooseDotFileName(std::string &dot_file_name);


	static const CondorID _defaultCondorId; // Default HTCondorID used for resetting

	ReadMultipleUserLogs _condorLogRdr{};
	CheckEvents _checkCondorEvents{};
	JobstateLog _jobstateLog{}; // Pegasus JobState log
	std::filesystem::path _defaultNodeLog{}; // Shared nodes job log (heart of DAGMan)
	mutable std::vector<Node*>::iterator _allNodesIt; // ALL_NODES iterator
	std::vector<int> _graph_widths{};

	std::map<std::string, Node*> _nodeNameHash{};
	std::map<NodeID_t, Node*> _nodeIDHash{};
	std::map<int, Node*> _condorIDHash{};
	std::map<int, Node*> _noopIDHash{};

	std::map<std::string, Dag*> _splices{};
	std::vector<Node*> _splice_initial_nodes{}; // All nodes with no parents
	std::vector<Node*> _splice_terminal_nodes{}; // All node with no children
	PinList _pinIns{};
	PinList _pinOuts{};

	std::deque<Node*> m_userDefinedDoneNodes{}; // List of nodes declared with DONE: JOB FOO submit DONE

	DCSchedd* _schedd;
	DagPriorityQ* _readyQ{nullptr};
	std::queue<Node*>* _submitQ{nullptr}; // Queue of nodes waiting to see submit event for

	ScriptQ* _preScriptQ{nullptr};
	ScriptQ* _postScriptQ{nullptr};
	ScriptQ* _holdScriptQ{nullptr};

	DagmanMetrics* _metrics{nullptr};
	ProvisionerClassad* _provisionerClassad{nullptr}; // Provisioner node ClassAd functionality
	// NOTE: Provisioner and Final nodes exist in _jobs vector.
	//       These pointers are for quick access
	Node* _provisioner_node{nullptr}; // Pointer to provisioner node
	Node* _final_node{nullptr}; // Pointer to final node.

	const CondorID* _DAGManJobId; // HTCondor ID of this DAGMan job

	std::string m_directory{"."};
	std::string _editPolicy{}; // Policy of how we respond to DAG edits
	std::string _spliceScope; // Current splice scope (i.e. 'A+B+' or 'root')
	std::string _firstRejectLoc{}; // File and Line number of the first REJECT keyword
	std::string _haltFile{}; // Name of the halt file

	char* _statusFileName{nullptr}; // Node status filename

	time_t _lastStatusUpdateTimestamp{0}; // Last time the node status file was written
	time_t _nextSubmitTime{0}; // The next time we are able to submit jobs (0 means go ahead)
	time_t _lastEventTime{0}; // Last time we read an event from the shared nodes log
	time_t _lastPendingNodePrintTime{0}; // Last time pending nodes report was printed
	time_t _validatedStateTime{0}; // Time stamp of last time DAG state was validated when pending on nodes
	time_t queryFailTime{0}; // Last time we failed to query the Schedd Queue

	int _numNodesDone{0}; // Number of nodes that have completed execution
	int _numNodesFailed{0}; // Number of nodes that have failed (list of jobs/PRE/POST failed)
	int _numNodesFutile{0}; // Number of nodes that can't run due to ancestor failing
	int _numNodesSubmitted{0}; // Number of batch system jobs currently submitted
	int _numServiceNodesSubmitted{0}; // Number of service nodes with jobs submitted in the queue

	int _totalJobsSubmitted{0}; // Total number of batch system jobs submitted
	int _totalJobsCompleted{0}; // Total number of batch system jobs that exited AP
	int _totalJobsSuccessful{0}; // Total number of batch system jobs that exited AP w/ success
	int _numIdleJobProcs{0}; // Number of DAG managed jobs currently idle

	int _preRunNodeCount{0}; // Number of nodes currently running PRE Scripts (STATUS_PRERUN)
	int _postRunNodeCount{0}; // Number of nodes currently running POST Scripts (STATUS_POSTRUN)
	int _holdRunNodeCount{0}; // Number of nodes currently running HOLD Scripts (no special status)

	int _maxJobsDeferredCount{0}; // Number of deferred nodes due to MaxJobs Limit
	int _maxIdleDeferredCount{0}; // Number of deferred nodes due to MaxIdle Limit
	int _catThrottleDeferredCount{0}; // Number of deferred nodes due to category throttling

	int DFS_ORDER{0};
	int _graph_width{0};
	int _graph_height{0};

	int _minStatusUpdateTime{0}; // Minimum time between updates for node status file
	int _nextSubmitDelay{1}; // Delay before attempting to submit a job again upon failed job submission
	int _recoveryMaxfakeID{0}; // Max SubProcID we see in recovery to prevent collision when writing new fake events

	bool _finalNodeRun{false};
	bool _provisioner_ready{false};
	bool _statusFileOutdated{true}; // Need to update node status file
	bool _alwaysUpdateStatus{false}; // Update node status file even if nothing has changed
	bool _recovery{false}; // DAGMan is in recovery mode (needed for POST script reaper)
	bool _validatedState{false}; // DAG is in a validated state (All pending node jobs exist in Schedd Queue)
	bool _isSplice{false}; // Specify whether DAG is a splice or not
	bool _reject{false}; // Reject this DAG
	bool _dagIsHalted{false}; // DAG is currently halted
	bool _dagIsAborted{false}; // DAG has been aborted. Needed because ABORT-DAG-ON w/ return 0 is DAG_STATUS_OK

	// Information for producing dot files, which can be used to visualize
	// DAG files. Dot is part of the graphviz package, which is available from
	// http://www.research.att.com/sw/tools/graphviz/
	char* _dot_file_name{nullptr};
	char* _dot_include_file_name{nullptr};
	bool  _update_dot_file{false};
	bool  _overwrite_dot_file{true};
	int   _dot_file_name_suffix{0};

};

#endif /* #ifndef DAG_H */
