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

// Terminology note:
// We are calling a *node* the combination of pre-script, job, and
// post-script.
// We are calling a *job* essentially what results from one invocation
// of condor_submit.
// We are calling a *job proc* an individual batch system process within
// a cluster.
// So nodes-to-jobs is 1-to-1; jobs to job procs is 1-to-n.
// wenger 2006-02-14.

// Local DAGMan includes
#include "condor_common.h"
#include "condor_attributes.h"
#include "dag.h"
#include "debug.h"
#include "dagman_submit.h"
#include "dagman_main.h"
#include "write_user_log.h"
#include "condor_daemon_core.h"
#include "dagman_metrics.h"
#include "enum_utils.h"
#include "tmp_dir.h"
#include "condor_q.h"

namespace deep = DagmanDeepOptions;
namespace shallow = DagmanShallowOptions;
// {ClusterId : {ProcId,ProcId...}}
using QueriedJobs = std::map<int, std::set<int>>;

const CondorID Dag::_defaultCondorId;

namespace DAG {
	const char *ALL_NODES = "ALL_NODES";
}

//---------------------------------------------------------------------------
Dag::Dag(const Dagman& dm, bool isSplice, const std::string &spliceScope) :
	dagOpts                (dm.options),
	_schedd                (dm._schedd),
	_DAGManJobId           (&dm.DAGManJobId),
	_spliceScope           (spliceScope),
	_condorRmExe           (dm.condorRmExe.c_str()),
	_maxJobHolds           (dm._maxJobHolds),
	m_retrySubmitFirst     (dm.retrySubmitFirst),
	m_retryNodeFirst       (dm.retryNodeFirst),
	_submitDepthFirst      (dm.submitDepthFirst),
	_abortOnScarySubmit    (dm.abortOnScarySubmit),
	_generateSubdagSubmits (dm._generateSubdagSubmits),
	_isSplice              (isSplice)
{
	debug_printf(DEBUG_DEBUG_1, "Dag(%s)::Dag()\n", _spliceScope.c_str());

	_defaultNodeLog.assign(dm._defaultNodeLog);
	_checkCondorEvents.SetAllowEvents(dm.allow_events);

	_haltFile = _dagmanUtils.HaltFileName(dm.options.primaryDag());

	const std::string &dagConfig = dm._dagmanConfigFile;
	_configFile = dagConfig.empty() ? nullptr : dagConfig.c_str();

	// for the toplevel dag, emit the dag files we ended up using.
	if ( ! _isSplice) {
		ASSERT(dm.options.numDagFiles() >= 1);
		PrintDagFiles(dm.options.dagFiles());
	}

	_readyQ = new DagPriorityQ;
	_submitQ = new std::queue<Node*>;
	if (!_readyQ || !_submitQ) {
		EXCEPT("ERROR: out of memory (%s:%d)!", __FILE__, __LINE__);
	}

	/* The ScriptQ object allocates daemoncore reapers, which are a
		regulated and precious resource. Since we *know* we will never need
		this object when we are parsing a splice, we don't allocate it.
		In the other codes that expect this pointer to be valid, there is
		either an ASSERT or other checks to ensure it is valid. */
	if (_isSplice == false) {
		_preScriptQ = new ScriptQ(this);
		_postScriptQ = new ScriptQ(this);
		_holdScriptQ = new ScriptQ(this);
		if (!_preScriptQ || !_postScriptQ) {
			EXCEPT("ERROR: out of memory (%s:%d)!", __FILE__, __LINE__);
		}
	}

	debug_printf(DEBUG_DEBUG_4, "MaxJobsSubmitted = %d, MaxPreScripts = %d, MaxPostScripts = %d\n",
	             dagOpts[shallow::i::MaxJobs], dagOpts[shallow::i::MaxPre], dagOpts[shallow::i::MaxPost]);
}

//-------------------------------------------------------------------------
Dag::~Dag()
{
	debug_printf(DEBUG_DEBUG_1, "Dag(%s)::~Dag()\n", _spliceScope.c_str());

	if (_condorLogRdr.activeLogFileCount() > 0) {
		(void) UnmonitorLogFile();
	}

	// Delete all node objects in _nodes
	for (auto *node : _nodes) {
		delete node;
	}

	for (auto &[_, splice] : _splices) {
		delete splice;
	}

	// And remove them from the vector
	_nodes.clear();

	delete _preScriptQ;
	delete _postScriptQ;
	delete _holdScriptQ;
	delete _submitQ;
	delete _readyQ;

	free(_dot_file_name);
	free(_dot_include_file_name);

	free(_statusFileName);

	delete _metrics;

	DeletePinList(_pinIns);
	DeletePinList(_pinOuts);

	delete _provisionerClassad;
	_provisionerClassad = nullptr;

}

//-------------------------------------------------------------------------
void
Dag::RunWaitingScripts()
{
	_preScriptQ->RunWaitingScripts();
	_postScriptQ->RunWaitingScripts();
	_holdScriptQ->RunWaitingScripts();
}

//-------------------------------------------------------------------------
void
Dag::CreateMetrics(const char *primaryDagFile, int rescueDagNum)
{
	_metrics = new DagmanMetrics(this, primaryDagFile, rescueDagNum);
}

//-------------------------------------------------------------------------
void
Dag::ReportMetrics(int exitCode)
{
	// For some failure modes this can get called before  we created the metrics object 
	if ( ! _metrics) { return; }
	bool report_graph_metrics = param_boolean("DAGMAN_REPORT_GRAPH_METRICS", false);
	if (report_graph_metrics) {
		if (_dagStatus != DagStatus::DAG_STATUS_CYCLE) {
			_metrics->GatherGraphMetrics(this);
		}
	}
	(void)_metrics->Report(exitCode, _dagStatus);
}

//-------------------------------------------------------------------------
bool Dag::Bootstrap(bool recovery)
{
	// This function should never be called on a dag object which is acting
	// like a splice.
	ASSERT(_isSplice == false);

	// update dependencies for pre-completed nodes (nodes marked DONE in
	// the DAG input file)
	for (auto & node : _nodes) {
		if (node->GetStatus() == Node::STATUS_DONE) {
			TerminateNode(node, false, true);
		}
	}
	int nodesDone = NumNodesDone(true);
	debug_printf(DEBUG_VERBOSE, "Number of pre-completed nodes: %d\n", nodesDone);

	//User defined DONE nodes may result in children being 'done' before parent nodes
	//Check for this use case and write a warning if found
	int count = 0;
	for (auto &node : _nodes) {
		bool done = node->GetStatus() == Node::STATUS_DONE;
		if (done) { count++; }
		if (node->IsWaiting() && done && !node->NoChildren()) {
			debug_printf(DEBUG_VERBOSE, "Warning: Node %s was marked as done even though parent nodes aren't complete."
			                            " Child nodes may run out of order.\n", node->GetNodeName());
		}
		if (count >= nodesDone) { break; }
	}

	_recovery = recovery;

	(void) MonitorLogFile();

	if (recovery) {
		debug_printf(DEBUG_NORMAL, "Running in RECOVERY mode... >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
		_jobstateLog.WriteRecoveryStarted();
		_jobstateLog.InitializeRecovery();

		// as we read the event log files, we emit lots of imformation into
		// the logfile. If this is on NFS, then we pay a *very* large price
		// for the open/close of each line in the log file.
		// Whether or not this caching is honored is dependant on if the
		// cache was originally enabled or not. If the cache is not
		// enabled, then debug_cache_start_caching() and 
		// debug_cache_stop_caching() are effectively noops.

		debug_cache_start_caching();

		auto log_size = std::filesystem::file_size(_defaultNodeLog);

		if (CondorLogFileCount() > 0) {
			if ( ! ProcessLogEvents(recovery)) {
				_recovery = false;
				debug_cache_stop_caching();
				_jobstateLog.WriteRecoveryFailure();
				return false;
			}
		}

		// Check node states for submitted and in post_run
		int numSubmitted = 0;
		for (auto &node : _nodes) {
			if (node->GetStatus() == Node::STATUS_SUBMITTED) { numSubmitted++; }
			else if (node->GetStatus() == Node::STATUS_POSTRUN) {
				if ( ! RunPostScript(node, dagOpts[shallow::b::PostRun], 0, false)) {
					debug_cache_stop_caching();
					_jobstateLog.WriteRecoveryFailure();
					return false;
				}
			}
		}

		set_fake_condorID(_recoveryMaxfakeID);

		debug_cache_stop_caching();

		_jobstateLog.WriteRecoveryFinished();
		debug_printf(DEBUG_NORMAL, "...done with RECOVERY mode <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");

		if (numSubmitted > 0) {
			debug_printf(DEBUG_NORMAL, "%d nodes claim to have jobs submitted to queue. Verifying...\n",
			             numSubmitted);
			VerifyJobsInQueue(log_size);
		}

		print_status();

		_recovery = false;
	}

	if (DEBUG_LEVEL(DEBUG_DEBUG_2)) {
		PrintNodeList();
		PrintReadyQ(DEBUG_DEBUG_2);
	}

		// If we have any service nodes, start those right away
	if ( ! _service_nodes.empty()) {
		if ( ! StartServiceNodes()) {
			debug_printf(DEBUG_QUIET, "Warning: unable to start service nodes. Proceeding with the DAG workflow.\n");
		}
	}

	// If we have a provisioner node, submit that before any other nodes
	if (HasProvisionerNode()) {
		StartProvisionerNode();
	} else {
		// Note: we're bypassing the ready queue here...
		for (auto & node : _nodes) {
			if(node->CanSubmit()) { StartNode(node, false); }
		}
	}

	return true;
}

//-------------------------------------------------------------------------
void Dag::SetPreDoneNodes() {
	while (m_userDefinedDoneNodes.begin() != m_userDefinedDoneNodes.end()) {
		Node* node = m_userDefinedDoneNodes.back();
		//Sanity check that Node pointer isn't NULL
		if (node) {
			//Set given node to STATUS_DONE and remove node pointer from vector
			node->SetStatus(Node::STATUS_DONE);
			node->SetPreDone();
		}
		else {
			//Print warning that somehow a NULL was set for a Node
			debug_printf(DEBUG_NORMAL, "Warning: NULL set for node marked as DONE in dag file.\n");
		}
		m_userDefinedDoneNodes.pop_back();
	}
}

//-------------------------------------------------------------------------
Node* Dag::FindNodeByNodeID(const NodeID_t nodeID) const {
	Node* node = nullptr;
	auto findResult = _nodeIDHash.find(nodeID);
	if (findResult == _nodeIDHash.end()) {
		debug_printf(DEBUG_NORMAL, "ERROR: node %d not found!\n", nodeID);
		dprintf(D_ALWAYS | D_BACKTRACE, "caller info");
	} else {
		node = (*findResult).second;
	}

	if (node) {
		if (nodeID != node->GetNodeID()) {
			EXCEPT("Searched for node ID %d; got %d!!", nodeID, node->GetNodeID());
		}
	}

	return node;
}

//-------------------------------------------------------------------------
// Helper function to process queried job ad and store clusterid
static bool StoreQueueInformation(void* pv, ClassAd* ad) {
	QueriedJobs *info = (QueriedJobs*)pv;
	int ClusterId = -1;
	int ProcId = -1;

	bool missing_attr = false;
	if ( ! ad->LookupInteger(ATTR_CLUSTER_ID, ClusterId)) {
		debug_printf(DEBUG_NORMAL, "ERROR: Queried Job Ad does not have ClusterId\n");
		missing_attr = true;
	}
	if ( ! ad->LookupInteger(ATTR_PROC_ID, ProcId)) {
		debug_printf(DEBUG_NORMAL, "ERROR: Queried Job Ad does not have ProcId\n");
		missing_attr = true;
	}

	// If missing an attribute don't store and return
	if (missing_attr) { return true; } // Don't take ownership of classad

	if (info->contains(ClusterId)) {
		(*info)[ClusterId].insert(ProcId);
	} else {
		info->insert({ClusterId, {ProcId}});
	}

	return true; // Don't take ownership of ClassAd
}

//-------------------------------------------------------------------------
/*
*	VerifyJobsInQueue() will query the local schedd job queue for all jobs
*	associated with this DAG and verify that all nodes in the submitted state
*	can find their recorded job (ClusterId.ProcId) in the returned ClassAds query.
*	If any submitted node jobs are missing from the queue then rescue and abort.
*/
void Dag::VerifyJobsInQueue(const std::uintmax_t before_size) {
	// If query failed then don't query again for a minimum of 10 minutes
	time_t now; time(&now);
	if (now - queryFailTime < 600) { return; }

	debug_printf(DEBUG_VERBOSE, "Querying local schedd for associated jobs in queue\n");
	// Setup variables for queue query
	CondorQ queue;
	const static std::vector<std::string> attrs{"ClusterId","ProcId"};
	CondorError errstack;
	QueriedJobs job_info;

	// Constraint to only DAGMan jobs and ClusterAds
	std::string dagID;
	formatstr(dagID, "DAGManJobId==%d", _DAGManJobId->_cluster);
	(void)queue.addAND(dagID.c_str());

	// Query the local schedd queue
	int ret = queue.fetchQueueFromHostAndProcess(_schedd->addr(), attrs, QueryFetchOpts::fetch_Jobs, /*MatchLimit*/ -1,
	                                             StoreQueueInformation, &job_info, 2, &errstack, nullptr);

	auto log_size = std::filesystem::file_size(_defaultNodeLog);

	// Check for query failure. If failure then write error and return
	if (ret != Q_OK) {
		switch(ret) {
			case Q_PARSE_ERROR:
			case Q_INVALID_CATEGORY:
			case Q_UNSUPPORTED_OPTION_ERROR:
				debug_printf(DEBUG_NORMAL, "ERROR(%d): Invalid Schedd query information.\n", ret);
				break;
			default:
				debug_printf(DEBUG_NORMAL, "ERROR(%d): Failed to fetch job ads from local Schedd: %s\n",
				             ret, errstack.getFullText().c_str());
		}
		queryFailTime = now;
		return;
	}

	bool possible_race = (log_size > before_size);
	bool abort_dag = false;
	bool valid_state = true;

	for (auto &node : _nodes) {
		if (node->GetStatus() == Node::STATUS_SUBMITTED) {
			int cluster = node->GetCluster();
			// No jobs (cluster) found in queue or particular job not found in queue
			if ( !job_info.contains(cluster) || !node->VerifyJobStates(job_info[cluster])) {
				valid_state = false;
				// Node has had issue already and a possible race is
				// detected then note issue and continue on
				if ( ! node->missingJobs && possible_race) {
					node->missingJobs = true;
				} else {
					abort_dag = true;
					debug_printf(DEBUG_NORMAL, "ERROR: DAGMan lost track of node %s (ClusterId=%d). Please resubmit DAG from produced rescue file.\n",
					             node->GetNodeName(), cluster);
				}
			// All expected jobs found in the queue
			} else if (node->missingJobs) { node->missingJobs = false; }
		}
	}

	// Lost track of entire cluster or a particular job so abort.
	if (abort_dag) { main_shutdown_rescue(EXIT_ERROR, DAG_STATUS_ERROR); }
	if ( ! valid_state) {
		_validatedState = false;
		return;
	}

	debug_printf(DEBUG_VERBOSE, "All pending job states validated\n");
	_validatedState = true;
	_validatedStateTime = _nodes[0]->GetLastStateChange();
}

//-------------------------------------------------------------------------
// Print nodes we are waiting on after a set amount of time has occurred since
// the last nodes log event and printing occurred. If printing has occurred for
// a long time check the queue for jobs.
ReadUserLog::FileStatus Dag::GetCondorLogStatus(time_t checkQInterval) {
	time_t currentTime;
	time(&currentTime);
	ReadUserLog::FileStatus status = _condorLogRdr.GetLogStatus();

	if (status == ReadUserLog::LOG_STATUS_GROWN) { _lastEventTime = currentTime; }
	auto log_size = std::filesystem::file_size(_defaultNodeLog);

	time_t elapsedEventTime = currentTime - _lastEventTime;
	time_t elapsedPrintTime = currentTime - _lastPendingNodePrintTime;

	if (elapsedEventTime >= (time_t)_pendingReportInterval &&
	    elapsedPrintTime >= (time_t)_pendingReportInterval)
	{
		_lastPendingNodePrintTime = currentTime;
		debug_printf(DEBUG_NORMAL, "%zu seconds since last log event\n", elapsedEventTime);
		PrintPendingNodes();

		if (checkQInterval > 0) {
			time_t elapsedCheckQTime = currentTime - _nodes[0]->GetLastStateChange();

			auto nodeIsSubmitted = [](const Node* n){ return n->GetStatus() == Node::STATUS_SUBMITTED; };
			bool hasSubmittedJobs = std::find_if(_nodes.begin(), _nodes.end(), nodeIsSubmitted) != _nodes.end();

			if (hasSubmittedJobs && !_validatedState && elapsedCheckQTime >= checkQInterval) {
				VerifyJobsInQueue(log_size);
			}
		}
	}

	return status;
}

//-------------------------------------------------------------------------
// Developer's Note: returning false tells main_timer to abort the DAG
bool Dag::ProcessLogEvents(bool recovery) {
	debug_printf(DEBUG_VERBOSE, "Currently monitoring %zu HTCondor log file(s)\n", _condorLogRdr.activeLogFileCount());

	bool done = false;  // Keep scanning until ULOG_NO_EVENT
	bool result = true;

	while ( ! done) {
		ULogEvent* e = nullptr;
		ULogEventOutcome outcome = ULOG_NO_EVENT;

		outcome = _condorLogRdr.readEvent(e);

		bool tmpResult = ProcessOneEvent(outcome, e, recovery, done);
		// If ProcessOneEvent returns false, the result here must be false.
		result = result && tmpResult;

		// event allocated earlier by _condorLogRdr.readEvent()
		if (e) { delete e; }
	}

	if (DEBUG_LEVEL(DEBUG_VERBOSE) && recovery) {
		debug_printf(DEBUG_NORMAL, "    ------------------------------\n");
		debug_printf(DEBUG_NORMAL, "       HTCondor Recovery Complete\n");
		debug_printf(DEBUG_NORMAL, "    ------------------------------\n");
	}

	// For performance reasons, we don't want to flush the jobstate
	// log file in recovery mode.  Outside of recovery mode, though
	// we want to do that so that the jobstate log file stays
	// current with the status of the workflow.
	if ( ! _recovery) { _jobstateLog.Flush(); }

	return result;
}

//---------------------------------------------------------------------------
// Developer's Note: returning false tells main_timer to abort the DAG
bool Dag::ProcessOneEvent (ULogEventOutcome outcome, const ULogEvent *event, bool recovery, bool &done) {
	bool result = true;

	static int log_unk_count = 0;
	static int ulog_rd_error_count = 0;
	
	debug_printf(DEBUG_DEBUG_4, "Log outcome: %s\n", ULogEventOutcomeNames[outcome]);

	if (outcome != ULOG_UNK_ERROR) { log_unk_count = 0; }

	switch (outcome) {
		case ULOG_NO_EVENT:
			done = true;
			break;
		case ULOG_RD_ERROR:
			if (++ulog_rd_error_count >= 10) {
				debug_printf(DEBUG_QUIET, "ERROR: repeated (%d) failures to read job log; quitting...\n",
				             ulog_rd_error_count);
				result = false;
			} else {
				debug_printf(DEBUG_NORMAL, "ERROR: failure to read job log\n"
				             "\tA log event may be corrupt.  DAGMan will skip the event and try to\n\tcontinue, but "
				             "information may have been lost.  If DAGMan exits\n\tunfinished, but reports no failed "
				             "nodes, re-submit the rescue file\n\tto complete the DAG.\n");
			}
			done = true;
			break;
		case ULOG_UNK_ERROR:
			log_unk_count++;
			if (recovery || log_unk_count >= 5) {
				debug_printf( DEBUG_QUIET, "ERROR: repeated (%d) unknown log errors; quitting...\n", log_unk_count);
				result = false;
			}
			debug_printf( DEBUG_NORMAL, "ERROR: unknown log error\n" );
			done = true;
			break;
		case ULOG_OK:
		{
			ASSERT(event != nullptr);
			bool submitEventIsSane;
			Node *node = LogEventNodeLookup(event, submitEventIsSane);
			PrintEvent(DEBUG_VERBOSE, event, node, recovery);
			// event is for a job outside this DAG; ignore it
			if ( ! node) { break; }
			if ( ! EventSanityCheck(event, node, &result)) {
				// this event is "impossible"; we will either abort the DAG (if result was set to false) or
				// ignore it and hope for the best...
				break;
			}

			// Log this event if necessary.
			_jobstateLog.WriteEvent(event, node);
			node->SetLastEventTime(event);

			_validatedState = _validatedStateTime == node->GetLastStateChange();

			// Note: this is a bit conservative -- some events (e.g.,
			// ImageSizeUpdate) don't actually outdate the status file.
			// If we need to, we could move this down to the cases
			// where it's strictly necessary.
			_statusFileOutdated = true;

			switch(event->eventNumber) {

				case ULOG_EXECUTABLE_ERROR:
					// Don't do anything here, because we seem to always also get an ABORTED event when we get an
					// EXECUTABLE_ERROR event.  (Not doing anything here fixes Gnats PR 697.)
					break;

				case ULOG_JOB_ABORTED:
					// Make sure we don't count finished jobs as idle.
					ProcessNotIdleEvent(node, event->proc);
					ProcessAbortEvent(event, node, recovery);
					break;

				case ULOG_JOB_TERMINATED:
					// Make sure we don't count finished jobs as idle.
					ProcessNotIdleEvent(node, event->proc);
					ProcessTerminatedEvent(event, node, recovery);
					break;

				case ULOG_POST_SCRIPT_TERMINATED:
					ProcessPostTermEvent(event, node, recovery);
					break;

				case ULOG_SUBMIT:
					ProcessSubmitEvent(node, recovery, submitEventIsSane);
					ProcessIsIdleEvent(node, event->proc);
					break;

				case ULOG_JOB_RECONNECT_FAILED:
				case ULOG_JOB_EVICTED:
				case ULOG_JOB_SUSPENDED:
				case ULOG_SHADOW_EXCEPTION:
					ProcessIsIdleEvent(node, event->proc);
					break;

				case ULOG_JOB_HELD:
					ProcessHeldEvent(node, event);
					ProcessIsIdleEvent(node, event->proc);
					break;

				case ULOG_JOB_UNSUSPENDED:
					ProcessNotIdleEvent(node, event->proc);
					break;

				case ULOG_EXECUTE:
					ProcessNotIdleEvent(node, event->proc);
					break;

				case ULOG_JOB_RELEASED:
					ProcessReleasedEvent(node, event);
					break;

				case ULOG_PRESKIP:
					TerminateNode(node, recovery);
					if ( ! recovery) { --_preRunNodeCount; }
					// TEMP -- we probably need to write something to the
					// jobstate.log file here, but JOB_SUCCESS is not the
					// right thing.  wenger 2011-09-01
					// _jobstateLog.WriteJobSuccessOrFailure( node );
					break;

				case ULOG_CLUSTER_SUBMIT:
					ProcessClusterSubmitEvent(node);
					break;

				case ULOG_CLUSTER_REMOVE:
					ProcessClusterRemoveEvent(node, recovery);
					break;

				case ULOG_CHECKPOINTED:
				case ULOG_IMAGE_SIZE:
				case ULOG_NODE_EXECUTE:
				case ULOG_NODE_TERMINATED:
				case ULOG_GENERIC:
				case ULOG_JOB_AD_INFORMATION:
				default:
					break;
			}
		}
		break;

	default:
		debug_printf(DEBUG_QUIET, "ERROR: illegal ULogEventOutcome value (%d)!!!\n", outcome);
		break;
	}

	return result;
}


//---------------------------------------------------------------------------
void
Dag::ProcessAbortEvent(const ULogEvent *event, Node *node, bool recovery) {
	// NOTE: while there are known HTCondor bugs leading to a "double"
	// terminate-then-abort event pair for the same job proc abortion,
	// this method will no longer actually see the second (abort) event,
	// because the event-checking code will suppress it.  Therefore, this
	// code no longer has to worry about it.  Note, though, that we can
	// still see a combination of terminated and aborted events for the
	// same *node* (not job proc).

	if (node) {
		node->SetProcEvent(event->proc, ABORT_TERM_MASK);
		node->IncrementJobsAborted();

		// This code is here because if a held job is removed, we
		// don't get a released event for that job.  This may not
		// work exactly right if some procs of a cluster are held
		// and some are not.  wenger 2010-08-26
		if (node->_jobProcsOnHold > 0) { node->Release(event->proc); }

		DecrementProcCount(node);

		// Only change the node status, error info,
		// etc., if we haven't already gotten an error
		// from another job proc in this job cluster
		if (node->GetStatus() != Node::STATUS_ERROR) {
			node->TerminateFailure();
			formatstr(node->error_text, "HTCondor reported %s event for job proc (%d.%d.%d)",
			          event->eventName(), event->cluster, event->proc, event->subproc);
			node->retval = DAG_ERROR_CONDOR_JOB_ABORTED;
			
			// It seems like we should be checking _numSubmittedProcs here, but
			// that breaks a test in Windows. Keep an eye on this in case we
			// have trouble recovering from aborted jobs using late materialization.
			if (node->_queuedNodeJobProcs > 0) {
				node->MarkFailed();
				// once one job proc fails, remove the whole cluster
				std::string rm_reason;
				formatstr(rm_reason, "Node Error: DAG node %s (%d.%d.%d) got %s event.",
				          node->GetNodeName(), event->cluster, event->proc, event->subproc, event->eventName());
				RemoveBatchJob(node, rm_reason);
			}
			if (node->_scriptPost != nullptr) {
				// let the script know the job's exit status
				node->_scriptPost->_retValJob = node->retval;
			}
		}

		//If no post script and not a retry node then set descendants to Futile
		if (!node->_scriptPost && !node->DoRetry()) { _numNodesFutile += node->SetDescendantsToFutile(*this); }

		ProcessJobProcEnd(node, recovery, true);
	}
}

//---------------------------------------------------------------------------
void
Dag::ProcessTerminatedEvent(const ULogEvent *event, Node *node, bool recovery) {
	if (node) {
		node->SetProcEvent(event->proc, ABORT_TERM_MASK);

		DecrementProcCount(node);

		const JobTerminatedEvent* termEvent = (const JobTerminatedEvent*) event;

		bool failed = !(termEvent->normal && termEvent->returnValue == 0);

		node->CountJobExitCode(termEvent->returnValue);

		if (failed) { // job failed or was killed by a signal
			if (termEvent->normal) {
				debug_printf(DEBUG_QUIET, "Node %s job proc (%d.%d.%d) failed with status %d.\n",
				             node->GetNodeName(), event->cluster, event->proc, event->subproc,
				             termEvent->returnValue);
			} else {
				debug_printf(DEBUG_QUIET, "Node %s job proc (%d.%d.%d) failed with signal %d.\n",
				             node->GetNodeName(), event->cluster, event->proc, event->subproc,
				             termEvent->signalNumber);
			}

			// Only change the node status, error info, etc.,
			// if we haven't already gotten an error on this node.
			if (node->GetStatus() != Node::STATUS_ERROR) {
				if (termEvent->normal) {
					formatstr(node->error_text, "Job proc (%d.%d.%d) failed with status %d",
					          termEvent->cluster, termEvent->proc, termEvent->subproc, termEvent->returnValue);
					node->retval = termEvent->returnValue;
					if (node->_scriptPost != nullptr) {
						// let the script know the job's exit status
						node->_scriptPost->_retValJob = node->retval;
					}
				} else {
					formatstr(node->error_text, "Job proc (%d.%d.%d) failed with signal %d",
					          termEvent->cluster, termEvent->proc, termEvent->subproc, termEvent->signalNumber);
					node->retval = (0 - termEvent->signalNumber);
					if (node->_scriptPost != nullptr) {
						// let the script know the job's exit status
						node->_scriptPost->_retValJob = node->retval;
					}
				}

				node->TerminateFailure();
				if (node->_queuedNodeJobProcs > 0) {
					node->MarkFailed();
					// once one job proc fails, remove the whole cluster
					std::string rm_reason;
					formatstr(rm_reason, "Node Error: DAG node %s (%d.%d.%d) failed with signal %d.",
					          node->GetNodeName(), termEvent->cluster, termEvent->proc, termEvent->subproc, termEvent->signalNumber);
					RemoveBatchJob(node, rm_reason);
				}
			}

			//If no post script and not retrying the node then set descendants to Futile
			if (!node->_scriptPost && !node->DoRetry()) { _numNodesFutile += node->SetDescendantsToFutile(*this); }

		} else { // job succeeded
			ASSERT(termEvent->returnValue == 0);
			_totalJobsCompleted++;

			// Only change the node status if we haven't already gotten an error on this node.
			if (node->GetStatus() != Node::STATUS_ERROR) {
				node->retval = 0;
				if (node->_scriptPost != nullptr) {
					// let the script know the job's exit status
					node->_scriptPost->_retValJob = node->retval;
				}
			} else {
				debug_printf(DEBUG_NORMAL, "Node %s job proc (%d.%d.%d) completed successfully, but status is STATUS_ERROR\n",
				             node->GetNodeName(), termEvent->cluster, termEvent->proc, termEvent->subproc);
			}
			debug_printf(DEBUG_NORMAL, "Node %s job proc (%d.%d.%d) completed successfully.\n",
			             node->GetNodeName(), termEvent->cluster, termEvent->proc, termEvent->subproc);
		}

		ProcessJobProcEnd(node, recovery, failed);

		if (node->_scriptPost == nullptr) {
			if (CheckForDagAbort(node, "job")) { return; }
		}

		PrintReadyQ(DEBUG_DEBUG_2);

		return;
	}
}

//---------------------------------------------------------------------------
void
Dag::RemoveBatchJob(Node *node, const std::string& reason) {
	ArgList args;
	std::string constraint;

	args.AppendArg(_condorRmExe);
	args.AppendArg(std::to_string(node->GetCluster()));
	args.AppendArg("-const");

	// Adding this DAGMan's cluster ID as a constraint to
	// be extra-careful to avoid removing someone else's job.
	formatstr(constraint, ATTR_DAGMAN_JOB_ID "==%d", _DAGManJobId->_cluster);
	args.AppendArg(constraint.c_str());

	args.AppendArg("-reason");
	args.AppendArg(reason);

	std::string display;
	args.GetArgsStringForDisplay(display);
	debug_printf(DEBUG_VERBOSE, "Executing: %s\n", display.c_str());
	if (_dagmanUtils.popen(args) != 0) {
		// Note: error here can't be fatal because there's a race condition where
		// you could do a condor_rm on a job that already terminated.  wenger 2006-02-08.
		debug_printf(DEBUG_NORMAL, "Error removing DAG node jobs\n");
	}
}

//---------------------------------------------------------------------------
// Note:  if job is the final node of the DAG, should we set _dagStatus
// in here according to whether job succeeded or failed?  wenger 2014-03-18
void
Dag::ProcessJobProcEnd(Node *node, bool recovery, bool failed) {
	// This function should never be called when the dag object is
	// being used to parse a splice.
	ASSERT (_isSplice == false);

	if (node->AllProcsDone()) {
		// Log job success or failure if necessary.
		_jobstateLog.WriteJobSuccessOrFailure(node);
	}

	// Note: structure here should be cleaned up, but I'm leaving it for
	// now to make sure parallel universe support is complete for 6.7.17.
	// wenger 2006-02-15.
	bool putFailedJobsOnHold = param_boolean("DAGMAN_PUT_FAILED_JOBS_ON_HOLD", false);
	if (failed && node->_scriptPost == nullptr) {
		if (node->DoRetry()) {
			if (node->AllProcsDone()) { RestartNode(node, recovery); }
		} else if (putFailedJobsOnHold) {
			node->SetHold(true);
			// Increase the job's retry max, so it will try again after the
			// retry count gets increased in the RestartNode() function.
			// We might want to limit this to avoid livelock.
			if (node->AllProcsDone()) {
				node->SetRetryMax(node->GetRetryMax() + 1);
				RestartNode(node, recovery);
			}
		} else {
			// no more retries -- node failed
			if (node->GetRetryMax() > 0) {
				// add # of retries to error_text
				formatstr_cat(node->error_text, " (after %d node retries)", node->GetRetries());
			}
			if (node->_queuedNodeJobProcs == 0) {
				if (node->GetType() != NodeType::SERVICE) {
					_numNodesFailed++;
				}
				_metrics->NodeFinished(node->GetDagFile() != nullptr, false);
				if (_dagStatus == DAG_STATUS_OK) {
					_dagStatus = DAG_STATUS_NODE_FAILED;
				}
			}
		}
		return;
	}

	// If this is *not* a multi-proc cluster job, and no more procs are
	// outstanding, start shutting things down now.
	// Multi-proc cluster jobs get shut down in ProcessClusterRemoveEvent().
	if (node->AllProcsDone()) {
		// All procs for this job are done.
		debug_printf(DEBUG_NORMAL, "Node %s job completed\n", node->GetNodeName());

		// if a POST script is specified for the node, run it
		if (node->_scriptPost != nullptr) {
			if (recovery) {
				node->SetStatus(Node::STATUS_POSTRUN);
				_postRunNodeCount++;
			} else {
				(void)RunPostScript(node, dagOpts[shallow::b::PostRun], 0);
			}
		} else if (node->GetStatus() != Node::STATUS_ERROR) {
			// no POST script was specified, so update DAG with
			// node's successful completion if the node succeeded.
			TerminateNode(node, recovery);
		}
	}
}

//---------------------------------------------------------------------------
void
Dag::ProcessPostTermEvent(const ULogEvent *event, Node *node, bool recovery) {
	if (node) {
		// Note: "|| recovery" below is somewhat of a "quick and dirty"
		// fix to Gnats PR 357.  The first part of the assert can fail
		// in recovery mode because if any retries of a node failed
		// because the post script failed, they don't show up in the
		// user log.  Therefore, condor_dagman doesn't know abou them,
		// and can think the post script was run before all retries
		// were exhausted.  wenger 2004-11-23.
		if (node->GetStatus() == Node::STATUS_POSTRUN) {
			_postRunNodeCount--;
		} else {
			ASSERT(recovery);
			// If we get here, it means that, in the run we're recovering,
			// the POST script for a node was run without any indication
			// in the log that the node was run.  This probably means that
			// all submit attempts on the node failed.  wenger 2007-04-09.
		}

		const PostScriptTerminatedEvent* termEvent = (const PostScriptTerminatedEvent*)event;

		std::string header;
		formatstr(header, "POST Script of node %s ", node->GetNodeName());
		if( ! (termEvent->normal && termEvent->returnValue == 0)) {
			// POST script failed or was killed by a signal
			node->TerminateFailure();

			int mainJobRetval = node->retval;
			std::string errStr;

			if (termEvent->normal) {
				// Normal termination -- POST script failed
				formatstr(errStr, "failed with status %d", termEvent->returnValue);
				debug_printf(DEBUG_NORMAL, "%s%s\n", header.c_str(), errStr.c_str());
				debug_printf(DEBUG_QUIET, "POST for Node %s returned %d\n",
				             node->GetNodeName(),termEvent->returnValue);

				node->retval = termEvent->returnValue;
			} else {
				// Abnormal termination -- POST script killed by signal
				formatstr(errStr, "died on signal %d", termEvent->signalNumber);
				debug_printf(DEBUG_NORMAL, "%s%s\n", header.c_str(), errStr.c_str());

				node->retval = (0 - termEvent->signalNumber);
			}

			formatstr(node->error_text, "POST script %s", errStr.c_str());

			// Log post script success or failure if necessary.
			_jobstateLog.WriteScriptSuccessOrFailure(node, ScriptType::POST);

			// Deal with retries.
			if (node->DoRetry()) {
				RestartNode(node, recovery);
			} else {
				// no more retries -- node failed
				if (node->GetType() != NodeType::SERVICE) {
					_numNodesFutile += node->SetDescendantsToFutile(*this);
					_numNodesFailed++;
				}
				_metrics->NodeFinished(node->GetDagFile() != nullptr, false);
				if (_dagStatus == DAG_STATUS_OK) {
					_dagStatus = DAG_STATUS_NODE_FAILED;
				}

				if (mainJobRetval > 0) {
					formatstr(node->error_text, "Job exited with status %d and ", mainJobRetval);
				} else if (mainJobRetval < 0 && mainJobRetval >= -MAX_SIGNAL) {
					formatstr(node->error_text, "Job died on signal %d and ", 0 - mainJobRetval);
				} else {
					formatstr(node->error_text, "Job failed due to DAGMAN error %d and ", mainJobRetval);
				}

				if (termEvent->normal) {
					formatstr_cat(node->error_text, "POST Script failed with status %d", termEvent->returnValue);
				} else {
					formatstr_cat(node->error_text, "POST Script died on signal %d", termEvent->signalNumber);
				}

				if (node->GetRetryMax() > 0) {
					// add # of retries to error_text
					formatstr_cat(node->error_text, " (after %d node retries)", node->GetRetries());
				}
			}

		} else {
			// POST script succeeded.
			ASSERT(termEvent->returnValue == 0);
			debug_printf(DEBUG_NORMAL, "%scompleted successfully.\n", header.c_str());
			node->retval = 0;
			// Log post script success or failure if necessary.
			_jobstateLog.WriteScriptSuccessOrFailure(node, ScriptType::POST);
			TerminateNode(node, recovery);
		}

		// if dag abort happened, we never return here!
		if (CheckForDagAbort(node, "POST script")) { return; }

		PrintReadyQ(DEBUG_DEBUG_4);
	}
}

//---------------------------------------------------------------------------
void
Dag::ProcessSubmitEvent(Node *node, bool recovery, bool &submitEventIsSane) {
	if ( ! node) { return; }

	// If we're in recovery mode, we need to keep track of the
	// maximum subprocID for NOOP jobs, so we can start out at
	// the next value instead of zero.
	if (recovery) {
		if (NodeIsNoop(node->GetID())) {
			_recoveryMaxfakeID = MAX(_recoveryMaxfakeID, GetIndexID(node->GetID()));
		}
	}

	if (node->IsWaiting()) {
		debug_printf(DEBUG_QUIET, "Error: DAG semantics violated! Node %s was submitted but has unfinished parents!\n",
		             node->GetNodeName());
		std::string dagFile = dagOpts.primaryDag();
		debug_printf(DEBUG_QUIET, "This may indicate log file corruption; you may want to check the log files and re-run the "
					"DAG in recovery mode by giving the command 'condor_submit %s.condor.sub'\n", dagFile.c_str());
		node->Dump(this);
		// We do NOT want to create a rescue DAG here, because it
		// would probably be invalid.
		DC_Exit(EXIT_ERROR);
	}

	// If we got an "insane" submit event for a node that currently
	// has a job in the queue, we don't want to increment
	// the job proc count and jobs submitted counts here, because
	// if we get a terminated event corresponding to the "original"
	// HTCondor ID, we won't decrement the counts at that time.
	if (submitEventIsSane || node->GetStatus() != Node::STATUS_SUBMITTED) {
		node->_queuedNodeJobProcs++;
		node->_numSubmittedProcs++;
		_totalJobsSubmitted++;
	}

	// Note: in non-recovery mode, we increment _numNodesSubmitted in ProcessSuccessfulSubmit().
	if (recovery) {
		if (submitEventIsSane || node->GetStatus() != Node::STATUS_SUBMITTED) {
			// Only increment the submitted job count on the *first* proc of a job.
			if (node->_numSubmittedProcs == 1) {
				UpdateNodeCounts(node, 1);
			}
		}

		node->SetStatus(Node::STATUS_SUBMITTED);
		return;
	}

	// If we only have one log, compare the order of submit events in the
	// log to the order in which we submitted the jobs -- but if we
	// have >1 userlog we can't count on any order, so we can't sanity-check
	// in this way.
	// What happens if we have more than one log? Will the DAG run
	// correctly without hitting this code block?
	if (TotalLogFileCount() == 1) {
		// Only perform sanity check on the first proc in a job cluster.
		if (node->_numSubmittedProcs == 1) {

			// as a sanity check, compare the job from the submit event to
			// the job we expected to see from our submit queue
			Node* expectedNode = nullptr;
			if (_submitQ->empty()) {
				debug_printf(DEBUG_QUIET, "Unrecognized submit event (for node \"%s\") found in log (none expected)\n",
				             node->GetNodeName());
				return;
			}
			expectedNode = _submitQ->front();
			_submitQ->pop();
			if (node != expectedNode) {
				ASSERT(expectedNode != nullptr);
				debug_printf(DEBUG_QUIET, "Unexpected submit event (for node \"%s\") found in log; node \"%s\" was expected.\n",
				             node->GetNodeName(), expectedNode->GetNodeName());
				// put expectedNode back onto submit queue
				_submitQ->push(expectedNode);
				return;
			}
		}
	}

	// If this is a provisioner node, initialize the classad object
	// that communicates with the schedd.
	if (node->GetType() == NodeType::PROVISIONER) {
		CondorID provisionerId = CondorID(node->GetCluster(), node->GetProc(), 0);
		_provisionerClassad = new ProvisionerClassad(provisionerId, _schedd);
	}

	PrintReadyQ(DEBUG_DEBUG_2);
}

//---------------------------------------------------------------------------
void
Dag::ProcessIsIdleEvent(Node *node, int proc) {
	if ( ! node) { return; }

	// Note:  we need to make sure here that the job proc isn't already
	// idle so we don't count it twice if, for example, we get a hold
	// event for a job that's already idle.
	if ( !node->GetProcIsIdle(proc) && (node->GetStatus() == Node::STATUS_SUBMITTED)) {
		node->SetProcIsIdle(proc, true);
		_numIdleJobProcs++;
	}

	// Do some consistency checks here.
	if (_numIdleJobProcs > 0 && NumNodesSubmitted() < 1) {
		debug_printf(DEBUG_NORMAL,
		            "Warning:  DAGMan thinks there are %d idle job procs, even though there are no nodes in the queue!  Setting idle count to 0 so DAG continues.\n",
		            _numIdleJobProcs );
		check_warning_strictness(DAG_STRICT_2);
		_numIdleJobProcs = 0;
	}

	debug_printf(DEBUG_VERBOSE, "Number of idle job procs: %d\n", _numIdleJobProcs);
}

//---------------------------------------------------------------------------
void
Dag::ProcessNotIdleEvent(Node *node, int proc) {
	if ( ! node) { return; }

	if (node->GetProcIsIdle(proc) &&
	    (node->GetStatus() == Node::STATUS_SUBMITTED || node->GetStatus() == Node::STATUS_ERROR))
	{
		node->SetProcIsIdle(proc, false);
		_numIdleJobProcs--;
	}

	// Do some consistency checks here.
	if (_numIdleJobProcs < 0) {
		debug_printf(DEBUG_NORMAL, "WARNING: DAGMan thinks there are %d idle job procs!\n",
		             _numIdleJobProcs );
	}

	if (_numIdleJobProcs > 0 && NumNodesSubmitted() < 1) {
		debug_printf(DEBUG_NORMAL,
		             "Warning:  DAGMan thinks there are %d idle job procs, even though there are no nodes in the queue!  Setting idle count to 0 so DAG continues.\n",
		             _numIdleJobProcs);
		check_warning_strictness(DAG_STRICT_2);
		_numIdleJobProcs = 0;
	}

	node->SetProcEvent(proc, EXEC_MASK);

	debug_printf(DEBUG_VERBOSE, "Number of idle job procs: %d\n", _numIdleJobProcs);
}

//---------------------------------------------------------------------------
// We need the event here so we can tell which process has been held for
// multi-process jobs.
void
Dag::ProcessHeldEvent(Node *node, const ULogEvent *event) {
	if ( ! node) { return; }
	ASSERT(event);

	const JobHeldEvent* heldEvent = (const JobHeldEvent*)event;
	const char *reason = heldEvent->getReason();
	debug_printf(DEBUG_VERBOSE, "  Hold reason: %s\n", (reason && reason[0]) ? reason : "(unknown)");

	if (node->Hold(event->proc)) {
		if (_maxJobHolds > 0 && node->_jobProcsOnHold >= _maxJobHolds) {
			debug_printf(DEBUG_VERBOSE, "Total hold count for job %d (node %s)has reached DAGMAN_MAX_JOB_HOLDS (%d); all job "
			             "proc(s) for this node will now be removed\n", event->cluster, node->GetNodeName(), _maxJobHolds);
			std::string rm_reason = "DAG Limit: Max number of held jobs was reached.";
			RemoveBatchJob(node, rm_reason);
		}
	}

	if (node->_scriptHold != nullptr) {
		// TODO: Does running a HOLD script warrant a separate helper function?
		RunHoldScript(node, true);
	}
}

//---------------------------------------------------------------------------
void
Dag::ProcessReleasedEvent(Node *node,const ULogEvent* event) {
	ASSERT(event);
	if ( ! node) { return; }
	node->Release(event->proc);
}

//---------------------------------------------------------------------------
void
Dag::ProcessClusterSubmitEvent(Node *node) {
	if ( ! node) { return; }
	node->is_factory = true;
}

//---------------------------------------------------------------------------
void
Dag::ProcessClusterRemoveEvent(Node *node, bool recovery) {
	if ( ! node) { return; }

	// Make sure the job is a multi-proc cluster, and has no more queued procs. 
	// Otherwise something is wrong.
	if (node->_queuedNodeJobProcs == 0 && node->is_factory) {
		// All procs for this job are done.
		debug_printf(DEBUG_NORMAL, "Node %s job completed\n", node->GetNodeName());
		// If a post script is defined for this job, that will run after we
		// receive the ClusterRemove event. In that case, run the post script
		// now and don't call TerminateNode(); that will get called later by
		// ProcessPostTermEvent().
		if (node->_scriptPost != nullptr) {
			if (recovery) {
				node->SetStatus(Node::STATUS_POSTRUN);
				_postRunNodeCount++;
			} else {
				(void)RunPostScript(node, dagOpts[shallow::b::PostRun], 0);
			}
		} else if (node->GetStatus() != Node::STATUS_ERROR) {
			// no POST script was specified, so update DAG with
			// node's successful completion if the node succeeded.
			TerminateNode(node, recovery);
		}
	}
	else {
		debug_printf(DEBUG_NORMAL, "ERROR: ProcessClusterRemoveEvent() called for node %s although %d procs still queued.\n",
		             node->GetNodeName(), node->_queuedNodeJobProcs);
	}

	// If this cluster did not complete successfully, restart it.
	if (node->GetStatus() == Node::STATUS_ERROR) {
		if (node->DoRetry()) {
			RestartNode(node, recovery);
		}
	}

	// Cleanup the job and write succcess/failure to the log. 
	// For non-cluster jobs, this is done in DecrementProcCount
	_jobstateLog.WriteJobSuccessOrFailure(node);
	UpdateNodeCounts(node, -1);
	node->Cleanup();
}

//---------------------------------------------------------------------------
Node* Dag::FindNodeByName(const char * nodeName) const {
	if ( ! nodeName) { return nullptr; }

	Node* node = nullptr;
	auto findResult = _nodeNameHash.find(nodeName);
	if (findResult == _nodeNameHash.end()) {
		debug_printf(DEBUG_VERBOSE, "ERROR: Node %s not found!\n", nodeName);
	} else {
		node = (*findResult).second;
	}

	if (node) {
		if (strcmp(nodeName, node->GetNodeName()) != MATCH) {
			EXCEPT("Searched for node %s; got %s!!", nodeName, node->GetNodeName());
		}
	}

	return node;
}

//---------------------------------------------------------------------------
Node*
Dag::FindAllNodesByName(const char* nodeName, const char *finalSkipMsg, const char *file, int line) const
{
	bool skipFinalNode = true;
	Node *node = nullptr;
	if (nodeName) {
		if (strcasecmp(nodeName, DAG::ALL_NODES) != MATCH) {
			// Looking for a specific node.
			_allNodesIt = _nodes.end(); 
			// Specific node lookups should not skip the final node.
			// TODO: Some commands (RETRY) should skip the final node,
			// but others (SCRIPT) should not. Find a better way to do this.
			skipFinalNode = false;
			node = FindNodeByName(nodeName);
		} else {
			// First call when looking for ALL_NODES.
			_allNodesIt = _nodes.begin();
			node = *_allNodesIt++;
		}

	} else {
		// Second or subsequent call when looking for ALL_NODES.
		if (_allNodesIt != _nodes.end()) {
			node = *_allNodesIt++;
		} else {
			node = nullptr;
		}
	}

	// We want to skip final nodes if we're in ALL_NODES mode.
	if (node && node->GetType() == NodeType::FINAL && skipFinalNode) {
		debug_printf(DEBUG_NORMAL, finalSkipMsg, node->GetNodeName(), file, line);
		// There can only be one FINAL node. 
		// If there are more _nodes left in the container, advance the iterator.
		if (_allNodesIt != _nodes.end()) {
			node = *_allNodesIt++;
		} else {
			// If no more nodes left, return node = NULL
			node = nullptr;
		}
	}

	return node;
}

//---------------------------------------------------------------------------
bool
Dag::NodeExists(const char* nodeName) const
{
	if ( ! nodeName) { return false; }

	// Note:  we don't just call FindNodeByName() here because that would print
	// an error message if the node doesn't exist.
	Node* node = nullptr;
	auto findResult = _nodeNameHash.find(nodeName);
	if (findResult == _nodeNameHash.end()) {
		return false;
	} else {
		node = (*findResult).second;
	}

	if (node) {
		if (strcmp(nodeName, node->GetNodeName()) != MATCH) {
			EXCEPT("Searched for node %s; got %s!!", nodeName, node->GetNodeName());
		}
	}

	return node != nullptr;
}

//---------------------------------------------------------------------------
Node* Dag::FindNodeByEventID (const CondorID condorID) const {
	if (condorID._cluster == -1) { return nullptr; }

	Node * node = nullptr;
	bool isNoop = NodeIsNoop(condorID);
	int id = GetIndexID(condorID);
	auto findResult = GetEventIDHash(isNoop)->find(id);
	if (findResult == GetEventIDHash(isNoop)->end()) {
		// Note: eventually get rid of the "(might be because of
		// node retries)" message here, and have code that explicitly
		// figures out whether the node was not found because of a
		// retry.  (See gittrac #1957 and #1961.)
		debug_printf(DEBUG_VERBOSE, "ERROR: node for condor ID %d.%d.%d not found! (might be because of node retries)\n",
		             condorID._cluster, condorID._proc, condorID._subproc);
	}
	else {
		node = (*findResult).second;
	}

	if (node) {
		if (condorID._cluster != node->GetCluster()) {
			if (node->GetCluster() != _defaultCondorId._cluster) {
				EXCEPT("Searched for node for cluster %d; got %d!!", condorID._cluster,	node->GetCluster());
			} else {
				// Note: we can get here if we get an aborted event after a terminated event for the same job (see
				// gittrac #744 and the job_dagman_abnormal_term_recovery_retries test).
				debug_printf(DEBUG_QUIET, "Warning: searched for node for cluster %d; got %d!!\n",
				             condorID._cluster, node->GetCluster());
				check_warning_strictness(DAG_STRICT_3);
			}
		}
		ASSERT(isNoop == node->GetNoop());
	}

	return node;
}

//-------------------------------------------------------------------------
void
Dag::PrintDagFiles(const std::list<std::string> &dagFiles)
{
	if (dagFiles.size() > 1) {
		debug_printf(DEBUG_VERBOSE, "All DAG files:\n");
		int thisDagNum = 0;
		for (auto & dagFile : dagFiles) {
			debug_printf(DEBUG_VERBOSE, "  %s (DAG #%d)\n", dagFile.c_str(), thisDagNum++);
		}
	}
}

//-------------------------------------------------------------------------
bool
Dag::StartNode(Node *node, bool isRetry)
{
	// We should never be calling this function when the dag is being used as a splicing dag.
	ASSERT(_isSplice == false);
	ASSERT( ! _finalNodeRun);
	ASSERT(node != nullptr);

	if ( ! node->CanSubmit()) {
		EXCEPT("Node %s not ready to submit!", node->GetNodeName());
	}

	if ( ! isRetry) { WriteSavePoint(node); }

	// if a PRE script exists and hasn't already been run, run that
	// first -- the PRE script's reaper function will start the
	// actual node if/when the script exits successfully
	if (node->_scriptPre && node->_scriptPre->_done == FALSE) {
		node->SetStatus(Node::STATUS_PRERUN);
		_preRunNodeCount++;
		_preScriptQ->Run(node->_scriptPre);
		return true;
	}

	// no PRE script exists or is done, so add node to the queue of ready nodes
	if (isRetry && m_retryNodeFirst) {
		_readyQ->prepend(node);
	} else {
		if (_submitDepthFirst) {
			_readyQ->prepend(node);
		} else {
			_readyQ->append(node);
		}
	}
	return TRUE;
}

//-------------------------------------------------------------------------
bool
Dag::StartFinalNode()
{
	if (_final_node && _final_node->GetStatus() == Node::STATUS_NOT_READY) {
		debug_printf(DEBUG_QUIET, "Starting final node...\n");

		// Clear out the ready queue so "regular" nodes don't run
		// when we run the final node (this is really just needed
		// for the DAG abort and DAG halt cases).
		// Note:  maybe we should change nodes in the prerun state
		// to not ready here, to be more consistent.  But I'm not
		// dealing with that for now.  wenger 2014-03-17
		auto nonFinal = [](Node *node) -> bool {
			if (node->GetType() != NodeType::FINAL) {
				debug_printf(DEBUG_DEBUG_2, "Removing node %s from ready queue\n", node->GetNodeName());
				node->SetStatus(Node::STATUS_NOT_READY);
				return true;
			}
			return false;
		};

		size_t erased = _readyQ->erase_if(nonFinal);
		debug_printf(DEBUG_DEBUG_1, "Removed %lu nodes from the ready queue.\n", erased);
		// Note: Don't need to call _readyQ->make_heap() because queue should be 1 node or empty
		ASSERT(_readyQ->size() == 0 || _readyQ->size() == 1);

		// Now start up the final node.
		_final_node->SetStatus(Node::STATUS_READY);
		if (StartNode(_final_node, false)) {
			_finalNodeRun = true;
			return true;
		}
	}

	return false;
}

//-------------------------------------------------------------------------
bool
Dag::StartServiceNodes()
{
	for (auto& node: _service_nodes) {
		if (node->GetStatus() == Node::STATUS_READY) {
			debug_printf(DEBUG_QUIET, "Starting service node %s...\n", node->GetNodeName());
			if ( ! StartNode(node, false)) {
				return false;
			}
		}
	}

	return true;
}


//-------------------------------------------------------------------------
bool
Dag::RemoveServiceNodes()
{
	for (auto& node: _service_nodes) {
		if (node->GetStatus() == Node::STATUS_SUBMITTED) {
			RemoveBatchJob( node );
			TerminateNode( node, false, false );
		}
	}

	return true;
}

//-------------------------------------------------------------------------
bool
Dag::StartProvisionerNode()
{
	if ( _provisioner_node && _provisioner_node->GetStatus() == Node::STATUS_READY) {
		debug_printf(DEBUG_QUIET, "Starting provisioner node...\n");
		if ( StartNode(_provisioner_node, false)) {
			return true;
		}
	}

	return false;
}

//-------------------------------------------------------------------------
int
Dag::GetProvisionerJobAdState()
{
	int provisionerState = -1;
	if (_provisionerClassad) {
		provisionerState = _provisionerClassad->GetProvisionerState();
	}
	return provisionerState;
}

//-------------------------------------------------------------------------
// returns number of nodes submitted
int
Dag::SubmitReadyNodes(const Dagman &dm)
{
	debug_printf(DEBUG_DEBUG_1, "Dag::SubmitReadyNodes()\n");
	time_t cycleStart = time(nullptr);

	// Nodes deferred by category throttles.
	std::vector<Node*> deferredNodes;

	int numSubmitsThisCycle = 0;

	// Check whether we have to wait longer before submitting again
	// (if a previous submit attempt failed).
	if (_nextSubmitTime && time(nullptr) < _nextSubmitTime) {
		sleep(1);
		return numSubmitsThisCycle;
	}

	// Check whether the held file exists -- if so, we don't submit any jobs or run scripts.
	bool prevDagIsHalted = _dagIsHalted;
	_dagIsHalted = (access(_haltFile.c_str() , F_OK) == 0);

	if (_dagIsHalted) {
		debug_printf(DEBUG_QUIET, "DAG is halted because halt file %s exists\n", _haltFile.c_str());
		if (_finalNodeRun) {
			debug_printf(DEBUG_QUIET, "Continuing to allow final node to run\n");
		} else {
			return numSubmitsThisCycle;
		}
	}
	if (prevDagIsHalted) {
		if ( ! _dagIsHalted) {
			debug_printf(DEBUG_QUIET, "DAG going from halted to running state\n");
		}
		// If going from the halted to the not halted state, we need
		// to fire up any PRE scripts that were deferred while we were
		// halted.
		_preScriptQ->RunWaitingScripts();
	}

	// Check if we are waiting for a provisioner node to become ready
	if (HasProvisionerNode() && !_provisioner_ready) {
		// If we just moved into a provisioned state, we can start
		// submitting the other jobs in the dag
		if (GetProvisionerJobAdState() == ProvisionerState::PROVISIONING_COMPLETE) {
			_provisioner_ready = true;
			for (auto & node : _nodes) {
				if (node->CanSubmit()) {
					StartNode(node, false);
				}
			}
		}
	}

	int maxJobs = dagOpts[shallow::i::MaxJobs];
	int maxIdle = dagOpts[shallow::i::MaxIdle];

	while (numSubmitsThisCycle < dm.max_submits_per_interval) {

		// no nodes ready to submit
		if (_readyQ->empty()) { break; }

		// max jobs already submitted
		if (maxJobs && _numNodesSubmitted >= maxJobs) {
			debug_printf(DEBUG_DEBUG_1, "Max jobs (%d) already running; deferring submission of %d ready node%s.\n",
			             maxJobs, _readyQ->size(), _readyQ->size() == 1 ? "" : "s");
			_maxJobsDeferredCount += _readyQ->size();
			break; // break out of while loop
		}

		if (maxIdle && _numIdleJobProcs >= maxIdle) {
			debug_printf(DEBUG_DEBUG_1, "Hit max number of idle DAG nodes (%d); deferring submission of %d ready node%s.\n",
			             maxIdle, _readyQ->size(), _readyQ->size() == 1 ? "" : "s");
			_maxIdleDeferredCount += _readyQ->size();
			break; // break out of while loop
		}

		// Check whether this submit cycle is taking too long (only if we
		// are not in aggressive submit mode)
		if ( ! dm.aggressive_submit) {
			time_t now = time(nullptr);
			time_t elapsed = now - cycleStart;
			if (elapsed > dm.m_user_log_scan_interval) {
				debug_printf(DEBUG_QUIET, "Warning: Submit cycle elapsed time (%lld s) has exceeded log scan interval (%d s); bailing out of submit loop\n",
				            (long long) elapsed, dm.m_user_log_scan_interval);
				break; // break out of while loop
			}
		}

		// remove & submit first node from ready queue
		Node* node = _readyQ->pop();
		ASSERT(node != nullptr);

		debug_printf(DEBUG_DEBUG_1, "Got node %s from the ready queue\n", node->GetNodeName());

		if (node->GetStatus() != Node::STATUS_READY) {
			EXCEPT("Node %s status is %s, not STATUS_READY", node->GetNodeName(), node->GetStatusName());
		}

		// Check for throttling by node category.
		ThrottleByCategory::ThrottleInfo *catThrottle = node->GetThrottleInfo();
		if (catThrottle && catThrottle->isSet() && catThrottle->_currentJobs >= catThrottle->_maxJobs) {
			debug_printf(DEBUG_DEBUG_1, "Node %s deferred by category throttle (%s, %d)\n",
			             node->GetNodeName(), catThrottle->_category->c_str(), catThrottle->_maxJobs);
			deferredNodes.push_back(node);
			_catThrottleDeferredCount++;
		} else if (dagOpts[shallow::b::DryRun]) {
			// Don't actually submit the node. Just terminate it right away
			debug_printf(DEBUG_NORMAL, "Processing node: %s\n", node->GetNodeName());
			TerminateNode(node, false, false);
			numSubmitsThisCycle++;
		} else {

			// Note:  I'm not sure why we don't just use the default
			// constructor here.  wenger 2015-09-25
			CondorID condorID(0, 0, 0);
			submit_result_t submit_result = SubmitNodeJob(dm, node, condorID);
	
			// Note: if instead of switch here so we can use break
			// to break out of while loop.
			if (submit_result == SUBMIT_RESULT_OK) {
				ProcessSuccessfulSubmit(node, condorID);
				numSubmitsThisCycle++;

			} else if (submit_result == SUBMIT_RESULT_FAILED || submit_result == SUBMIT_RESULT_NO_SUBMIT) {
				ProcessFailedSubmit(node, dm.max_submit_attempts);
				break; // break out of while loop
			} else {
				EXCEPT("Illegal submit_result_t value: %d", submit_result);
			}
		}
	}

	// if we didn't actually invoke condor_submit, and we submitted any jobs
	// we should now send a reschedule command
	if (numSubmitsThisCycle > 0 && !dagOpts[shallow::b::DryRun]) {
		// If DAGMan submitted jobs without error invalidate state for queue checking
		_validatedState = false;
		send_reschedule(dm);
	}

	// Put any deferred nodes back into the ready queue for next time.
	for (Node *node : deferredNodes) {
		debug_printf(DEBUG_DEBUG_1, "Returning deferred node %s to the ready queue\n", node->GetNodeName());
		_readyQ->prepend(node);
	}

	return numSubmitsThisCycle;
}

//---------------------------------------------------------------------------
int
Dag::PreScriptReaper(Node *node, int status)
{
	ASSERT(node != nullptr);
	if (node->GetStatus() != Node::STATUS_PRERUN) {
		EXCEPT("Error: node %s is not in PRERUN state", node->GetNodeName());
	}

	_preRunNodeCount--;

	bool preScriptFailed = false;
	if (WIFSIGNALED(status)) {
		// if script was killed by a signal
		preScriptFailed = true;
		debug_printf(DEBUG_QUIET, "PRE Script of node %s died on %s\n",
		             node->GetNodeName(), daemonCore->GetExceptionString(status));
		formatstr(node->error_text, "PRE Script died on %s", daemonCore->GetExceptionString(status));
		node->retval = (0 - WTERMSIG(status));
	} else if (WEXITSTATUS(status) != 0) {
		// if script returned failure
		preScriptFailed = true;
		debug_printf(DEBUG_QUIET, "PRE Script of Node %s failed with status %d\n",
		             node->GetNodeName(), WEXITSTATUS(status) );
		formatstr(node->error_text, "PRE Script failed with status %d", WEXITSTATUS(status));
		node->retval = WEXITSTATUS(status);
	} else {
		// if script succeeded
		if (node->_scriptPost != nullptr) {
			node->_scriptPost->_retValScript = 0;
		}
		node->retval = 0;
	}

	_jobstateLog.WriteScriptSuccessOrFailure(node, ScriptType::PRE);

	if (preScriptFailed) {
		// Check for PRE_SKIP.
		if (node->HasPreSkip() && node->GetPreSkip() == node->retval) {
			// The PRE script exited with a non-zero status, but
			// because that status matches the PRE_SKIP value,
			// we're skipping the node job and the POST script
			// and considering the node successful.
			debug_printf(DEBUG_NORMAL, "PRE_SKIP return value %d indicates we are done (successfully) with node %s\n",
			             node->retval, node->GetNodeName() );

			// Mark the node as a skipped node.
			CondorID id;
			std::string logFile = DefaultNodeLog();
			if ( ! writePreSkipEvent(id, node, node->GetNodeName(), node->GetDirectory(), logFile.c_str())) {
				debug_printf(DEBUG_NORMAL, "Failed to write PRE_SKIP event for node %s\n",
				             node->GetNodeName());
				main_shutdown_rescue(EXIT_ERROR, DAG_STATUS_ERROR);
			}
			++_preRunNodeCount;
			// We are not running this again, but we want to keep the tables correct.
			// The log reading code will take  care of this in a moment.
			node->_scriptPre->_done = TRUE; // So the pre script is not run again.
			node->retval = 0; // Job _is_ successful!
		} else if (dagOpts[shallow::b::PostRun] && node->_scriptPost != nullptr) {
			// Check for POST script. PRE script Failed.  The return code is in retval member.
			node->_scriptPost->_retValScript = node->retval;
			node->_scriptPost->_retValJob = DAG_ERROR_JOB_SKIPPED;
			node->MarkFailed();
			RunPostScript(node, dagOpts[shallow::b::PostRun], node->retval);
		} else if (node->DoRetry()) {
			// Check for retries.
			node->TerminateFailure();
			// Note: don't update count in metrics here because we're retrying!
			RestartNode(node, false);
		} else {
			// None of the above apply -- the node has failed.
			node->MarkFailed();
			node->TerminateFailure();
			if (node->GetType() != NodeType::SERVICE) {
				_numNodesFutile += node->SetDescendantsToFutile(*this);
				_numNodesFailed++;
			}
			_metrics->NodeFinished(node->GetDagFile() != nullptr, false);
			if (_dagStatus == DAG_STATUS_OK) {
				_dagStatus = DAG_STATUS_NODE_FAILED;
			}
			if (node->GetRetryMax() > 0) {
				// add # of retries to error_text
				formatstr_cat(node->error_text, " (after %d node retries)", node->GetRetries());
			}
		}

	} else {
		debug_printf(DEBUG_NORMAL, "PRE Script of node %s completed successfully.\n", node->GetNodeName());
		node->retval = 0; // for safety on retries
		node->SetStatus(Node::STATUS_READY);
		if (_submitDepthFirst) {
			_readyQ->prepend(node);
		} else {
			_readyQ->append(node);
		}
	}

	CheckForDagAbort(node, "PRE script");
	// if dag abort happened, we never return here!
	return true;
}

//---------------------------------------------------------------------------
// This is the only way a POST script runs.
bool Dag::RunPostScript(Node *node, bool ignore_status, int status, bool incrementRunCount) {
	// A little defensive programming. Just check that we are
	// allowed to run this script.  Because callers can be a little
	// sloppy...
	if ((!ignore_status && status != 0) || !node->_scriptPost) {
		return false;
	}
	// a POST script is specified for the job, so run it
	// We are told to ignore the result of the PRE script
	node->SetStatus(Node::STATUS_POSTRUN);
	if (incrementRunCount) {
		_postRunNodeCount++;
	}
	_postScriptQ->Run(node->_scriptPost);

	return true;
}

//---------------------------------------------------------------------------
// Note that the actual handling of the post script's exit status is
// done not when the reaper is called, but in ProcessLogEvents when
// the log event (written by the reaper) is seen...
int
Dag::PostScriptReaper(Node *node, int status)
{
	ASSERT(node != nullptr);

	if (node->GetStatus() != Node::STATUS_POSTRUN) {
		EXCEPT("Node %s is not in POSTRUN state", node->GetNodeName());
	}

	PostScriptTerminatedEvent event;
	event.dagNodeName = node->GetNodeName();

	if (WIFSIGNALED(status)) {
		debug_printf(DEBUG_QUIET, "POST script died on signal %d\n", status);
		event.normal = false;
		event.signalNumber = status;
	} else {
		event.normal = true;
		event.returnValue = WEXITSTATUS(status);
	}

	// For NOOP nodes, we need the proc and subproc values;
	// for "real" nodes, they are not significant.
	event.cluster = node->GetCluster();
	event.proc = node->GetNoop() ? node->GetProc() : 0;
	event.subproc = node->GetNoop() ? node->GetSubProc() : 0;

	WriteUserLog ulog;
	ulog.setUseCLASSAD(0);

	std::string logFile = DefaultNodeLog();
	debug_printf(DEBUG_QUIET, "Initializing user log writer for %s, (%d.%d.%d)\n",
	             logFile.c_str(), event.cluster, event.proc, event.subproc);
	ulog.initialize(logFile.c_str(), event.cluster, event.proc, event.subproc);

	for (int write_attempts = 0;;++write_attempts) {
		if ( ! ulog.writeEvent(&event)) {
			if (write_attempts >= 2) {
				debug_printf(DEBUG_QUIET, "Unable to log ULOG_POST_SCRIPT_TERMINATED event\n");
				// Exit here, because otherwise we'll wait forever to see
				// the event that we just failed to write (see gittrac #934).
				// wenger 2009-11-12.
				main_shutdown_rescue(EXIT_ERROR, DAG_STATUS_ERROR);
			}
		} else {
			break;
		}
	}
	return true;
}

//---------------------------------------------------------------------------
// Run a HOLD script.
bool Dag::RunHoldScript(Node *node, bool incrementRunCount)
{
	// Make sure we are allowed to run this script.
	if ( ! node->_scriptHold) { return false; }
	// Run the HOLD script. This is a best-effort attempt that does not change
	// the node status.
	if (incrementRunCount) { _holdRunNodeCount++; }
	_holdScriptQ->Run(node->_scriptHold);

	return true;
}

//-------------------------------------------------------------------------
int
Dag::HoldScriptReaper(Node *node)
{
	ASSERT(node != nullptr);
	_holdRunNodeCount--;

	return true;
}

//---------------------------------------------------------------------------
void Dag::PrintNodeList() const {
	for (auto & node : _nodes) {
		node->Dump(this);
	}
	dprintf(D_ALWAYS, "---------------------------------------\t<END>\n");
}

//---------------------------------------------------------------------------
void
Dag::PrintNodeList(Node::status_t status) const
{
	for (auto & node : _nodes) {
		if(node->GetStatus() == status) {
			node->Dump(this);
		}
	}
	dprintf(D_ALWAYS, "---------------------------------------\t<END>\n");
}

//---------------------------------------------------------------------------
void
Dag::PrintReadyQ(debug_level_t level) const {
	if (DEBUG_LEVEL(level)) {
		dprintf(D_ALWAYS, "Ready Queue: ");
		if (_readyQ->empty()) {
			dprintf(D_ALWAYS | D_NOHEADER, "<empty>\n");
			return;
		}

		bool first = true;
		for (const auto& node : *_readyQ) {
			if (first) {
				first = false;
				dprintf(D_ALWAYS | D_NOHEADER, "%s", node->GetNodeName());
			} else {
				dprintf(D_ALWAYS | D_NOHEADER, ", %s", node->GetNodeName());
			}
		}
	}
}

//---------------------------------------------------------------------------
bool
Dag::FinishedRunning(bool includeFinalNode) const
{
	if (includeFinalNode && _final_node && !_finalNodeRun) {
		// Make sure we don't incorrectly return true if we get called
		// just before a final node is started...  (There is a race
		// condition here otherwise, because all of the "regular"
		// nodes can be done, and the final node not changed to
		// ready yet.)
		return false;
	} else if (IsHalted()) {
		// Note that we're checking for scripts actually running here,
		// not the number of nodes in the PRERUN or POSTRUN state --
		// if we're halted, we don't start any new PRE scripts.
		return (NumNodesSubmitted() == 0 && NumScriptsRunning() == 0);
	}

	return (NumNodesSubmitted() == 0 && NumNodesReady() == 0 && ScriptRunNodeCount() == 0);
}

//---------------------------------------------------------------------------
bool
Dag::DoneSuccess(bool includeFinalNode) const
{
	if ( ! FinishedRunning(includeFinalNode)) {
		// Note: if final node is running we should get to here...
		return false;
	} else if (NumNodesDone(includeFinalNode) == NumNodes(includeFinalNode)) {
		// This is the normal case.
		return true;
	} else if (includeFinalNode && _final_node && _final_node->GetStatus() == Node::STATUS_DONE) {
		// Final node can override the overall DAG status.
		return true;
	} else if (_dagIsAborted && _dagStatus == DAG_STATUS_OK) {
		// Abort-dag-on can override the overall DAG status.
		return true;
	}

	return false;
}

//---------------------------------------------------------------------------
bool
Dag::DoneFailed(bool includeFinalNode) const
{
	if ( ! FinishedRunning(includeFinalNode)) {
		// Note: if final node is running we should get to here...
		return false;
	} else if (includeFinalNode && _final_node && _final_node->GetStatus() == Node::STATUS_DONE) {
		// Success of final node overrides failure of any other node.
		return false;
	} else if (_dagIsAborted && _dagStatus == DAG_STATUS_OK) {
		// Abort-dag-on can override the overall DAG status.
		return false;
	} else if (IsHalted()) {
		return true;
	}

	return (NumNodesFailed() > 0);
}

//---------------------------------------------------------------------------
bool
Dag::DoneCycle(bool includeFinalNode) const
{
	return FinishedRunning(includeFinalNode) && !DoneSuccess(includeFinalNode) &&
	       NumNodesFailed() == 0 && !IsHalted() && !_dagIsAborted;
}

//---------------------------------------------------------------------------
int
Dag::NumNodes(bool includeFinal) const
{
	int result = _nodes.size();
	if (!includeFinal && HasFinalNode()) {
		result--;
	}
	return result;
}

//---------------------------------------------------------------------------
int
Dag::NumNodesDone(bool includeFinal) const
{
	int result = _numNodesDone;
	if (!includeFinal && HasFinalNode() && _final_node->GetStatus() == Node::STATUS_DONE) {
		result--;
	}

	return result;
}

//---------------------------------------------------------------------------
// Note: the HTCondor part of this method essentially duplicates functionality
// that is now in schedd.cpp.  We are keeping this here for now in case
// someone needs to run a 7.5.6 DAGMan with an older schedd.
// wenger 2011-01-26
// Note 2: We need to keep this indefinitely for the ABORT-DAG-ON case,
// where we need to condor_rm any running node jobs, and the schedd
// won't do it for us.  wenger 2014-10-29.
void Dag::RemoveRunningJobs(const CondorID &dmJobId, const std::string& reason, bool removeCondorJobs, bool bForce)
{
	if (bForce) { removeCondorJobs = true; }

	// Hmm -- should we check here if we have jobs queued? wenger 2014-12-17
	bool haveCondorJob = true;

	ArgList args;

	if (removeCondorJobs && haveCondorJob) {
		debug_printf(DEBUG_NORMAL, "Removing any/all submitted HTCondor jobs...\n");
		std::string constraint;

		args.Clear();
		args.AppendArg(_condorRmExe);
		args.AppendArg("-const");

		// NOTE: having whitespace in the constraint argument will cause quoting problems on windows
		formatstr(constraint, ATTR_DAGMAN_JOB_ID "==%d", dmJobId._cluster );
		args.AppendArg(constraint.c_str());
		if ( ! reason.empty()) {
			args.AppendArg("-reason");
			args.AppendArg(reason);
		}
		if (_dagmanUtils.popen(args) != 0) {
			debug_printf(DEBUG_NORMAL, "Error removing DAGMan jobs\n");
		}
	}

	return;
}

//---------------------------------------------------------------------------
void Dag::RemoveRunningScripts() const {
	for (auto & node : _nodes) {
		ASSERT(node != nullptr);

		// if node is running a PRE script, hard kill it
		if (node->GetStatus() == Node::STATUS_PRERUN) {
			if ( ! node->_scriptPre) {
				EXCEPT("Node %s has no PRE script!", node->GetNodeName());
			}
			if (node->_scriptPre->_pid != 0) {
				debug_printf(DEBUG_DEBUG_1, "Killing PRE script %d\n", node->_scriptPre->_pid);
				if (daemonCore->Shutdown_Fast(node->_scriptPre->_pid) == FALSE) {
					debug_printf(DEBUG_QUIET, "WARNING: shutdown_fast() failed on pid %d: %s\n",
					             node->_scriptPre->_pid, strerror(errno));
				}
			}
		} else if (node->GetStatus() == Node::STATUS_POSTRUN) {
			// if node is running a POST script, hard kill it
			if ( ! node->_scriptPost) {
				EXCEPT("Node %s has no POST script!", node->GetNodeName());
			}
			if (node->_scriptPost->_pid != 0) {
				debug_printf(DEBUG_DEBUG_1, "Killing POST script %d\n", node->_scriptPost->_pid);
				if (daemonCore->Shutdown_Fast(node->_scriptPost->_pid) == FALSE) {
					debug_printf(DEBUG_QUIET, "WARNING: shutdown_fast() failed on pid %d: %s\n",
					             node->_scriptPost->_pid, strerror(errno));
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
void Dag::Rescue(const char * dagFile, bool multiDags, int maxRescueDagNum, bool overwrite, bool parseFailed, bool isPartial) /* const */
{
	std::string rescueDagFile;
	std::string headerInfo;
	if (parseFailed) {
		rescueDagFile = dagFile;
		rescueDagFile += ".parse_failed";
		formatstr(headerInfo,"# \"Rescue\" DAG file, created after failure parsing\n#   the %s DAG file\n", dagFile);
	} else {
		int nextRescue = _dagmanUtils.FindLastRescueDagNum(dagFile, multiDags, maxRescueDagNum) + 1;
		if (overwrite && nextRescue > 1) {
			nextRescue--;
		}
		if (nextRescue > maxRescueDagNum) {
			nextRescue = maxRescueDagNum;
		}
		rescueDagFile = _dagmanUtils.RescueDagName(dagFile, multiDags, nextRescue);
		formatstr(headerInfo,"# Rescue DAG file, created after running\n#   the %s DAG file\n", dagFile);
	}

	// Note: there could possibly be a race condition here if two
	// DAGMans are running on the same DAG at the same time.  That
	// should be avoided by the lock file, though, so I'm not doing
	// anything about it right now.  wenger 2007-02-27

	WriteRescue(rescueDagFile.c_str(), headerInfo.c_str(), parseFailed, isPartial);
}

//-----------------------------------------------------------------------------
void Dag::WriteSavePoint(Node* node) {
	if (!node || !node->IsSavePoint()) { return; }
	auto [saveFile, success] = _dagmanUtils.ResolveSaveFile(dagOpts.primaryDag(), node->GetSaveFile(), true);
	if ( ! success) { return; }
	TmpDir tmpDir;
	std::string errMsg;
	//If using useDagDir then switch to directory to write save files
	//in case relative path was given
	if (dagOpts[deep::b::UseDagDir]) {
		if ( ! tmpDir.Cd2TmpDir(node->GetDirectory(), errMsg)) {
			debug_printf(DEBUG_QUIET, "Error: Failed to change to directory %s: %s\n", node->GetDirectory(), errMsg.c_str());
			debug_printf(DEBUG_QUIET, "       Not writing save file (%s) at node %s.\n", saveFile.c_str(), node->GetNodeName());
			return;
		}
	}
	//At the moment only keep one older iteration of save files marked as filename.old
	if (_dagmanUtils.fileExists(saveFile)) {
		std::string rotateName;
		formatstr(rotateName, "%s.old", saveFile.c_str());
		int r = remove(rotateName.c_str());
		if (r < 0) {
			// Continue anyway, and hope the rename will work...
			debug_printf(DEBUG_QUIET, "Warning: Unable to remove old save file: %s\n", rotateName.c_str());
		}
		r = rename(saveFile.c_str(), rotateName.c_str());
		if (r < 0) {
			debug_printf(DEBUG_QUIET, "Warning: Unable to rename save file: %s to %s\n", saveFile.c_str(), rotateName.c_str());
		}
	}
	//Write save file
	std::string headerInfo;
	formatstr(headerInfo, "# Save file written at Start Node %s\n", node->GetNodeName());
	WriteRescue(saveFile.c_str(), headerInfo.c_str(), false, true, true);
	if (dagOpts[deep::b::UseDagDir]) {
		if ( ! tmpDir.Cd2MainDir(errMsg)) {
			debug_printf(DEBUG_QUIET, "Error: Failed to change back to original directory: %s\n", errMsg.c_str());
		}
	}
}

static const char *RESCUE_DAG_VERSION = "2.0.1";

//-----------------------------------------------------------------------------
void Dag::WriteRescue(const char * rescue_file, const char * headerInfo, bool parseFailed, bool isPartial, bool isSavePoint) /* const */
{
	debug_printf(DEBUG_NORMAL, "Writing %s to %s...\n", isSavePoint ? "Save File" : "Rescue DAG", rescue_file);

	FILE *fp = safe_fopen_wrapper_follow(rescue_file, "w");
	if ( ! fp) {
		debug_printf(DEBUG_QUIET, "Could not open %s for writing.\n", rescue_file);
		return;
	}

	bool reset_retries_upon_rescue = true;
	if ( ! isSavePoint) {
		reset_retries_upon_rescue = param_boolean("DAGMAN_RESET_RETRIES_UPON_RESCUE", true);
	}

	fprintf(fp,"%s",headerInfo);

	time_t timestamp;
	(void)time(&timestamp);
	const struct tm *tm;
	tm = gmtime(&timestamp);
	fprintf(fp, "# Created %d/%d/%d %02d:%02d:%02d UTC\n", tm->tm_mon + 1, tm->tm_mday,
	        tm->tm_year + 1900, tm->tm_hour, tm->tm_min, tm->tm_sec);
	fprintf(fp, "# Rescue DAG version: %s (%s)\n", RESCUE_DAG_VERSION, isPartial ? "partial" : "full");

	fprintf(fp, "#\n");
	fprintf(fp, "# Total number of Nodes: %d\n", NumNodes(true));
	fprintf(fp, "# Nodes premarked DONE: %d\n", _numNodesDone);
	fprintf(fp, "# Nodes that failed: %d\n", _numNodesFailed);

	// Print the names of failed nodes
	fprintf(fp, "#   ");
	for (auto & node : _nodes) {
		if (node->GetStatus() == Node::STATUS_ERROR) {
			fprintf(fp, "%s,", node->GetNodeName());
		}
	}
	fprintf(fp, "<ENDLIST>\n\n");

	// REJECT tells DAGMan to reject this DAG if we try to run it (which we shouldn't).
	if (parseFailed && !isPartial) {
		fprintf(fp, "REJECT\n\n");
	}

	// Print the CONFIG file, if any.
	if (_configFile && !isPartial) {
		fprintf(fp, "CONFIG %s\n\n", _configFile);
	}

	// Print the node status file, if any.
	if (_statusFileName && !isPartial) {
		fprintf(fp, "NODE_STATUS_FILE %s\n\n", _statusFileName);
	}

	// Print the jobstate.log file, if any.
	if (_jobstateLog.LogFile() && !isPartial) {
		fprintf(fp, "JOBSTATE_LOG %s\n\n", _jobstateLog.LogFile());
	}

	// Print per-node information.
	for (auto & node : _nodes) {
		WriteNodeToRescue(fp, node, reset_retries_upon_rescue, isPartial);
	}

	// Print Dependency Section
	if ( ! isPartial) {
		fprintf(fp, "\n");
		for (auto & node : _nodes) {
			if ( ! node->NoChildren()) {
				fprintf(fp, "PARENT %s CHILD ", node->GetNodeName());

				node->VisitChildren(*this, [](Dag&, Node*, Node* child, void* pv) -> int {
						fprintf((FILE*)pv, " %s", child->GetNodeName());
						return 1;
					}, fp);
				fprintf(fp, "\n");
			}
		}
	}

	// Print "throttle by node category" settings.
	if ( ! isPartial) {
		_catThrottles.PrintThrottles(fp);
	}

	fclose(fp);
}

//-----------------------------------------------------------------------------
void
Dag::WriteNodeToRescue(FILE *fp, Node *node, bool reset_retries_upon_rescue, bool isPartial)
{
		// Print the JOB/DATA line.
	const char *keyword = "";
	if (node->GetType() == NodeType::FINAL) {
		keyword = "FINAL";
	} else {
		keyword = node->GetDagFile() ? "SUBDAG EXTERNAL" : "JOB";
	}

	if ( ! isPartial) {
		fprintf(fp, "\n%s %s %s ", keyword, node->GetNodeName(), node->GetDagFile() ? node->GetDagFile() : node->GetCmdFile());
		if (strcmp(node->GetDirectory(), "")) {
			fprintf(fp, "DIR %s ", node->GetDirectory());
		}
		if (node->GetNoop()) {
			fprintf(fp, "NOOP ");
		}
		fprintf(fp, "\n");

		// Print the SCRIPT PRE line, if any.
		if (node->_scriptPre != nullptr) {
			WriteScriptToRescue(fp, node->_scriptPre);
		}

		// Print the PRE_SKIP line, if any.
		if (node->HasPreSkip() != 0) {
			fprintf(fp, "PRE_SKIP %s %d\n", node->GetNodeName(), node->GetPreSkip());
		}

		// Print the SCRIPT POST line, if any.
		if (node->_scriptPost != nullptr) {
			WriteScriptToRescue(fp, node->_scriptPost);
		}

		// Print the VARS line, if any.
		if (node->HasVars()) {
			std::string vars;
			vars.reserve(500);
			vars = "";
			node->PrintVars(vars);
			fprintf(fp, "VARS %s", node->GetNodeName());
			fprintf(fp, "%s\n", vars.c_str());
		}

		// Print the ABORT-DAG-ON line, if any.
		if (node->have_abort_dag_val) {
			fprintf(fp, "ABORT-DAG-ON %s %d", node->GetNodeName(), node->abort_dag_val);
			if (node->have_abort_dag_return_val) {
				fprintf(fp, " RETURN %d", node->abort_dag_return_val);
			}
			fprintf(fp, "\n");
		}

		// Print the PRIORITY line, if any.
		// Note: when gittrac #2167 gets merged, we need to think
		// about how this code will interact with that code.
		// wenger/nwp 2011-08-24
		if (node->_explicitPriority != 0) {
			fprintf(fp, "PRIORITY %s %d\n", node->GetNodeName(), node->_explicitPriority);
		}

		// Print the CATEGORY line, if any.
		if (node->GetThrottleInfo()) {
			fprintf(fp, "CATEGORY %s %s\n", node->GetNodeName(), node->GetThrottleInfo()->_category->c_str());
		}
	}

	// Never mark a FINAL node as done.
	// Also avoid a possible race condition where the node
	// has been skipped but is not yet marked as DONE.
	if (node->GetStatus() == Node::STATUS_DONE && node->GetType() != NodeType::FINAL) {
		fprintf(fp, "DONE %s\n", node->GetNodeName());
	}

	// Print the RETRY line, if any.
	if (node->retry_max > 0) {
		int retriesLeft = (node->retry_max - node->retries);

		if (node->GetStatus() == Node::STATUS_ERROR && node->retries < node->retry_max &&
		    node->have_retry_abort_val && node->retval == node->retry_abort_val)
		{
			fprintf(fp, "# %d of %d retries performed; remaining attempts aborted after node returned %d\n", 
			        node->retries, node->retry_max, node->retval);
		} else {
			if ( ! reset_retries_upon_rescue) {
				fprintf(fp, "# %d of %d retries already performed; %d remaining\n",
				        node->retries, node->retry_max, retriesLeft);
			}
		}

		ASSERT(node->retries <= node->retry_max);
		if ( ! reset_retries_upon_rescue) {
			fprintf(fp, "RETRY %s %d", node->GetNodeName(), retriesLeft);
		} else {
			fprintf(fp, "RETRY %s %d", node->GetNodeName(), node->retry_max);
		}
		if (node->have_retry_abort_val) {
			fprintf(fp, " UNLESS-EXIT %d", node->retry_abort_val);
		}
		fprintf(fp, "\n");
	}
}

//-----------------------------------------------------------------------------
void
Dag::WriteScriptToRescue(FILE *fp, Script *script)
{
	const char *type = nullptr;
	switch(script->GetType()) {
		case ScriptType::PRE: type = "PRE"; break;
		case ScriptType::POST: type = "POST"; break;
		case ScriptType::HOLD: type = "HOLD"; break;
	}
	fprintf(fp, "SCRIPT ");
	if (script->_deferStatus != SCRIPT_DEFER_STATUS_NONE) {
		fprintf(fp, "DEFER %d %lld ", script->_deferStatus, (long long)script->_deferTime);
	}
	fprintf(fp, "%s %s %s\n", type, script->GetNode()->GetNodeName(), script->GetCmd());
}

//-------------------------------------------------------------------------
void
Dag::TerminateNode(Node* node, bool recovery, bool bootstrap)
{
	ASSERT( ! (recovery && bootstrap));
	ASSERT(node != nullptr);

	node->TerminateSuccess(); // marks node as STATUS_DONE
	if (node->GetStatus() != Node::STATUS_DONE) {
		EXCEPT("Node %s is not in DONE state", node->GetNodeName());
	}

	// If this was a service node, set the node as done and exit
	if (node->GetType() == NodeType::SERVICE) {
		_metrics->NodeFinished( node->GetDagFile() != nullptr, true);
		node->countedAsDone = true;
		node->SetProcEvent(node->GetProc(), ABORT_TERM_MASK);
		node->retval = 0;
		return;
	}

	// this is a little ugly, but since this function can be
	// called multiple times for the same node, we need to be
	// careful not to double-count...
	if (node->countedAsDone == false) {
		_numNodesDone++;
		_metrics->NodeFinished(node->GetDagFile() != nullptr, true);
		node->countedAsDone = true;
		ASSERT((unsigned int)_numNodesDone <= _nodes.size());
	} else {
		// do not update children again - the children have only a completion counter.
		// not a parent list, so we can only call ParentComplete() once per child.
		return;
	}

	// Report termination to all child nodes by removing parent's ID from
	// each child's waiting queue.
	if (bootstrap || recovery) {
		// notify children of parent completion, but don't start any nodes
		node->NotifyChildren(*this, nullptr);
	} else {
		// notify children of parent completion, and start any nodes that are no longer idle
		node->NotifyChildren(*this, [](Dag& dag, Node* child) -> bool {
				// this is invoked after child->ParentComplete(node) returns true
				if (child->GetStatus() == Node::STATUS_READY) {
					return dag.StartNode(child, false);
				} else {
					return false;
				}
			});
	}
}

//-------------------------------------------------------------------------
void Dag::PrintEvent(debug_level_t level, const ULogEvent* event, Node* node, bool recovery)
{
	ASSERT(event);

	const char *recovStr = recovery ? " [recovery mode]" : "";

	std::string timestr;
	// Be sure to pass GetEventTime() here, because we want the
	// event time to always be output has a human-readable string,
	// even if dprintf() is configured to print timestamps.
	time_to_str(event->GetEventclock(), timestr);
	// String from time_to_str has trailing blank (needed for other
	// places in the code).
	trim(timestr);

	if (node) {
		debug_printf(level, "Event: %s for %s Node %s (%d.%d.%d) {%s}%s\n", event->eventName(), node->JobTypeString(),
		             node->GetNodeName(), event->cluster, event->proc, event->subproc, timestr.c_str(), recovStr);
	} else {
		debug_printf(level, "Event: %s for unknown Node (%d.%d.%d) {%s}: ignoring...%s\n", event->eventName(),
		             event->cluster, event->proc, event->subproc, timestr.c_str(), recovStr);
	}
}

//-------------------------------------------------------------------------
void
Dag::RestartNode(Node *node, bool recovery)
{
	ASSERT(node != nullptr);

	if (node->GetStatus() != Node::STATUS_ERROR) {
		EXCEPT("Node %s is not in ERROR state", node->GetNodeName());
	}

	if (_finalNodeRun || ( node->have_retry_abort_val && node->retval == node->retry_abort_val)) {
		const char *finalRun = _finalNodeRun ? "because final node is running " : "";
		debug_printf(DEBUG_NORMAL, "Aborting further retries of node %s %s(last attempt returned %d)\n",
		             node->GetNodeName(), finalRun, node->retval);
		if (node->GetType() != NodeType::SERVICE) {
			_numNodesFailed++;
		}
		_metrics->NodeFinished(node->GetDagFile() != nullptr, false);
		if (_dagStatus == DAG_STATUS_OK) {
			_dagStatus = DAG_STATUS_NODE_FAILED;
		}
		return;
	}

	node->SetStatus(Node::STATUS_READY);
	node->retries++;
	node->ResetJobInfo();
	ASSERT(node->GetRetries() <= node->GetRetryMax());
	if (node->_scriptPre) {
		// undo PRE script completion
		node->_scriptPre->_done = false;
	}
	node->error_text = "";
	node->ResetJobstateSequenceNum();

	if ( ! recovery) {
		debug_printf(DEBUG_VERBOSE, "Retrying node %s (retry #%d of %d)...\n",
		             node->GetNodeName(), node->GetRetries(), node->GetRetryMax());
		StartNode(node, true);
	} else {
		debug_printf(DEBUG_VERBOSE, "Looking for retry of node %s (retry #%d of %d)...\n",
		             node->GetNodeName(), node->GetRetries(), node->GetRetryMax());

		// Remove the "old" Condor ID from the ID->node hash table
		// here to fix gittrac #1957.
		// Note: the if checking against the default condor ID
		// should *always* be true here, but checking just to be safe.
		if (node->GetID() != _defaultCondorId) {
			int id = GetIndexID(node->GetID());
			if (GetEventIDHash(node->GetNoop())->erase(id) != 1) {
				EXCEPT("Event ID hash table error!");
			}
		}

		// Doing this fixes gittrac #481 (recovery fails on a DAG that
		// has retried nodes).  (See SubmitNodeJob() for where this
		// gets done during "normal" running.)
		node->SetCondorID(_defaultCondorId);
	}
}

//-------------------------------------------------------------------------
// Number the nodes according to DFS order 
void 
Dag::DFSVisit(Node * node, int depth)
{
	//Check whether node has been numbered already
	if ( ! node || node->_visited) { return; }
	
	//Remember that the node has been numbered	
	node->_visited = true;

	// the height of a node is the minumum recursion depth at which we first visited it
	// the height of the graph is the maximum of the node depths
	_graph_height = MAX(_graph_height, depth);

	// with width is the sum of nodes at a given depth, the overall width is the max of those
	_graph_widths[depth] += 1;
	_graph_width = MAX(_graph_width, _graph_widths[depth] += 1);
	
	// visit the children of the current node, marking their first visitation order
	if ( ! node->NoChildren()) {
		int plus_one = depth + 1;
		while ((int)_graph_widths.size() <= plus_one) { _graph_widths.push_back(0); }

		node->VisitChildren(*this, [](Dag& dag, Node* /*parent*/, Node* child, void* pv) -> int {
				dag.DFSVisit(child, *(int*)pv);
				return 1;
			},
			&plus_one);
	}

	DFS_ORDER++;
	node->_dfsOrder = DFS_ORDER;
}

//-------------------------------------------------------------------------
// Detects cycle and warns user about it
bool 
Dag::isCycle ()
{
	bool cycle = false; 

	//Start DFS numbering from zero, although not necessary
	DFS_ORDER = 0; 
	// as a side effect of DFS cycle detection, will we determine width and height of the graph
	_graph_width = 0;
	_graph_height = 0;
	_graph_widths.clear();
	_graph_widths.push_back(0);

	//Visit all nodes in DAG and number them	
	for (auto & node : _nodes) {
		if (!node->_visited && node->NoParents())
			DFSVisit(node, 0);
	}

	//Detect cycle
	for (auto & node : _nodes) {
		if (node->VisitChildren(*this, [](Dag&, Node* parent, Node* child, void*) -> int {
				if (child->_dfsOrder >= parent->_dfsOrder) {
		#ifdef REPORT_CYCLE
					debug_printf(DEBUG_QUIET, "Cycle in the graph possibly involving nodes %s and %s\n",
					             parent->GetNodeName(), child->GetNodeName());
		#endif
					return 1; // increment the cycle count
				}
				return 0;
			}, nullptr)) {
			// the return value of VisitChildren is the number of children with _dfsOrder
			// greater that that of their parents.  If *any* have this, then we have a cycle.
			cycle = true;
		}
	}
	return cycle;
}

//-------------------------------------------------------------------------
bool
Dag::CheckForDagAbort(Node *node, const char *type)
{
	if (node->DoAbort()) {
		debug_printf(DEBUG_QUIET, "Aborting DAG because we got the ABORT exit value from a %s\n", type);
		_dagIsAborted = true;

		if (node->have_abort_dag_return_val) {
			main_shutdown_rescue(node->abort_dag_return_val, node->abort_dag_return_val != 0 ? DAG_STATUS_ABORT : DAG_STATUS_OK);
		} else {
			main_shutdown_rescue(node->retval, DAG_STATUS_ABORT);
		}
		return true;
	}

	return false;
}

//-------------------------------------------------------------------------
// Sets the base name of the dot file name. If we aren't 
// overwriting the file, it will have a number appended to it.
void 
Dag::SetDotFileName(const char *dot_file_name)
{
	free(_dot_file_name);
	_dot_file_name = strdup(dot_file_name);
	return;
}

//-------------------------------------------------------------------------
// Sets the name of a file that we'll take the contents from and
// place them into the dot file. The idea is that if someone wants
// to set some parameters for the graph but doesn't want to manually
// edit the file (perhaps a higher-level program is watching the 
// dot file) then we'll get the parameters from here.
void
Dag::SetDotIncludeFileName(const char *include_file_name)
{
	free(_dot_include_file_name);
	_dot_include_file_name = strdup(include_file_name);
	return;
}

//-------------------------------------------------------------------------
// Called whenever the dot file should be created. 
// This writes to the _dot_file_name, although the name may have
// a numeric suffix if we're not overwriting files. 
void 
Dag::DumpDotFile(void)
{
	if (_dot_file_name != nullptr) {
		std::string current_dot_file_name;
		std::string temp_dot_file_name;
		FILE *temp_dot_file;

		ChooseDotFileName(current_dot_file_name);

		temp_dot_file_name = current_dot_file_name + ".temp";

		_dagmanUtils.tolerant_unlink(temp_dot_file_name);
		temp_dot_file = safe_fopen_wrapper_follow(temp_dot_file_name.c_str(), "w");
		if (temp_dot_file == nullptr) {
			debug_dprintf(D_ALWAYS, DEBUG_NORMAL, "Can't create dot file '%s'\n", temp_dot_file_name.c_str());
		} else {
			time_t current_time;
			char   *time_string;
			char   *newline;

			time(&current_time);
			time_string = ctime(&current_time);
			newline = strchr(time_string, '\n');
			if (newline != nullptr) {
				*newline = 0;
			}
			
			fprintf(temp_dot_file, "digraph DAG {\n");
			fprintf(temp_dot_file, "    label=\"DAGMan Job status at %s\";\n\n", time_string);

			IncludeExtraDotCommands(temp_dot_file);

			// We sweep the node list twice, just to make a nice
			// looking DOT file. 
			// The first sweep sets the appearance of each node.
			// The second sweep describes all of the arcs.

			DumpDotFileNodes(temp_dot_file);
			DumpDotFileArcs(temp_dot_file);

			fprintf(temp_dot_file, "}\n");
			fclose(temp_dot_file);
			// Note:  we do tolerant_unlink because renaming over an
			// existing file fails on Windows.
			_dagmanUtils.tolerant_unlink(current_dot_file_name);
			if (rename(temp_dot_file_name.c_str(), current_dot_file_name.c_str()) != 0) {
				debug_printf(DEBUG_NORMAL, "Warning: can't rename temporary dot file (%s) to permanent file (%s): %s\n",
				             temp_dot_file_name.c_str(), current_dot_file_name.c_str(), strerror(errno ));
				check_warning_strictness(DAG_STRICT_1);
			}
		}
	}
	return;
}

/** Set the filename of the node status file.
	@param the filename to which to dump node status information
	@param the minimum interval, in seconds, at which to update the
		status file (0 means no limit)
*/
void 
Dag::SetNodeStatusFileName(const char *statusFileName, int minUpdateTime, bool alwaysUpdate)
{
	if (_statusFileName) {
		debug_printf(DEBUG_NORMAL, "Warning: Attempt to set NODE_STATUS_FILE to %s does not override existing value of %s\n",
		             statusFileName, _statusFileName);
		check_warning_strictness(DAG_STRICT_3);
		return;
	}
	_statusFileName = strdup(statusFileName);
	_minStatusUpdateTime = minUpdateTime;
	_alwaysUpdateStatus = alwaysUpdate;
}

//-------------------------------------------------------------------------
/** Dump the node status.
	@param whether the DAG has just been held
	@param whether the DAG has just been removed
*/
// Note:  We might eventually want to change this to actually creating
// classad objects and calling the unparser on them.  (See gittrac
// #4316.)  wenger 2014-04-16
void
Dag::DumpNodeStatus(bool held, bool removed)
{
	// Decide whether to update the file.
	if (_statusFileName == nullptr) { return; }
	
	if (!_alwaysUpdateStatus && !_statusFileOutdated && !held && !removed) {
		debug_printf(DEBUG_DEBUG_1, "Node status file not updated because it is not yet outdated\n");
		return;
	}
	
	time_t startTime = time(nullptr);
	bool tooSoon = (_minStatusUpdateTime > 0) && ((startTime - _lastStatusUpdateTimestamp) < _minStatusUpdateTime);
	if (tooSoon && !held && !removed && !FinishedRunning(true) && !_dagIsAborted) {
		debug_printf(DEBUG_DEBUG_1, "Node status file not updated because min. status update time has not yet passed\n");
		return;
	}

	// If we made it to here, we want to actually update the
	// file.  We do that by actually writing to a temporary file,
	// and then renaming that to the "real" file, so that the
	// "real" file is always complete.
	debug_printf(DEBUG_DEBUG_1, "Updating node status file\n");

	std::string tmpStatusFile(_statusFileName);
	tmpStatusFile += ".tmp";
	// Note: it's not an error if this fails (file may not exist).
	_dagmanUtils.tolerant_unlink(tmpStatusFile);

	FILE *outfile = safe_fopen_wrapper_follow(tmpStatusFile.c_str(), "w");
	if (outfile == nullptr) {
		debug_printf(DEBUG_NORMAL, "Warning: can't create node status file '%s': %s\n",
		             tmpStatusFile.c_str(), strerror(errno));
		check_warning_strictness(DAG_STRICT_1);
		return;
	}

	fprintf(outfile, "[\n");
	fprintf(outfile, "  Type = \"DagStatus\";\n");

	// Print DAG file list.
	fprintf(outfile, "  DagFiles = {\n");
	const char *separator = "";
	for (auto & _dagFile : dagOpts.dagFiles()) {
		fprintf(outfile, "%s    %s", separator, EscapeClassadString(_dagFile.c_str()));
		separator = ",\n";
	}
	fprintf(outfile, "\n  };\n");

	// Print timestamp.
	std::string timeStr = ctime(&startTime);
	chomp(timeStr);
	fprintf(outfile, "  Timestamp = %lu; /* %s */\n", (unsigned long)startTime, EscapeClassadString(timeStr.c_str()));

	// If markNodesError is true, this means that we want to mark
	// nodes in the PRERUN, SUBMITTED, and POSTRUN states as being
	// in the ERROR state.  This is because if, for example, we're
	// condor_rm'ed, we won't see the ABORTED events for node jobs
	// that get removed, but we're *assuming* they will get removed.
	// (See gittrac #4686.)  We only want to do this the very last
	// time we dump out the node status file, to make sure that we
	// don't erroneously mark a running final node (if we have one)
	// as finished unsuccessfully.
	bool markNodesError = false;

	// Print overall DAG status.
	Node::status_t dagJobStatus = Node::STATUS_SUBMITTED;
	const char *statusNote = "";

	if (DoneSuccess(true)) {
		dagJobStatus = Node::STATUS_DONE;
		statusNote = "success";
		// In case we have a combination of abort-dag-on and final
		// node, and we get here before seeing ABORTED events for
		// node jobs that got removed.
		markNodesError = true;

	} else if (DoneFailed(true)) {
		dagJobStatus = Node::STATUS_ERROR;
		if (_dagStatus == DAG_STATUS_ABORT) {
			statusNote = "aborted";
			markNodesError = true;
		} else {
			statusNote = "failed";
			// In case we're halted and nodes are waiting for
			// deferred PRE or POST scripts.
			markNodesError = true;
		}

	} else if (DoneCycle(true)) {
		dagJobStatus = Node::STATUS_ERROR;
		statusNote = "cycle";

	} else if (held) {
		statusNote = "held";

	} else if (removed) {
		if (HasFinalNode() && !FinishedRunning(true)) {
			dagJobStatus = Node::STATUS_SUBMITTED;
			statusNote = "removed";
		} else {
			dagJobStatus = Node::STATUS_ERROR;
			statusNote = "removed";
			markNodesError = true;
		}
	} else if (_dagIsAborted) {
		if (HasFinalNode() && !FinishedRunning(true)) {
			dagJobStatus = Node::STATUS_SUBMITTED;
			statusNote = "aborted";
		} else {
			dagJobStatus = Node::STATUS_ERROR;
			statusNote = "aborted";
			markNodesError = true;
		}
	}

	std::string statusStr = Node::status_t_names[dagJobStatus];
	trim(statusStr);
	statusStr += " (";
	statusStr += statusNote;
	statusStr += ")";
	fprintf(outfile, "  DagStatus = %d; /* %s */\n", dagJobStatus, EscapeClassadString(statusStr.c_str()));

	int nodesPre = PreRunNodeCount();
	int nodesQueued = NumNodesSubmitted();
	int nodesPost = PostRunNodeCount();
	int nodesFailed = NumNodesFailed();
	int nodesHeld = 0, nodesIdle = 0;
	NumJobProcStates(&nodesHeld,&nodesIdle);
	if (markNodesError) {
		// Adjust state counts to reflect "pending" removes of node jobs, etc.
		nodesFailed += nodesPre;
		nodesPre = 0;
		nodesFailed += nodesQueued;
		nodesQueued = 0;
		nodesFailed += nodesPost;
		nodesPost = 0;
		nodesHeld = 0;
		nodesIdle = 0;
	}
	fprintf(outfile, "  NodesTotal = %d;\n", NumNodes(true));
	fprintf(outfile, "  NodesDone = %d;\n", NumNodesDone(true));
	fprintf(outfile, "  NodesPre = %d;\n", nodesPre);
	fprintf(outfile, "  NodesQueued = %d;\n", nodesQueued);
	fprintf(outfile, "  NodesPost = %d;\n", nodesPost);
	fprintf(outfile, "  NodesReady = %d;\n", NumNodesReady());
	fprintf(outfile, "  NodesUnready = %d;\n",NumNodesUnready(true));
	fprintf(outfile, "  NodesFutile = %d;\n", NumNodesFutile());
	fprintf(outfile, "  NodesFailed = %d;\n", nodesFailed);
	fprintf(outfile, "  JobProcsHeld = %d;\n", nodesHeld);
	fprintf(outfile, "  JobProcsIdle = %d; /* includes held */\n", nodesIdle);
	fprintf(outfile, "]\n");

	// Print status of all nodes.
	for (auto & node : _nodes) {
		fprintf(outfile, "[\n");
		fprintf(outfile, "  Type = \"NodeStatus\";\n");

		int jobProcsQueued = node->_queuedNodeJobProcs;
		int jobProcsHeld = node->_jobProcsOnHold;

		Node::status_t status = node->GetStatus();
		const char *nodeNote = "";
		if (status == Node::STATUS_READY) {
			// Note:  Node::STATUS_READY only means that the job is
			// ready to submit if it doesn't have any unfinished
			// parents.
			if (!node->CanSubmit()) {
				status = Node::STATUS_NOT_READY;
			}

		} else if (status == Node::STATUS_SUBMITTED) {
			if (markNodesError) {
				status = Node::STATUS_ERROR;
				nodeNote = "Was STATUS_SUBMITTED";
				jobProcsQueued = 0;
				jobProcsHeld = 0;
			} else {
				// This isn't really the right thing to do for multi-
				// proc nodes, but I want to get in a fix for
				// gittrac #5333 today...  wenger 2015-11-05
				nodeNote = node->GetProcIsIdle(0) ? "idle" : "not_idle";
				// Note: add info here about whether the job(s) are
				// held, once that code is integrated.
			}

		} else if (status == Node::STATUS_ERROR) {
			nodeNote = node->error_text.c_str();

		} else if (status == Node::STATUS_PRERUN) {
			if (markNodesError) {
				status = Node::STATUS_ERROR;
				nodeNote = "Was STATUS_PRERUN";
			}

		} else if (status == Node::STATUS_POSTRUN) {
			if (markNodesError) {
				status = Node::STATUS_ERROR;
				nodeNote = "Was STATUS_POSTRUN";
			}
		} else if (status == Node::STATUS_FUTILE) {
			nodeNote = "Had an ancestor node fail";
		} else if (status == Node::STATUS_DONE && node->IsPreDone()) {
			nodeNote = "User defined as DONE";
		}

		fprintf(outfile, "  Node = %s;\n", EscapeClassadString(node->GetNodeName()));
		statusStr = Node::status_t_names[status];
		trim(statusStr);
		fprintf(outfile, "  NodeStatus = %d; /* %s */\n", status, EscapeClassadString(statusStr.c_str()));
		// fprintf( outfile, "  /* HTCondorStatus = xxx; */\n" );
		fprintf(outfile, "  StatusDetails = %s;\n", EscapeClassadString(nodeNote));
		fprintf(outfile, "  RetryCount = %d;\n", node->GetRetries());
		// fprintf( outfile, "  /* JobProcsTotal = xxx; */\n" );
		fprintf(outfile, "  JobProcsQueued = %d;\n", jobProcsQueued);
		// fprintf( outfile, "  /* JobProcsRunning = xxx; */\n" );
		// fprintf( outfile, "  /* JobProcsIdle = xxx; */\n" );
		fprintf(outfile, "  JobProcsHeld = %d;\n", jobProcsHeld);

		fprintf(outfile, "]\n");
	}

	// Print end information.
	fprintf(outfile, "[\n");
	fprintf(outfile, "  Type = \"StatusEnd\";\n");

	time_t endTime = time(nullptr);
	timeStr = ctime(&endTime);
	chomp(timeStr);
	fprintf(outfile, "  EndTime = %lu; /* %s */\n", (unsigned long)endTime, EscapeClassadString(timeStr.c_str()));

	time_t nextTime;
	if (FinishedRunning(true) || removed) {
		nextTime = 0;
		timeStr = "none";
	} else {
		nextTime = endTime + _minStatusUpdateTime;
		timeStr = ctime(&nextTime);
		chomp(timeStr);
	}
	fprintf(outfile, "  NextUpdate = %lu; /* %s */\n", (unsigned long)nextTime, EscapeClassadString(timeStr.c_str()));
	fprintf(outfile, "]\n");

	fclose(outfile);

	// Now rename the temporary file to the "real" file.
	std::string statusFileName(_statusFileName);

#ifdef WIN32
	//Note: We do tolerant_unlink because renaming over an existing file fails on Windows.
	_dagmanUtils.tolerant_unlink(statusFileName.c_str());
#endif

	if (rename(tmpStatusFile.c_str(), statusFileName.c_str()) != 0) {
		debug_printf(DEBUG_NORMAL, "Warning: can't rename temporary node status file (%s) to permanent file (%s): %s\n",
		             tmpStatusFile.c_str(), statusFileName.c_str(), strerror(errno));
		check_warning_strictness(DAG_STRICT_1);
		return;
	}

	_statusFileOutdated = false;
	_lastStatusUpdateTimestamp = startTime;
}

//-------------------------------------------------------------------------
const char *
Dag::EscapeClassadString(const char* strIn)
{
	static classad::Value tmpValue;
	static std::string tmpStr; // must be static so we can return c_str()
	static classad::ClassAdUnParser unparse;

	tmpValue.SetStringValue(strIn);
	tmpStr = "";
	unparse.Unparse(tmpStr, tmpValue);

	return tmpStr.c_str();
}

//---------------------------------------------------------------------------
bool Dag::MonitorLogFile() {
	std::string nodesLog = DefaultNodeLog();
	debug_printf(DEBUG_DEBUG_2, "Attempting to monitor log file <%s>\n", nodesLog.c_str());

	CondorError errstack;
	if ( ! _condorLogRdr.monitorLogFile(nodesLog, !_recovery, errstack)) {
		errstack.pushf("DAGMan::MonitorLogFile", DAGMAN_ERR_LOG_FILE, "ERROR: Unable to monitor log file <%s>\n",
		               nodesLog.c_str());
		debug_printf(DEBUG_QUIET, "%s\n", errstack.getFullText().c_str());
		EXCEPT("Fatal log file monitoring error!");
	}

	return true;
}

//---------------------------------------------------------------------------
bool Dag::UnmonitorLogFile() {
	std::string nodesLog = DefaultNodeLog();
	debug_printf(DEBUG_DEBUG_2, "Unmonitoring log file <%s>\n", nodesLog.c_str());

	CondorError errstack;
	if ( ! _condorLogRdr.unmonitorLogFile(nodesLog, errstack)) {
		errstack.pushf("DAGMan::UnmonitorLogFile", DAGMAN_ERR_LOG_FILE, "ERROR: Unable to unmonitor log file <%s>\n",
		               nodesLog.c_str());
		debug_printf(DEBUG_QUIET, "%s\n", errstack.getFullText().c_str());
		EXCEPT("Fatal log file monitoring error!");
	}

	return true;
}

//-------------------------------------------------------------------------
void
Dag::SetReject(const std::string &location)
{
	if (_firstRejectLoc.empty()) { _firstRejectLoc = location; }
	_reject = true;
}

//-------------------------------------------------------------------------
bool
Dag::GetReject(std::string &firstLocation)
{
	firstLocation = _firstRejectLoc;
	return _reject;
}

//-------------------------------------------------------------------------
void 
Dag::EnforceNewJobsLimit() {
	// Remove Node batch jobs that exceed new MaxJobs Limit
	int submittedJobsCount = 0;
	for (auto & node : _nodes) {
		if (node->GetStatus() == Node::STATUS_SUBMITTED) {
			submittedJobsCount++;
			if (submittedJobsCount > dagOpts[shallow::i::MaxJobs]) {
				node->retry_max++;
				std::string rm_reason = "DAG Limit: Max number of submitted nodes was reached.";
				RemoveBatchJob(node, rm_reason);
			}
		}
	}
}

//-------------------------------------------------------------------------
/** Set the filename of the jobstate.log file.
	@param the filename to which to write the jobstate log
*/
void 
Dag::SetJobstateLogFileName(const char *logFileName)
{
	if (_jobstateLog.LogFile() != nullptr) {
		debug_printf(DEBUG_NORMAL, "Warning: Attempt to set JOBSTATE_LOG to %s does not override existing value of %s\n",
		             logFileName, _jobstateLog.LogFile());
		check_warning_strictness(DAG_STRICT_3);
		return;
	}
	_jobstateLog.SetLogFile(logFileName);
}

//-------------------------------------------------------------------------
// Called after the DAG has finished to check for any
// inconsistency in the job events we've gotten.
void
Dag::CheckAllJobs()
{
	CheckEvents::check_event_result_t result;
	std::string jobError;

	result = _checkCondorEvents.CheckAllJobs(jobError);
	if (result == CheckEvents::EVENT_ERROR) {
		debug_printf(DEBUG_QUIET, "Error checking HTCondor job events: %s\n", jobError.c_str());
		ASSERT(false);
	} else if (result == CheckEvents::EVENT_BAD_EVENT || result == CheckEvents::EVENT_WARNING) {
		debug_printf(DEBUG_NORMAL, "Warning checking HTCondor job events: %s\n", jobError.c_str());
		check_warning_strictness(DAG_STRICT_3);
	} else {
		debug_printf(DEBUG_DEBUG_1, "All HTCondor job events okay\n");
	}
}

//-------------------------------------------------------------------------
void
Dag::NumJobProcStates(int* n_held, int* n_idle, int* n_running, int* n_terminated)
{
	//These are total counters
	int held = 0, idle = 0, run = 0, term = 0;
	//These are per node counters
	int node_held = 0, numProcs = 0;
	
	//For each node in the dag look a job proc events vector
	for (auto & node : _nodes) {
		//Reset state counters
		node_held = 0;
		numProcs = node->GetProcEventsSize();
		//For each Job Proc event
		for (int i=0; i < numProcs; i++) {
			//Get unsigned char representing the event bitmap
			//and check states
			const unsigned char procEvent = node->GetProcEvent(i);
			if ((procEvent & HOLD_MASK) != 0) { held++; node_held++; }
			else if ((procEvent & IDLE_MASK) != 0) { idle++; }
			else if ((procEvent & ABORT_TERM_MASK) != 0) { term++; }
			else { run++; }
		}
		
		//Perform some sanity checks per Node
		if (node_held != node->_jobProcsOnHold) { //Held job procs check
			debug_printf( DEBUG_NORMAL,
						"Warning: Number of counted held job processes (%d) is not equivalent to Node %s's internal count of held job processes count (%d).\n",
						held, node->GetNodeName(), node->_jobProcsOnHold);
		}
		
	}
	
	//Perform whole DAG sanity checks
	//Internal DAG counts held job procs as idle
	if ((idle+held) != NumIdleJobProcs()) { //Idle job procs check
		debug_printf( DEBUG_NORMAL,
					"Warning: Number of counted idle job processes (%d) is not equivalent to DAGs internal idle job processes count (%d).\n",
					idle, NumIdleJobProcs());
	}
	//If passed counter then set to totals found in DAG
	if (n_held) { *n_held = held; }
	if (n_idle) { *n_idle = idle; }
	if (n_terminated) { *n_terminated = term; }
	if (n_running) { *n_running = run; }
}
	
//-------------------------------------------------------------------------
int
Dag::NumHeldJobProcs()
{
	int numHeldProcs = 0;
	for (auto & node : _nodes) {
		numHeldProcs += node->_jobProcsOnHold;
	}
	return numHeldProcs;
}

//-------------------------------------------------------------------------
void
Dag::PrintDeferrals(debug_level_t level, bool force) const
{
	// This function should never be called when this dag object is acting like
	// a splice. 
	ASSERT(_isSplice == false);

	if (_maxJobsDeferredCount > 0 || force) {
		debug_printf(level, "Note: %d total node deferrals because of -MaxJobs limit (%d)\n",
		             _maxJobsDeferredCount, dagOpts[shallow::i::MaxJobs]);
	}

	if (_maxIdleDeferredCount > 0 || force) {
		debug_printf(level, "Note: %d total node deferrals because of -MaxIdle limit (%d)\n",
		             _maxIdleDeferredCount, dagOpts[shallow::i::MaxIdle]);
	}

	if (_catThrottleDeferredCount > 0 || force) {
		debug_printf(level, "Note: %d total node deferrals because of node category throttles\n", _catThrottleDeferredCount);
	}

	if (_preScriptQ->GetScriptDeferredCount() > 0 || force) {
		debug_printf(level, "Note: %d total PRE script deferrals because of -MaxPre limit (%d) or DEFER\n",
		             _preScriptQ->GetScriptDeferredCount(), dagOpts[shallow::i::MaxPre]);
	}

	if (_postScriptQ->GetScriptDeferredCount() > 0 || force) {
		debug_printf(level, "Note: %d total POST script deferrals because of -MaxPost limit (%d) or DEFER\n",
		             _postScriptQ->GetScriptDeferredCount(), dagOpts[shallow::i::MaxPost]);
	}

	if (_holdScriptQ->GetScriptDeferredCount() > 0 || force) {
		debug_printf(level, "Note: %d total HOLD script deferrals because of -MaxHold limit (%d) or DEFER\n",
		             _holdScriptQ->GetScriptDeferredCount(), dagOpts[shallow::i::MaxHold]);
	}
}

//---------------------------------------------------------------------------
void
Dag::PrintPendingNodes() const
{
	dprintf( D_ALWAYS, "Pending DAG nodes:\n" );

	for (auto & node : _nodes) {
		switch (node->GetStatus()) {
		case Node::STATUS_PRERUN:
		case Node::STATUS_SUBMITTED:
		case Node::STATUS_POSTRUN:
			dprintf(D_ALWAYS, "  Node %s, HTCondor ID %d, status %s\n",
			        node->GetNodeName(), node->GetCluster(), node->GetStatusName());
			break;

		default:
			// No op.
			break;
		}
	}
}

//---------------------------------------------------------------------------
void
Dag::SetPendingNodeReportInterval(int interval)
{
	_pendingReportInterval = interval;
	time(&_lastEventTime);
	time(&_lastPendingNodePrintTime);
}

//-------------------------------------------------------------------------
void
Dag::CheckThrottleCats()
{
	for (const auto& throttle: *_catThrottles.GetThrottles()) {
		ThrottleByCategory::ThrottleInfo *info = throttle.second;
		debug_printf(DEBUG_DEBUG_1, "Category %s has %d jobs, throttle setting of %d\n",
		             info->_category->c_str(), info->_totalJobs, info->_maxJobs);
		ASSERT(info->_totalJobs >= 0);
		if (info->_totalJobs < 1) {
			debug_printf(DEBUG_NORMAL,
			             "Warning: category %s has no assigned nodes, so the throttle setting (%d) will have no effect\n",
			             info->_category->c_str(), info->_maxJobs);
			check_warning_strictness(DAG_STRICT_2);
		}

		if ( ! info->isSet()) {
			debug_printf(DEBUG_NORMAL, "Warning: category %s has no throttle value set\n", info->_category->c_str());
			check_warning_strictness(DAG_STRICT_2);
		}
	}
}

//-------------------------------------------------------------------------
// Helper function for DumpDotFile. Reads the _dot_include_file_name
// file and places everything in it into the dot file that is being written.
void
Dag::IncludeExtraDotCommands(FILE *dot_file)
{
	FILE *include_file;

	include_file = safe_fopen_wrapper_follow(_dot_include_file_name, "r");
	if (include_file == nullptr) {
		if (_dot_include_file_name) {
			debug_printf(DEBUG_NORMAL, "Can't open dot include file %s\n", _dot_include_file_name);
		}
	} else {
		char line[100];
		fprintf(dot_file, "// Beginning of commands included from %s.\n", _dot_include_file_name);
		while (fgets(line, 100, include_file) != nullptr) {
			fputs(line, dot_file);
		}
		fprintf(dot_file, "// End of commands included from %s.\n\n",  _dot_include_file_name);
		fclose(include_file);
	}
	return;
}

//-------------------------------------------------------------------------
// Helper function for DumpDotFile. Prints one line for each 
// node in the graph. This line describes how the node should
// be drawn, and it's based on the state of the node. In particular,
// we draw different shapes for each node type, but also add
// a short description of the state, to make it easily readable by
// people that are familiar with HTCondor job states. 
void 
Dag::DumpDotFileNodes(FILE *temp_dot_file)
{
	for (auto & node : _nodes) {
		const char *node_name;
		
		node_name = node->GetNodeName();
		switch (node->GetStatus()) {
		case Node::STATUS_READY:
			fprintf(temp_dot_file, "    \"%s\" [shape=ellipse label=\"%s (I)\"];\n",
			        node_name, node_name);
			break;
		case Node::STATUS_PRERUN:
			fprintf(temp_dot_file, "    \"%s\" [shape=ellipse label=\"%s (Pre)\" style=dotted];\n",
			        node_name, node_name);
			break;
		case Node::STATUS_SUBMITTED:
			fprintf(temp_dot_file, "    \"%s\" [shape=ellipse label=\"%s (R)\" peripheries=2];\n",
			        node_name, node_name);
			break;
		case Node::STATUS_POSTRUN:
			fprintf(temp_dot_file, "    \"%s\" [shape=ellipse label=\"%s (Post)\" style=dotted];\n",
			        node_name, node_name);
			break;
		case Node::STATUS_DONE:
			fprintf(temp_dot_file, "    \"%s\" [shape=ellipse label=\"%s (Done)\" style=bold];\n",
			        node_name, node_name);
			break;
		case Node::STATUS_ERROR:
			fprintf(temp_dot_file, "    \"%s\" [shape=box label=\"%s (E)\"];\n",
			        node_name, node_name);
			break;
		default:
			fprintf(temp_dot_file, "    \"%s\" [shape=ellipse label=\"%s (I)\"];\n",
			        node_name, node_name);
			break;
		}
	}

	fprintf(temp_dot_file, "\n");

	return;
}

//-------------------------------------------------------------------------
// Helper function for DumpDotFile. This prints one line for each
// arc that is in the DAG.
void 
Dag::DumpDotFileArcs(FILE *temp_dot_file)
{
	for (auto & node : _nodes) {
		if (node->GetNodeName()) {
			node->VisitChildren(*this, [](Dag&, Node* parent, Node* child, void* pv) -> int {
					FILE* fp = (FILE*)pv;
					const char * child_name = child->GetNodeName();
					if (child_name) {
						fprintf(fp, "    \"%s\" -> \"%s\";\n", parent->GetNodeName(), child_name);
					}
					return 1;
				},
				temp_dot_file);
		}
	}
	
	return;
}

//-------------------------------------------------------------------------
// If we're overwriting the dot file, the name is exactly as
// the user specified it. If we're not overwriting, we need to 
// choose a unique name. We choose one of the form name.number, 
// where the number is the first unused we find. If the user 
// creates a bunch of dot files (foo.0-foo.100) and deletes
// some in the middle (foo.50), then this could confuse the user
// because we'll fill in the middle. We don't worry about it though.
void 
Dag::ChooseDotFileName(std::string &dot_file_name)
{
	if (_overwrite_dot_file) {
		dot_file_name = _dot_file_name;
	} else {
		bool found_unused_file = false;

		while ( ! found_unused_file) {
			FILE *fp;
			formatstr(dot_file_name, "%s.%d", _dot_file_name, _dot_file_name_suffix);
			fp = safe_fopen_wrapper_follow(dot_file_name.c_str(), "r");
			if (fp) {
				fclose(fp);
				_dot_file_name_suffix++;
			} else {
				found_unused_file = true;
			}
		}
		_dot_file_name_suffix++;
	}
	return;
}

//---------------------------------------------------------------------------
bool Dag::Add(Node& node)
{
	auto insertJobResult = _nodeNameHash.insert(std::make_pair(node.GetNodeName(), &node));
	ASSERT(insertJobResult.second == true);
	auto insertIdResult = _nodeIDHash.insert(std::make_pair( node.GetNodeID(), &node));
	ASSERT(insertIdResult.second == true);

	// Final node status is set to STATUS_NOT_READY here, so it
	// won't get run even though it has no parents; its status
	// will get changed when it should be run.
	if (node.GetType() == NodeType::FINAL) {
		if (_final_node) {
			debug_printf(DEBUG_QUIET, "Error: DAG already has a final node %s; attempting to add final node %s\n",
			             _final_node->GetNodeName(), node.GetNodeName() );
			return false;
		}
		node.SetStatus(Node::STATUS_NOT_READY);
		_final_node = &node;
	}

	if (node.GetType() == NodeType::PROVISIONER) {
		if (_provisioner_node) {
			debug_printf(DEBUG_QUIET, "Error: DAG already has a provisioner node %s; attempting to add provisioner node %s\n",
			             _provisioner_node->GetNodeName(), node.GetNodeName());
			return false;
		}
		_provisioner_node = &node;
	}

	if (node.GetType() == NodeType::SERVICE) {
		_service_nodes.push_back(&node);
		// Service nodes do not get included in the _nodes list
		return true;
	}

	_nodes.push_back(&node);
	return true;
}

//---------------------------------------------------------------------------
Node*
Dag::LogEventNodeLookup(const ULogEvent* event, bool &submitEventIsSane)
{
	ASSERT(event);
	submitEventIsSane = false;

	Node *node = nullptr;
	CondorID condorID(event->cluster, event->proc, event->subproc);

	// if this is a job whose submit event we've already seen, we
	// can simply use the job ID to look up the corresponding DAG
	// node, and we're done...

	if (event->eventNumber != ULOG_SUBMIT && event->eventNumber != ULOG_PRESKIP) {
		node = FindNodeByEventID(condorID);
		if (node) { return node; }
	}

	// if the job ID wasn't familiar and we didn't find a node
	// above, there are at least four possibilites:
	//
	// 1) it's the submit event for a node we just submitted, and
	// we don't yet know the job ID; in this case, we look up the
	// node name in the event body. Alternately if we are in recovery
	// mode we don't yet know the job ID.
	//
	// 2) it's the POST script terminated event for a job whose
	// submission attempts all failed, leaving it with no valid
	// job ID at all (-1.-1.-1).  Here we also look up the node
	// name in the event body.
	//
	// 3) it's a job submitted by someone outside than this
	// DAGMan, and can/should be ignored (we return NULL).
	//
	// 4) it's a pre skip event, which is handled similarly to
	// a submit event.
	//
	// 5) it's a cluster submit event, which is handled similarly to
	// a submit event.
	if (event->eventNumber == ULOG_SUBMIT) {
		const SubmitEvent* submit_event = (const SubmitEvent*)event;
		if ( ! submit_event->submitEventLogNotes.empty()) {
			char nodeName[1024] = "";
			if (sscanf(submit_event->submitEventLogNotes.c_str(), "DAG Node: %1023s", nodeName) == 1) {
				node = FindNodeByName(nodeName);
				if (node) {
					submitEventIsSane = SanityCheckSubmitEvent(condorID, node);
					node->SetCondorID(condorID);

					// Insert this node into the CondorID->node hash
					// table if we don't already have it (e.g., recovery
					// mode).  (In "normal" mode we should have already
					// inserted it when we did the condor_submit.)
					bool isNoop = NodeIsNoop(condorID);
					ASSERT(isNoop == node->GetNoop());
					int id = GetIndexID(condorID);
					std::map<int, Node*> *ht = GetEventIDHash(isNoop);
					auto findResult = ht->find(id);
					if (findResult == ht->end()) {
						// Node not found.
						auto insertResult = ht->insert(std::make_pair(id, node));
						ASSERT(insertResult.second == true);
					} else {
						// Node was found.
						ASSERT((*findResult).second == node);
					}
				}
			} else {
				debug_printf(DEBUG_QUIET, "ERROR: 'DAG Node:' not found in submit event notes: <%s>\n",
				             submit_event->submitEventLogNotes.c_str());
			}
		}
		return node;
	}

	if (event->eventNumber == ULOG_POST_SCRIPT_TERMINATED && event->cluster == -1) {
		const PostScriptTerminatedEvent* pst_event = (const PostScriptTerminatedEvent*)event;
		node = FindNodeByName(pst_event->dagNodeName.c_str());
		return node;
	}
	
	if (event->eventNumber == ULOG_PRESKIP) {
		const PreSkipEvent* skip_event = (const PreSkipEvent*)event;
		char nodeName[1024] = "";
		if (skip_event->skipEventLogNotes.empty()) {
			debug_printf(DEBUG_NORMAL, "No DAG Node indicated in a PRE_SKIP event\n");	
			node = nullptr;
		} else if (sscanf(skip_event->skipEventLogNotes.c_str(), "DAG Node: %1023s", nodeName) == 1) {
			node = FindNodeByName(nodeName);
			if (node) {
				node->SetCondorID(condorID);
				// Insert this node into the CondorID->node hash table.
				bool isNoop = NodeIsNoop(condorID);
				int id = GetIndexID(condorID);
				std::map<int, Node*> *ht = GetEventIDHash(isNoop);
				auto findResult = ht->find(id);
				if (findResult == ht->end()) {
					// Node not found.
					auto insertResult = ht->insert(std::make_pair(id, node));
					ASSERT(insertResult.second == true);
				} else {
					// Node was found.
					ASSERT((*findResult).second == node);
				}
			}
		} else {
			debug_printf(DEBUG_QUIET, "ERROR: 'DAG Node:' not found in skip event notes: <%s>\n",
			             skip_event->skipEventLogNotes.c_str());
		}
		return node;
	}

	if (event->eventNumber == ULOG_CLUSTER_SUBMIT) {
		const ClusterSubmitEvent* cluster_submit_event = (const ClusterSubmitEvent*)event;
		if ( ! cluster_submit_event->submitEventLogNotes.empty()) {
			char nodeName[1024] = "";
			if (sscanf(cluster_submit_event->submitEventLogNotes.c_str(), "DAG Node: %1023s", nodeName) == 1) {
				node = FindNodeByName(nodeName);
				if (node) {
					submitEventIsSane = SanityCheckSubmitEvent(condorID, node);
					node->SetCondorID( condorID );

					// Insert this node into the CondorID->node hash
					// table if we don't already have it (e.g., recovery
					// mode).  (In "normal" mode we should have already
					// inserted it when we did the condor_submit.)
					bool isNoop = NodeIsNoop(condorID);
					ASSERT(isNoop == node->GetNoop());
					int id = GetIndexID(condorID);
					std::map<int, Node*> *ht = GetEventIDHash(isNoop);
					auto findResult = ht->find(id);
					// std::map::find() returns an iterator pointing to the desired element, or end() if not found
					if (findResult == ht->end()) {
						// Node not found.
						auto insertResult = ht->insert(std::make_pair(id, node));
						// std::map::insert() returns a pair, second element is the success bool
						ASSERT(insertResult.second == true);
					} else {
						// Node was found.
						ASSERT((*findResult).second == node);
					}
				}
			} else {
				debug_printf(DEBUG_QUIET, "ERROR: 'DAG Node:' not found in cluster submit event notes: <%s>\n",
				             cluster_submit_event->submitEventLogNotes.c_str());
			}
		}
		return node;
	}
	return node;
}


//---------------------------------------------------------------------------
// Checks whether this is a "good" event ("bad" events include HTCondor
// sometimes writing a terminated/aborted pair instead of just
// aborted, and the "submit once, run twice" bug).  Extra abort events
// we just ignore; other bad events will abort the DAG unless
// configured to continue.
//
// Note: we only check an event if we have a node object for it, so
// that we don't pay any attention to unrelated "garbage" events that
// may be in the log(s).
//
// Note: the event checking has pretty much been considered
// "diagnostic" until now; however, it turns out that if you get the
// terminated/abort event combo on a node that has retries left,
// DAGMan will assert if the event checking is turned off.  (See Gnats
// PR 467.)  wenger 2005-04-08.
//
// returns true if the event is sane and false if it is "impossible";
// (additionally sets *result=false if DAG should be aborted)

bool
Dag::EventSanityCheck(const ULogEvent* event, const Node* node, bool* result)
{
	ASSERT(event);
	ASSERT(node);

	std::string eventError;
	CheckEvents::check_event_result_t checkResult = CheckEvents::EVENT_OKAY;

	checkResult = _checkCondorEvents.CheckAnEvent(event, eventError);

	if (checkResult == CheckEvents::EVENT_OKAY) {
		debug_printf(DEBUG_DEBUG_1, "Event is okay\n");
		return true;
	}

	debug_printf(DEBUG_NORMAL, "%s\n", eventError.c_str());
	//debug_printf( DEBUG_NORMAL, "WARNING: bad event here may indicate a serious bug in HTCondor -- beware!\n" );

	if (checkResult == CheckEvents::EVENT_WARNING) {
		debug_printf(DEBUG_NORMAL, "BAD EVENT is warning only\n");
		return true;
	}

	if (checkResult == CheckEvents::EVENT_BAD_EVENT) {
		debug_printf(DEBUG_NORMAL, "Continuing with DAG in spite of bad event (%s) because of allow_events setting\n",
		             eventError.c_str());
	
		// Don't do any further processing of this event,
		// because it can goof us up (e.g., decrement count
		// of running jobs when we shouldn't).
		return false;
	}

	if (checkResult == CheckEvents::EVENT_ERROR) {
		debug_printf(DEBUG_QUIET, "ERROR: aborting DAG because of bad event (%s)\n", eventError.c_str());
		// set *result to indicate we should abort the DAG
		*result = false;
		return false;
	}

	debug_printf(DEBUG_QUIET, "Illegal CheckEvents::check_event_allow_t value: %d\n", checkResult);
	return false;
}


//---------------------------------------------------------------------------
// compares a submit event's job ID with the one that appeared earlier
// in the submit command's stdout (which we stashed in the node object)

bool
Dag::SanityCheckSubmitEvent(const CondorID condorID, const Node* node) const
{
	// Changed this if from "if( recovery )" during work on PR 806 --
	// this is better because if you get two submit events for the
	// same node you'll still get a warning in recovery mode.
	// wenger 2007-01-31.
	if (node->GetID() == _defaultCondorId) {
		// we no longer have the submit command stdout to check against
		return true;
	}

	if (condorID._cluster == node->GetCluster()) {
		return true;
	}

	std::string message;
	formatstr(message,
	          "ERROR: node %s: job ID in userlog submit event (%d.%d.%d) doesn't match ID reported earlier by submit command (%d.%d.%d)!", 
	          node->GetNodeName(), condorID._cluster, condorID._proc, condorID._subproc, node->GetCluster(), node->GetProc(), node->GetSubProc());

	if (_abortOnScarySubmit) {
		debug_printf(DEBUG_QUIET,
		             "%s  Aborting DAG; set DAGMAN_ABORT_ON_SCARY_SUBMIT to false if you are *sure* this shouldn't cause an abort.\n",
		             message.c_str());
		main_shutdown_rescue(EXIT_ERROR, DAG_STATUS_ERROR);
		return true;
	} else {
		debug_printf(DEBUG_QUIET,
		             "%s  Trusting the userlog for now (because of DAGMAN_ABORT_ON_SCARY_SUBMIT setting), but this is scary!\n",
		             message.c_str() );
	}
	return false;
}

//---------------------------------------------------------------------------
std::map<int, Node*>*
Dag::GetEventIDHash(bool isNoop)
{
	if (isNoop) { return &_noopIDHash; }
	return &_condorIDHash;
}

//---------------------------------------------------------------------------
const std::map<int, Node*>*
Dag::GetEventIDHash(bool isNoop) const
{
	if (isNoop) { return &_noopIDHash; }
	return &_condorIDHash;
}

//---------------------------------------------------------------------------
Dag::submit_result_t
Dag::SubmitNodeJob(const Dagman &dm, Node *node, CondorID &condorID)
{
	submit_result_t result = SUBMIT_RESULT_NO_SUBMIT;

	// Resetting the HTCondor ID here fixes PR 799.  wenger 2007-01-24.
	if (node->GetCluster() != _defaultCondorId._cluster) {
		// Remove the "previous" HTCondor ID for this node from
		// the ID->node hash table.
		int id = GetIndexID(node->GetID());
		int removeResult = GetEventIDHash(node->GetNoop())->erase(id);
		// std::map::erase() returns the number of elements erased
		ASSERT(removeResult == 1);
	}
	node->SetCondorID(_defaultCondorId);

		// sleep for a specified time before submitting
	if (dm.submit_delay != 0) {
		debug_printf(DEBUG_VERBOSE, "Sleeping for %d s (DAGMAN_SUBMIT_DELAY) to throttle submissions...\n",
		             dm.submit_delay);
		sleep(dm.submit_delay);
	}

	// Do condor_submit_dag -no_submit if this is a nested DAG node
	// and lazy submit file generation is enabled (this must be
	// done before we try to monitor the log file).
	if ( ! node->GetNoop() && node->GetDagFile() != nullptr && _generateSubdagSubmits) {
		bool isRetry = node->GetRetries() > 0;
		if (_dagmanUtils.runSubmitDag(dm.inheritOpts, node->GetDagFile(), node->GetDirectory(), node->_effectivePriority, isRetry) != 0) {
			++node->_submitTries;
			debug_printf(DEBUG_QUIET, "ERROR: condor_submit_dag -no_submit failed for node %s.\n", node->GetNodeName());
			// Hmm -- should this be a node failure, since it probably
			// won't work on retry?  wenger 2010-03-26
			return SUBMIT_RESULT_NO_SUBMIT;
		}
	}

	debug_printf(DEBUG_NORMAL, "Submitting %s Node %s job(s)...\n", node->JobTypeString(), node->GetNodeName());

	bool submit_success = false;
	std::string logFile = DefaultNodeLog();

 	node->_submitTries++;
	if (node->GetNoop()) {
		submit_success = fake_condor_submit(condorID, 0, node->GetNodeName(), node->GetDirectory(), logFile.c_str());
	} else {
		submit_success = condor_submit(dm, node, condorID);
	}

	result = submit_success ? SUBMIT_RESULT_OK : SUBMIT_RESULT_FAILED;

	return result;
}

//---------------------------------------------------------------------------
void
Dag::ProcessSuccessfulSubmit(Node *node, const CondorID &condorID)
{
	// Set the times to *not* wait before trying another submit, since
	// the most recent submit worked.
	_nextSubmitTime = 0;
	_nextSubmitDelay = 1;

	// append node to the submit queue so we can match it with its
	// submit event once the latter appears in the HTCondor job log
	_submitQ->push(node);

	node->SetStatus(Node::STATUS_SUBMITTED);

	// Note: I assume we're incrementing this here instead of when
	// we see the submit events so that we don't accidentally exceed
	// maxjobs (now really maxnodes) if it takes a while to see
	// the submit events.  wenger 2006-02-10.
	UpdateNodeCounts(node, 1);


	// stash the job ID reported by the submit command, to compare
	// with what we see in the userlog later as a sanity-check
	// (note: this sanity-check is not possible during recovery,
	// since we won't have seen the submit command stdout...)
	node->SetCondorID(condorID);
	ASSERT(NodeIsNoop(node->GetID()) == node->GetNoop());
	int id = GetIndexID(node->GetID());
	auto result = GetEventIDHash(node->GetNoop())->insert(std::make_pair(id, node));
	// std::map::insert() returns a pair, second element is the success bool
	ASSERT(result.second == true);

	debug_printf(DEBUG_VERBOSE, "\tassigned %s ID (%d.%d.%d)\n", node->JobTypeString(),
	             condorID._cluster, condorID._proc, condorID._subproc);
}

//---------------------------------------------------------------------------
void
Dag::ProcessFailedSubmit(Node *node, int max_submit_attempts)
{
	// This function should never be called when the Dag object is being used
	// to parse a splice.
	ASSERT(_isSplice == false);

	_jobstateLog.WriteSubmitFailure(node);

	// Flag the status file as outdated so it gets updated soon.
	_statusFileOutdated = true;

	// Set the times to wait twice as long as last time.
	int thisSubmitDelay = _nextSubmitDelay;
	_nextSubmitTime = time(nullptr) + thisSubmitDelay;
	_nextSubmitDelay *= 2;

	if (_dagStatus == DagStatus::DAG_STATUS_RM && node->GetType() != NodeType::FINAL) {
		max_submit_attempts = std::min(max_submit_attempts, 2);
	}

	if (node->_submitTries >= max_submit_attempts) {
		// We're out of submit attempts, treat this as a submit failure.
		// To match the existing behavior, we're resetting the times here.
		_nextSubmitTime = 0;
		_nextSubmitDelay = 1;

		debug_printf(DEBUG_QUIET, "Node submit failed after %d tr%s.\n",
		             node->_submitTries, node->_submitTries == 1 ? "y" : "ies");

		formatstr(node->error_text, "Job submit failed");

		// NOTE: this failure short-circuits the "retry" feature
		// because it's already exhausted a number of retries
		// (maybe we should make sure max_submit_attempts >
		// node->retries before assuming this is a good idea...)
		debug_printf(DEBUG_NORMAL, "Shortcutting node %s retries because of submit failure(s)\n", node->GetNodeName());
		node->retries = node->GetRetryMax();
		bool ranPostScript = false;
		if (node->_scriptPost) {
			node->_scriptPost->_retValJob = DAG_ERROR_CONDOR_SUBMIT_FAILED;
			ranPostScript = RunPostScript(node, dagOpts[shallow::b::PostRun], 0);
		} else {
			node->TerminateFailure();
			// We consider service nodes to be best-effort
			// Do not count failures in the overall dag count
			if (node->GetType() != NodeType::SERVICE) {
				_numNodesFailed++;
			}
			_metrics->NodeFinished(node->GetDagFile() != nullptr, false);
			if (_dagStatus == DAG_STATUS_OK) {
				_dagStatus = DAG_STATUS_NODE_FAILED;
			}
		}
		//If no post script ran then set all descendants to Futile
		if ( ! ranPostScript) { _numNodesFutile += node->SetDescendantsToFutile(*this); }
	} else {
		// We have more submit attempts left, put this node back into the
		// ready queue.
		debug_printf(DEBUG_NORMAL, "Job submit try %d/%d failed, will try again in >= %d second%s.\n",
		             node->_submitTries, max_submit_attempts, thisSubmitDelay, thisSubmitDelay == 1 ? "" : "s");

		if (m_retrySubmitFirst) {
			_readyQ->prepend(node);
		} else {
			_readyQ->append(node);
		}
	}
}

//---------------------------------------------------------------------------
void
Dag::DecrementProcCount(Node *node)
{
	node->_queuedNodeJobProcs--;
	ASSERT(node->_queuedNodeJobProcs >= 0);

	if (node->AllProcsDone()) {
		UpdateNodeCounts(node, -1);
		node->Cleanup();
	}
}

//---------------------------------------------------------------------------
void
Dag::UpdateNodeCounts(Node *node, int change)
{
	// Service nodes are separate from normal nodes
	if (node->GetType() == NodeType::SERVICE) { return; }
	_numNodesSubmitted += change;
	ASSERT(_numNodesSubmitted >= 0);

	if (node->GetThrottleInfo()) {
		node->GetThrottleInfo()->_currentJobs += change;
		ASSERT(node->GetThrottleInfo()->_currentJobs >= 0);
	}
}

//---------------------------------------------------------------------------
void
Dag::PropagateDirectoryToAllNodes(void)
{
	if (m_directory == ".") { return; }

	// Propagate the directory setting to all nodes in the DAG.
	for (auto & node : _nodes) {
		node->PrefixDirectory(m_directory);
	}

	// I wipe out m_directory here. If this gets called multiple
	// times for a specific DAG, it'll prefix multiple times, and that is most
	// likely wrong.

	m_directory = ".";
}

//-------------------------------------------------------------------------
bool
Dag::SetPinInOut(bool isPinIn, const char *nodeName, int pinNum)
{
	debug_printf(DEBUG_DEBUG_1, "Dag(%s)::SetPinInOut(%d, %s, %d)\n",
	             _spliceScope.c_str(), isPinIn, nodeName, pinNum );

	ASSERT(pinNum > 0);

	Node *node = FindNodeByName(nodeName);
	if ( ! node) {
		debug_printf(DEBUG_QUIET, "ERROR: node %s not found!\n", nodeName);
		return false;
	}

	bool result = false;
	if (isPinIn) {
		result = SetPinInOut(_pinIns, node, pinNum);
	} else {
		result = SetPinInOut(_pinOuts, node, pinNum);
	}

	return result;
}

//---------------------------------------------------------------------------
bool
Dag::SetPinInOut(PinList &pinList, Node *node, int pinNum)
{
	--pinNum; // Pin numbers start with 1
	if (pinNum >= static_cast<int>(pinList.size())) {
		pinList.resize(pinNum+1, nullptr);
	}
	PinNodes *pn = pinList[pinNum];
	if ( ! pn) {
		pinList[pinNum] = new PinNodes();
		pn = pinList[pinNum];
	}
	pn->push_back(node);

	return true;
}

//---------------------------------------------------------------------------
const Dag::PinNodes*
Dag::GetPinInOut(bool isPinIn, int pinNum) const
{
	debug_printf(DEBUG_DEBUG_1, "Dag(%s)::GetPinInOut(%d, %d)\n",
	             _spliceScope.c_str(), isPinIn, pinNum);

	ASSERT(pinNum > 0);

	const PinNodes *pn;
	if (isPinIn) {
		pn = GetPinInOut(_pinIns, "in", pinNum);
	} else {
		pn = GetPinInOut(_pinOuts, "out", pinNum);
	}

	return pn;
}

//---------------------------------------------------------------------------
const Dag::PinNodes*
Dag::GetPinInOut(const PinList &pinList, const char *inOutStr, int pinNum)
{
	--pinNum; // Pin numbers start with 1
	if (pinNum >= static_cast<int>(pinList.size())) {
		debug_printf(DEBUG_QUIET, "ERROR: pin %s number %d specified; max is %d\n",
		             inOutStr, pinNum+1, static_cast<int>( pinList.size()));
		return nullptr;
	} else {
		return pinList[pinNum];
	}
}

//---------------------------------------------------------------------------
int
Dag::GetPinCount(bool isPinIn)
{
	if (isPinIn) {
		return (int)_pinIns.size();
	} else {
		return (int)_pinOuts.size();
	}
}

//---------------------------------------------------------------------------
bool
Dag::ConnectSplices(Dag *parentSplice, Dag *childSplice)
{
	debug_printf(DEBUG_DEBUG_1, "Dag::ConnectSplices(%s, %s)\n",
	             parentSplice->_spliceScope.c_str(), childSplice->_spliceScope.c_str());

	std::string parentName = parentSplice->_spliceScope;
	// Trim trailing '+' from parentName.
	int last = parentName.length() - 1;
	ASSERT(last >= 0);
	if (parentName[last] == '+') {
		parentName = parentName.substr(0, last);
	}

	std::string childName = childSplice->_spliceScope;
	// Trim trailing '+' from childName.
	last = childName.length() - 1;
	ASSERT(last >= 0);
	if (childName[last] == '+') {
		childName = childName.substr(0, last);
	}

	// Make sure the parent and child splices have pin_ins/pin_outs
	// as appropriate, and that the number of pin_ins and pin_outs matches.
	int pinOutCount = parentSplice->GetPinCount(false);
	if (pinOutCount <= 0) {
		debug_printf(DEBUG_QUIET, "ERROR: parent splice %s has 0 pin_outs\n",
		             parentName.c_str());
		return false;
	}

	int pinInCount = childSplice->GetPinCount(true);
	if (pinInCount <= 0) {
		debug_printf(DEBUG_QUIET, "ERROR: child splice %s has 0 pin_ins\n",
		             childName.c_str());
		return false;
	}

	if (pinOutCount != pinInCount) {
		debug_printf(DEBUG_QUIET,
		             "ERROR: pin_in/out mismatch:  parent splice %s has %d pin_outs; child splice %s has %d pin_ins\n",
		             parentName.c_str(), pinOutCount, childName.c_str(), pinInCount);
		return false;
	}

	std::string failReason;

	// Go thru the pin_in/pin_out lists, and add parent/child
	// dependencies between splices as appropriate.  (Note that
	// we will catch any missing pin_in/pin_out numbers here.)
	for (int pinNum = 1; pinNum <= pinOutCount; ++pinNum) {
		const PinNodes *parentPNs = parentSplice->GetPinInOut(false, pinNum);
		if ( ! parentPNs) {
			debug_printf(DEBUG_QUIET, "ERROR: parent splice %s has no node for pin_out %d\n",
			             parentName.c_str(), pinNum);
			return false;
		}

		const PinNodes *childPNs = childSplice->GetPinInOut(true, pinNum);
		if ( ! childPNs) {
			debug_printf(DEBUG_QUIET, "ERROR: child splice %s has no node for pin_in %d\n",
			             childName.c_str(), pinNum);
			return false;
		}

		for (auto parentNode : *parentPNs) {
				for (auto childNode : *childPNs) {
					std::forward_list<Node*> lst = { childNode };
				if ( ! parentNode->AddChildren(lst, failReason)) {
					debug_printf(DEBUG_QUIET, "ERROR: unable to add parent/child dependency for pin %d\n", pinNum);
					return false;
				}
			}
		}
	}

	// Check for "orphan" nodes in the child splice -- nodes that
	// don't have either a parent within the splice or a pin_in
	// connection.
	for (auto & node : childSplice->_nodes) {
		if (node->NoParents()) {
			debug_printf(DEBUG_QUIET,
						"ERROR: child splice node %s has no parents after making pin connections; add pin_in or parent\n",
						node->GetNodeName());
			return false;
		}
	}

	return true;
}

//---------------------------------------------------------------------------
void
Dag::PrefixAllNodeNames(const std::string &prefix)
{
	std::string key;

	debug_printf(DEBUG_DEBUG_1, "Entering: Dag::PrefixAllNodeNames() with prefix %s\n", prefix.c_str());

	for (auto & node : _nodes) {
		node->PrefixName(prefix);
	}

	// Here we must reindex the hash view with the prefixed name.
	// also fix my node name hash view of the nodes

	// First, wipe out the index.
	auto it = _nodeNameHash.begin();
	while (it != _nodeNameHash.end()) {
		it = _nodeNameHash.erase(it);
	}

	// Then, reindex all the nodes keyed by their new name
	for (auto & node : _nodes) {
		key = node->GetNodeName();
		auto insertResult = _nodeNameHash.insert(std::make_pair(key, node));
		if (insertResult.second != true) {
			// I'm reinserting everything newly, so this should never happen
			// unless two nodes have an identical name, which means another
			// part o the code failed to keep the constraint that all nodes have
			// unique names
			debug_error(1, DEBUG_QUIET,  "Dag::PrefixAllNodeNames(): This is an impossible error\n");
		}
	}
	debug_printf(DEBUG_DEBUG_1, "Leaving: Dag::PrefixAllNodeNames()\n");
}

//---------------------------------------------------------------------------
bool 
Dag::InsertSplice(std::string spliceName, Dag *splice_dag)
{
	auto insertResult = _splices.insert(std::make_pair(spliceName, splice_dag));
	return insertResult.second;
}

//---------------------------------------------------------------------------
Dag*
Dag::LookupSplice(std::string name)
{
	auto findResult = _splices.find(name);
	if (findResult == _splices.end()) {
		return nullptr;
	}
	return (*findResult).second;
}

//---------------------------------------------------------------------------
// This represents not the actual initial nodes of the dag just after
// the file containing the dag had been parsed.
// You must NOT free the returned array or the contained pointers.
std::vector<Node*>*
Dag::InitialRecordedNodes(void)
{
	return &_splice_initial_nodes;
}

//---------------------------------------------------------------------------
// This represents not the actual final nodes of the dag just after
// the file containing the dag had been parsed.
// You must NOT free the returned array or the contained pointers.
std::vector<Node*>*
Dag::FinalRecordedNodes(void)
{
	return &_splice_terminal_nodes;
}

//---------------------------------------------------------------------------
// After we parse the dag, let's remember which ones were the initial and 
// terminal nodes in the dag (in case this dag was used as a splice).
void
Dag::RecordInitialAndTerminalNodes(void)
{
	for (auto & node : _nodes) {

		// record the initial nodes
		if (node->NoParents()) {
			_splice_initial_nodes.push_back(node);
		}

		// record the final nodes
		if (node->NoChildren()) {
			_splice_terminal_nodes.push_back(node);
		}
	}
}

//---------------------------------------------------------------------------
// Make a copy of the _nodes array and return it. This will also remove the
// nodes out of _nodes so the destructor doesn't delete them.
OwnedMaterials*
Dag::RelinquishNodeOwnership(void)
{
	// 1. Copy the nodes
	auto *nodes = new std::vector<Node*>(_nodes.begin(),_nodes.end());
	_nodes.clear();

	// shove it into a packet and give it back
	return new OwnedMaterials(nodes, &_catThrottles, _reject, _firstRejectLoc);
}

//---------------------------------------------------------------------------
OwnedMaterials*
Dag::LiftSplices(SpliceLayer layer)
{
	//PrintNodeList();
	OwnedMaterials *om = nullptr;

	// if this splice contains no other splices, then relinquish the nodes I own
	if (layer == DESCENDENTS && _splices.size() == 0) {
		return RelinquishNodeOwnership();
	}

	// recurse down the splice tree moving everything up into myself.
	for (auto& splice: _splices) {
		debug_printf(DEBUG_DEBUG_1, "Lifting splice %s\n", splice.first.c_str());
		om = splice.second->LiftSplices(DESCENDENTS);
		// this function moves what it needs out of the returned object
		AssumeOwnershipofNodes(splice.first, om);
		delete om;
	}

	// Now delete all of them.
	for (auto &nv_pair : _splices) {
		delete nv_pair.second;
	}
	_splices.clear();

	ASSERT( _splices.size() == 0 );

	// and prefix them if there was a DIR for the dag.
	PropagateDirectoryToAllNodes();

	// base case is above.
	return nullptr;
}

//-------------------------------------------------------------------------
void
Dag::AdjustEdges()
{
	for (auto & node : _nodes) {
		node->BeginAdjustEdges(this);
	}
	for (auto & node : _nodes) {
		node->AdjustEdges(this);
	}
	for (auto & node : _nodes) {
		node->FinalizeAdjustEdges(this);
	}
}

//---------------------------------------------------------------------------
// Grab all of the nodes out of the splice, and place them into here.
// If the splice, after parsing of the dag file representing 'here', still
// have true initial or final nodes, then those must move over the the
// recorded inital and final nodes for 'here'.
void
Dag::AssumeOwnershipofNodes(const std::string &spliceName, OwnedMaterials *om)
{
	Node *node = nullptr;
	unsigned int i;
	std::string key;
	NodeID_t key_id;

	std::vector<Node*> *nodes = om->nodes;

	// 0. Take ownership of the categories

	// Merge categories from the splice into this DAG object.  If the
	// same category exists in both (whether global or non-global) the
	// higher-level value overrides the lower-level value.

	// Note: by the time we get to here, all category names have already
	// been prefixed with the proper scope.
	ThrottleByCategory::ThrottleInfo *spliceThrottle;
	for (const auto& throttle: *om->throttles->GetThrottles()) {
		spliceThrottle = throttle.second;
		ThrottleByCategory::ThrottleInfo *mainThrottle = _catThrottles.GetThrottleInfo(spliceThrottle->_category);
		if (mainThrottle && mainThrottle->isSet() && mainThrottle->_maxJobs != spliceThrottle->_maxJobs) {
			debug_printf(DEBUG_NORMAL,
			             "Warning: higher-level (%s) maxjobs value of %d for category %s overrides splice %s value of %d\n",
			             _spliceScope.c_str(), mainThrottle->_maxJobs, mainThrottle->_category->c_str(), spliceName.c_str(),
			             spliceThrottle->_maxJobs);
			check_warning_strictness(DAG_STRICT_2);
		} else {
			_catThrottles.SetThrottle(spliceThrottle->_category, spliceThrottle->_maxJobs);
		}
	}

	// 1. Take ownership of the nodes

	// 1a. If there are any actual initial/final nodes, then ensure to record
	// it into the recorded initial and final nodes for this node.
	for (i = 0; i < nodes->size(); i++) {
		if ((*nodes)[i]->NoParents()) {
			_splice_initial_nodes.push_back((*nodes)[i]);
			continue;
		}
		if ((*nodes)[i]->NoChildren()) {
			_splice_terminal_nodes.push_back((*nodes)[i]);
		}
	}

	// 1. Take ownership of the nodes
	// 1b. Re-set the node categories (if any) so they point to the
	// ThrottleByCategory object in *this* DAG rather than the splice
	// DAG (which will be deleted soon).
	for (i = 0; i < nodes->size(); i++) {
		Node *tmpNode = (*nodes)[i];
		spliceThrottle = tmpNode->GetThrottleInfo();
		if (spliceThrottle) {
			// Now re-set the category in the node, so that the
			// category info points to the upper DAG rather than the splice DAG.
			tmpNode->SetCategory(spliceThrottle->_category->c_str(), _catThrottles);
		}
	}

	// 1c. Copy the nodes into _nodes.
	for (i = 0; i < nodes->size(); i++) {
		_nodes.push_back((*nodes)[i]);
	}

	// 2. Update our name hash to include the new nodes.
	for (i = 0; i < nodes->size(); i++) {
		key = (*nodes)[i]->GetNodeName();

		debug_printf(DEBUG_DEBUG_1, "Creating view hash fixup for: node %s\n", key.c_str());

		auto insertResult = _nodeNameHash.insert(std::make_pair(key, (*nodes)[i]));
		if (insertResult.second == false) {
			debug_printf(DEBUG_QUIET,  "Found name collision while taking ownership of node: %s\n",
			             key.c_str());
			debug_printf(DEBUG_QUIET, "Trying to insert key %s, node:\n", key.c_str());
			(*nodes)[i]->Dump(this);
			debug_printf(DEBUG_QUIET, "but it collided with key %s, node:\n", key.c_str());
			auto findResult = _nodeNameHash.find(key);
			if (findResult != _nodeNameHash.end()) {
				node = (*findResult).second;
				node->Dump(this);
			} else {
				debug_error(1, DEBUG_QUIET, "What? This is impossible!\n");
			}

			debug_error(1, DEBUG_QUIET, "Aborting.\n");
		}
	}

	// 3. Update our node id hash to include the new nodes.
	for (i = 0; i < nodes->size(); i++) {
		key_id = (*nodes)[i]->GetNodeID();
		auto insertResult = _nodeIDHash.insert(std::make_pair(key_id, (*nodes)[i])) ;
		if (insertResult.second != true) {
			debug_error(1, DEBUG_QUIET, "Found node id collision while taking ownership of node: %s\n",
			           (*nodes)[i]->GetNodeName());
		}
	}

	// 4. Copy any reject info from the splice.
	if (om->_reject) {
		SetReject(om->_firstRejectLoc);
	}
}

//---------------------------------------------------------------------------
// Iterate over the nodes and set the effective priorities for the nodes.
void Dag::SetNodePriorities()
{
	const int dagPrior = dagOpts[shallow::i::Priority];
	if (dagPrior != 0) {
		for (auto & node : _nodes) {
			node->_effectivePriority += dagPrior;
		}
	}
}
