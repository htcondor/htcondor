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

#include "node.h"
#include "condor_debug.h"
#include "dag.h"
#include "directory_util.h"

//---------------------------------------------------------------------------
// Initialize static members (public and private)
static const char *JOB_TAG_NAME = "+job_tag_name";
static const char *PEGASUS_SITE = "+pegasus_site";
int Node::_nextJobstateSeqNum = 1;

node_id_t Node::_nodeID_counter = 0;
int Node::NOOP_NODE_PROCID = INT_MAX;

time_t Node::lastStateChangeTime;

// Node States that represent an active node
const std::set<Node::status_t> Node::ACTIVE_STATES= {
	Node::STATUS_PRERUN,
	Node::STATUS_SUBMITTED,
	Node::STATUS_POSTRUN,
};

// NOTE: this must be kept in sync with the status_t enum
const char* Node::status_t_names[] = {
	"STATUS_NOT_READY",
	"STATUS_READY    ",
	"STATUS_PRERUN   ",
	"STATUS_SUBMITTED",
	"STATUS_POSTRUN  ",
	"STATUS_DONE     ",
	"STATUS_ERROR    ",
	"STATUS_FUTILE   "
};

//---------------------------------------------------------------------------
Node::Node(const char* nodeName, const char *directory, const char* cmdFile) {
	ASSERT(nodeName != nullptr);
	ASSERT(cmdFile != nullptr);

	debug_printf(DEBUG_DEBUG_1, "Node::Node(%s, %s, %s)\n", nodeName, directory, cmdFile);

	_nodeName = nodeName;
	// Initialize _directory and _cmdFile in a de-duped stringSpace since
	// these strings may be repeated in thousands of nodes
	_directory = DAG::STRING_SPACE::DEDUP(directory);
	_cmdFile = DAG::STRING_SPACE::DEDUP(cmdFile);
	// jobID is a primary key (a database term).  All should be unique
	_nodeID = _nodeID_counter++;

	return;
}

//---------------------------------------------------------------------------
void
Node::PrefixDirectory(const std::string &prefix) {
	if (prefix == "." || fullpath(_directory.data())) {
		return;
	}

	std::string newDir;
	dircat(prefix.c_str(), _directory.data(), newDir);

	DAG::STRING_SPACE::FREE(_directory);
	_directory = DAG::STRING_SPACE::DEDUP(newDir);
}

//---------------------------------------------------------------------------
void Node::Dump(const Dag *dag) const {
	dprintf(D_ALWAYS, "---------------------- Node ----------------------\n");
	dprintf(D_ALWAYS, "      Node Name: %s\n", _nodeName.c_str());
	dprintf(D_ALWAYS, "           Noop: %s\n", _noop ? "true" : "false");
	dprintf(D_ALWAYS, "         NodeID: %d\n", _nodeID);
	dprintf(D_ALWAYS, "    Node Status: %s\n", GetStatusName());
	dprintf(D_ALWAYS, "Node return val: %d\n", retval);

	if (_Status == STATUS_ERROR) {
		dprintf(D_ALWAYS, "          Error: %s\n", error_text.c_str());
	}

	dprintf(D_ALWAYS, "Job Submit File: %s\n", _cmdFile.data());

	if (_scriptPre)  { dprintf(D_ALWAYS, "     PRE Script: %s\n", _scriptPre->GetCmd()); }
	if (_scriptPost) { dprintf(D_ALWAYS, "    POST Script: %s\n", _scriptPost->GetCmd()); }
	if (_scriptHold) { dprintf(D_ALWAYS, "    HOLD Script: %s\n", _scriptHold->GetCmd()); }

	if (retry_max > 0) { dprintf(D_ALWAYS, "          Retry: %d\n", retry_max); }

	if (_CondorID._cluster == -1) {
		dprintf(D_ALWAYS, " HTCondor Job ID: [not yet submitted]\n");
	} else {
		dprintf(D_ALWAYS, " HTCondor Job ID: (%d.%d.%d)\n",
		        _CondorID._cluster, _CondorID._proc, _CondorID._subproc);
	}

	std::string parents, children;
	PrintParents(parents, 1024, dag);
	PrintChildren(children, 1024, dag);
	dprintf(D_ALWAYS, "PARENTS: %s WAITING: %d CHILDREN: %s\n", parents.c_str(), (int)IsWaiting(), children.c_str());
}

//---------------------------------------------------------------------------
bool
Node::SanityCheck() const {
	bool result = true;

	if (countedAsDone == true && _Status != STATUS_DONE) {
		dprintf(D_ALWAYS, "BADNESS 10000: countedAsDone == true but _Status != STATUS_DONE\n");
		result = false;
	}

		// TODO:
		//
		// - make sure # of parents whose state != done+success is
		// equal to waitingCount
		//
		// - make sure no job appear twice in the DAG
		// 
		// - verify parent/child symmetry across entire DAG

	return result;
}

//---------------------------------------------------------------------------
bool
Node::SetStatus(status_t newStatus) {
	debug_printf(DEBUG_DEBUG_1, "Node(%s)::_Status = %s\n",
	             GetNodeName(), status_t_names[_Status]);

	debug_printf(DEBUG_DEBUG_1, "Node(%s)::SetStatus(%s)\n",
	             GetNodeName(), status_t_names[newStatus]);

	_Status = newStatus;
	// TODO: add some state transition sanity-checking here?
	return true;
}

//---------------------------------------------------------------------------
bool
Node::GetProcIsIdle(int proc) {
	CheckTrackingJob(proc);
	return (jobs[proc].events & IDLE_MASK) != 0;
}

//---------------------------------------------------------------------------
void
Node::SetProcIsIdle(int proc, bool isIdle) {
	SetStateChangeTime();

	CheckTrackingJob(proc);

	if (isIdle) {
		jobs[proc].events |= IDLE_MASK;
	} else {
		jobs[proc].events &= ~IDLE_MASK;
	}
}

//---------------------------------------------------------------------------
void
Node::SetProcEvent(int proc, int event) {
	SetStateChangeTime();

	CheckTrackingJob(proc);

	jobs[proc].events |= event;
}

//---------------------------------------------------------------------------
bool
Node::CheckBatchFailed(int tolerance) {
	// TODO: Add per node overrides
	return totalJobsFailed > tolerance;
}

//---------------------------------------------------------------------------
void
Node::PrintProcIsIdle() {
	int proc = 0;
	for (auto [state, _] : jobs) {
		debug_printf(DEBUG_QUIET, "Node(%s)::_isIdle[%d]: %d\n",
		             GetNodeName(), proc++, (state & IDLE_MASK) != 0);
	}
}

//---------------------------------------------------------------------------
int
Node::PrintParents(std::string& buf, size_t bufmax, const Dag* dag, const char* sep) const {
	int count = 0;

	if (HasSingleParent()) {
		Node* parent = dag->FindNodeByNodeID(m_parents);
		ASSERT(parent != nullptr);
		buf += parent->GetNodeName();
		count = 1;
	} else if (HasMultipleParents()) {
		for (auto& [id, _] : dag->edge_table.GetWaitEdge(m_parents)) {
			Node* parent = dag->FindNodeByNodeID(id);
			ASSERT(parent != nullptr);

			if (buf.size() >= bufmax) { break; }
			if (count > 0) { buf += sep; }
			buf += parent->GetNodeName();
			++count;
		}
	}

	return count;
}

//---------------------------------------------------------------------------
int
Node::PrintChildren(std::string& buf, size_t bufmax, const Dag* dag, const char* sep) const {
	int count = 0;

	if (m_children != NO_EDGE_ID) {
		if (EdgeTable::IsDirect(m_children)) {
			DagArc& direct = dag->edge_table.GetDirectArc(m_children);
			ASSERT(direct.id != NO_ID);
			Node* child = dag->FindNodeByNodeID(direct.id);
			ASSERT(child != nullptr);

			buf += child->GetNodeName();
			++count;
		} else {
			for (auto& [id, _] : dag->edge_table[m_children]) {
				Node * child = dag->FindNodeByNodeID(id);
				ASSERT(child != nullptr);

				if (buf.size() >= bufmax) { break; }
				if (count > 0) { buf += sep; }
				buf += child->GetNodeName();
				++count;
			}
		}
	}

	return count;
}

//---------------------------------------------------------------------------
// Record that `parent` has finished (regardless of outcome); returns true once
// this was the last outstanding parent, i.e. the child is fully unblocked.
bool
Node::MarkParentDone(Dag& dag, Node* child, node_id_t parent) {
	ASSERT(child != nullptr);

	if ( ! child->HasMultipleParents()) {
		ASSERT(child->HasSingleParent());
		ASSERT(child->GetParentsID() == parent); // notifier must be the recorded parent
		child->_parents_done = true;
		return true;
	}

	bool done = dag.edge_table.GetWaitEdge(child->GetParentsID()).MarkDone(parent);
	if (done) { child->_parents_done = true; }

	return done;
}

//---------------------------------------------------------------------------
// Set `child` to STATUS_FUTILE unless already FUTILE/preDone; increments count on change.
// Returns false only when child was already FUTILE (caller should stop recursing that branch).
bool
Node::InvalidateChild(Node* child, int& count) {
	ASSERT(child != nullptr);

	//If Status is already futile or the node is preDone don't try
	//to set status and update counts
	if (child->GetStatus() == Node::STATUS_FUTILE) {
		return false;
	} else if ( ! child->IsPreDone()) {
		ASSERT( ! child->CanSubmit());
		if (child->SetStatus(Node::STATUS_FUTILE)) { count++; }
		else {
			debug_printf(DEBUG_NORMAL,"Error: Failed to set node %s to status %s\n",
			             child->GetNodeName(), status_t_names[Node::STATUS_FUTILE]);
		}
	}

	return true;
}

//---------------------------------------------------------------------------
// tell children that the parent is complete, and call the given function
// for children that have no more incomplete parents
void
Node::NotifyChildren(Dag& dag, const std::function<bool(Dag& dag, Node* child)>& fn) {
	if (m_children != NO_EDGE_ID) {
		if (EdgeTable::IsDirect(m_children)) {
			DagArc& direct = dag.edge_table.GetDirectArc(m_children);
			ASSERT(direct.id != NO_ID);
			Node* child = dag.FindNodeByNodeID(direct.id);
			ASSERT(child != nullptr);

			if (MarkParentDone(dag, child, GetNodeID())) {
				if (fn) { fn(dag, child); }
			}
		} else {
			for (auto& [id, _] : dag.edge_table[m_children]) {
				Node * child = dag.FindNodeByNodeID(id);
				ASSERT(child != nullptr);

				if (MarkParentDone(dag, child, GetNodeID())) {
					if (fn) { fn(dag, child); }
				}
			}
		}
	}
}

//---------------------------------------------------------------------------
// Recursively set descendants to status FUTILE (strong deps), or -- for a direct
// child reached via a weak dependency -- treat this exactly like a normal
// parent-completion notification instead of cascading FUTILE.
// Return the number of nodes newly set to FUTILE.
int
Node::SetDescendantsToFutile(Dag& dag, const std::function<bool(Dag& dag, Node* child)>& on_weak_unblocked) {
	int count = 0;

	auto weak_complete = [this, &dag, &on_weak_unblocked](Node* child) {
		if (MarkParentDone(dag, child, GetNodeID()) && on_weak_unblocked) {
			on_weak_unblocked(dag, child);
		}
	};

	if (m_children != NO_EDGE_ID) {
		if (EdgeTable::IsDirect(m_children)) {
			DagArc& direct = dag.edge_table.GetDirectArc(m_children);
			ASSERT(direct.id != NO_ID);
			Node* child = dag.FindNodeByNodeID(direct.id);

			if (direct.IsWeak()) {
				weak_complete(child);
			} else if (InvalidateChild(child, count)) {
				count += child->CascadeFutile(dag);
			}
		} else {
			for (auto& arc : dag.edge_table[m_children]) {
				Node * child = dag.FindNodeByNodeID(arc.id);

				if (arc.IsWeak()) {
					weak_complete(child);
				} else if (InvalidateChild(child, count)) {
					count += child->CascadeFutile(dag);
				}
			}
		}
	}

	return count;
}

//---------------------------------------------------------------------------
// Non-transitive cascade: this node never executed, so none of its children --
// weak or strong -- can be considered satisfied.
int
Node::CascadeFutile(Dag& dag) {
	int count = 0;

	if (m_children != NO_EDGE_ID) {
		if (EdgeTable::IsDirect(m_children)) {
			DagArc& direct = dag.edge_table.GetDirectArc(m_children);
			ASSERT(direct.id != NO_ID);
			Node* child = dag.FindNodeByNodeID(direct.id);

			if ( ! InvalidateChild(child, count)) { return 0; }

			count += child->CascadeFutile(dag);
		} else {
			for (auto& [id, _] : dag.edge_table[m_children]) {
				Node * child = dag.FindNodeByNodeID(id);

				if ( ! InvalidateChild(child, count)) { continue; }

				count += child->CascadeFutile(dag);
			}
		}
	}

	return count;
}

//---------------------------------------------------------------------------
// True if this node has at least one child and every child dependency is weak.
bool
Node::AllChildrenWeak(const Dag* dag) const {
	if (m_children == NO_EDGE_ID) { return false; }

	if (EdgeTable::IsDirect(m_children)) {
		return dag->edge_table.GetDirectArc(m_children).IsWeak();
	}

	return std::ranges::all_of(dag->edge_table[m_children], &DagArc::IsWeak);
}

//---------------------------------------------------------------------------
// visit all of the children, either marking them, or checking for cycles
int
Node::VisitChildren(Dag& dag, const std::function<int(Dag& dag, Node* parent, Node* child)>& fn) {
	int retval = 0;

	if (m_children != NO_EDGE_ID) {
		if (EdgeTable::IsDirect(m_children)) {
			DagArc& direct = dag.edge_table.GetDirectArc(m_children);
			ASSERT(direct.id != NO_ID);
			Node* child = dag.FindNodeByNodeID(direct.id);
			ASSERT(child != nullptr);
			retval += fn(dag, this, child);
		} else {
			for (auto& [id, _] : dag.edge_table[m_children]) {
				Node* child = dag.FindNodeByNodeID(id);
				ASSERT(child != nullptr);
				retval += fn(dag, this, child);
			}
		}
	}

	return retval;
}

//---------------------------------------------------------------------------
bool
Node::CanAddParent(std::string &whynot) {
	switch(GetType()) {
		case NodeType::FINAL:
			whynot = "Tried to add a parent to a Final node";
			return false;
		case NodeType::PROVISIONER:
			whynot = "Tried to add parent to the PROVISIONER node";
			return false;
		case NodeType::SERVICE:
			whynot = "Tried to add a parent to a SERVICE node";
			return false;
		default:
			break;
	}

	return true;
}

//---------------------------------------------------------------------------
bool
Node::CanAddChildren(std::string &whynot) {
	switch(GetType()) {
		case NodeType::FINAL:
			whynot = "Tried to add a child to a Final node";
			return false;
		case NodeType::PROVISIONER:
			whynot = "Tried to add child to the PROVISIONER node";
			return false;
		case NodeType::SERVICE:
			whynot = "Tried to add a child to a SERVICE node";
			return false;
		default:
			break;
	}

	return true;
}

//---------------------------------------------------------------------------
bool
Node::AddVar(const std::string& name, const std::string& value, bool prepend) {
	for (auto& var : varsFromDag) {
		if (name == var._name) {
			debug_printf(DEBUG_NORMAL, "Warning: VAR \"%s\" is already defined in node \"%s\"\n",
			             name.c_str(), GetNodeName());
			check_warning_strictness(DAG_STRICT_3);
			debug_printf(DEBUG_NORMAL, "        Setting VAR \"%s\" = \"%s\"\n", name.c_str(), value.c_str());
			DAG::STRING_SPACE::FREE(var._value);
			var._value = DAG::STRING_SPACE::DEDUP(value);
			return true;
		}
	}

	auto name_v = DAG::STRING_SPACE::DEDUP(name);
	auto value_v = DAG::STRING_SPACE::DEDUP(value);

	varsFromDag.emplace_back(name_v, value_v, prepend);
	return true;
}


//---------------------------------------------------------------------------
bool
Node::AddScript(Script* script) {
	if ( ! script) { return false; }

	script->SetNode(this);

	// Check if a script of the same type has already been assigned to this node
	const char *type_name;
	Script* old_script = nullptr;
	switch(script->GetType()) {
		case ScriptType::PRE:
			type_name = "PRE";
			old_script = _scriptPre;
			_scriptPre = script;
			break;
		case ScriptType::POST:
			type_name = "POST";
			old_script = _scriptPost;
			_scriptPost = script;
			break;
		case ScriptType::HOLD:
			type_name = "HOLD";
			old_script = _scriptHold;
			_scriptHold = script;
			break;
	}

	if (old_script) {
		debug_printf(DEBUG_NORMAL, "Warning: node %s already has %s script <%s> assigned; changing to <%s>\n",
		             GetNodeName(), type_name, old_script->GetCmd(), script->GetCmd());
		delete old_script;
		old_script = nullptr;
	}

	return true;
}

//---------------------------------------------------------------------------
bool
Node::AddPreSkip(int exitCode, std::string &whynot) {
	if (exitCode < PRE_SKIP_MIN || exitCode > PRE_SKIP_MAX) {
		formatstr(whynot, "PRE_SKIP exit code must be between %d and %d\n",
		          PRE_SKIP_MIN, PRE_SKIP_MAX );
		return false;
	}

	if (exitCode == 0) {
		debug_printf(DEBUG_NORMAL, "Warning: exit code 0 for a PRE_SKIP value is weird.\n");
	}

	if (_preskip != PRE_SKIP_INVALID) {
		debug_printf(DEBUG_NORMAL, "Warning: new PRE_SKIP value  %d for node %s overrides old value %d\n",
		            exitCode, GetNodeName(), _preskip);
		check_warning_strictness(DAG_STRICT_3);
	}
	_preskip = exitCode;	

	whynot.clear();
	return true;
}

//---------------------------------------------------------------------------
void
Node::SetCategory(const char *categoryName, ThrottleByCategory &catThrottles) {
	ASSERT(_type != NodeType::FINAL);
	ASSERT(_type != NodeType::SERVICE);

	std::string tmpName(categoryName);

	if ((_throttleInfo != nullptr) && (tmpName != *(_throttleInfo->_category))) {
		debug_printf(DEBUG_NORMAL, "Warning: new category %s for node %s "
		             "overrides old value %s\n", categoryName, GetNodeName(),
		             _throttleInfo->_category->c_str());
		check_warning_strictness(DAG_STRICT_3);
	}

		// Note: we must assign a ThrottleInfo here even if the name
		// already matches, for the case of lifting splices.
	ThrottleByCategory::ThrottleInfo *oldInfo = _throttleInfo;

	ThrottleByCategory::ThrottleInfo *throttleInfo = catThrottles.GetThrottleInfo(&tmpName);
	if (throttleInfo != nullptr) {
		_throttleInfo = throttleInfo;
	} else {
		_throttleInfo = catThrottles.AddCategory(&tmpName);
	}

	if (oldInfo != _throttleInfo) {
		if (oldInfo != nullptr) {
			oldInfo->_totalJobs--;
		}
		_throttleInfo->_totalJobs++;
	}
}

//---------------------------------------------------------------------------
const char *
Node::GetJobstateJobTag() {
	if (_jobTag.empty()) {
		std::string jobTagName = MultiLogFiles::loadValueFromSubFile(_cmdFile.data(), _directory.data(), JOB_TAG_NAME);
		if (jobTagName.empty()) {
			jobTagName = PEGASUS_SITE;
		} else {
			// Remove double-quotes
			int begin = jobTagName[0] == '\"' ? 1 : 0;
			int last = jobTagName.length() - 1;
			int end = jobTagName[last] == '\"' ? last - 1 : last;
			jobTagName = jobTagName.substr(begin, 1 + end - begin);
		}

		std::string tmpJobTag = MultiLogFiles::loadValueFromSubFile(_cmdFile.data(), _directory.data(), jobTagName.c_str());
		if (tmpJobTag.empty()) {
			tmpJobTag = "-";
		} else {
			// Remove double-quotes
			int begin = tmpJobTag[0] == '\"' ? 1 : 0;
			int last = tmpJobTag.length() - 1;
			int end = tmpJobTag[last] == '\"' ? last - 1 : last;
			tmpJobTag = tmpJobTag.substr(begin, 1 + end - begin);
		}
		_jobTag = DAG::STRING_SPACE::DEDUP(tmpJobTag);
	}

	return _jobTag.data();
}

//---------------------------------------------------------------------------
int
Node::GetJobstateSequenceNum() {
	if (_jobstateSeqNum == 0) {
		_jobstateSeqNum = _nextJobstateSeqNum++;
	}

	return _jobstateSeqNum;
}

//---------------------------------------------------------------------------
bool
Node::SetCondorID(const CondorID& cid) {
	bool ret = true;
	if(GetCluster() != -1) {
		debug_printf(DEBUG_NORMAL, "Reassigning the id of job %s from (%d.%d.%d) to (%d.%d.%d)\n",
		             GetNodeName(), GetCluster(), GetProc(), GetSubProc(),
		             cid._cluster, cid._proc,cid._subproc);
		ret = false;
	}
	_CondorID = cid;
	return ret;	
}

//---------------------------------------------------------------------------
bool
Node::Hold(int proc) {
	SetStateChangeTime();

	CheckTrackingJob(proc);

	if ((jobs[proc].events & HOLD_MASK) != HOLD_MASK) {
		jobs[proc].events |= HOLD_MASK;

		++_jobProcsOnHold;
		++_timesHeld;
		return true;
	} else {
		dprintf(D_FULLDEBUG, "Received hold event for node %s, and job %d.%d is already on hold!\n",
		        GetNodeName(), GetCluster(), proc);
	}

	return false;
}

//---------------------------------------------------------------------------
bool
Node::Release(int proc, bool warn) {
	SetStateChangeTime();

	if (proc >= static_cast<int>(jobs.size()) || (jobs[proc].events & HOLD_MASK) != HOLD_MASK) {
		if (warn) {
			dprintf(D_FULLDEBUG, "Received release event for node %s, but job %d.%d is not on hold\n",
			        GetNodeName(), GetCluster(), GetProc());
		}
		return false; // We never marked this as being on hold
	}

	jobs[proc].events &= ~HOLD_MASK;
	--_jobProcsOnHold;
	return true;
}

//---------------------------------------------------------------------------
// Note:  For multi-proc jobs, if one proc failed this was getting called
// immediately, which was not correct.  I changed how this was called, but
// we should probably put in code to make sure it's not called too soon,
// but is called...  wenger 2015-11-05
void
Node::Cleanup()
{
	int proc = 0;
	for (auto [state, _] : jobs) {
		if (state != (EXEC_MASK | ABORT_TERM_MASK)) {
			debug_printf(DEBUG_NORMAL, "Warning for node %s: unexpected job event value for proc %d: %d!\n",
			             GetNodeName(), proc, (int)state);
			check_warning_strictness(DAG_STRICT_2);
		}
		++ proc;
	}

	jobs.clear();
}

//---------------------------------------------------------------------------
/*
	Verify internal job states agree with queue query such that jobs found in
	queue don't have ABORT_TERM_MASK and jobs not found in queue do have ABORT_TERM_MASK
	return True for everything matches
	return False for one job has differring state
*/
bool
Node::VerifyJobStates(std::set<int>& queuedJobs) {
	bool good_state = true;
	int proc = 0;
	int cluster = GetCluster();
	const char* nodeName = GetNodeName();

	if (jobs.size() == 0) {
		debug_printf(DEBUG_NORMAL, "ERROR: Node %s is internally in submitted state with no recorded job events!\n",
		             nodeName);
		return false;
	}

	for (auto& [state, _] : jobs) {
		if (queuedJobs.contains(proc)) {
			if (state & ABORT_TERM_MASK) {
				debug_printf(DEBUG_NORMAL,
				             "ERROR: Node %s (%d.%d) located in queue but internally marked as exited.\n",
				             nodeName, cluster, proc);
				good_state = false;
			}
		} else if ( ! (state & ABORT_TERM_MASK)) { // If abort/term mask bit not set
			debug_printf(DEBUG_NORMAL,
			             "ERROR: Node %s (%d.%d) not located in queue and not internally marked as exited.\n",
			             nodeName, cluster, proc);
			good_state = false;
		}
		proc++;
	}

	return good_state;
}

