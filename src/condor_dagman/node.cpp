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

std::map<std::string, int> Node::stringSpace;

NodeID_t Node::_nodeID_counter = 0;
int Node::NOOP_NODE_PROCID = INT_MAX;
std::deque<std::unique_ptr<Edge>> Edge::_edgeTable;

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
std::string_view
Node::dedup_str(const std::string& str) {
	if (stringSpace.contains(str)) {
		stringSpace[str]++;
	} else {
		stringSpace.insert({str, 1});
	}

	auto it = stringSpace.find(str);
	return std::string_view(it->first);
}

//---------------------------------------------------------------------------
void
Node::free_str(std::string_view& view) {
	std::string key(view.data());
	if (stringSpace.contains(key)) {
		stringSpace[key]--;
		std::erase_if(stringSpace, [](const auto& pair) {
			const auto& [_, count] = pair;
			return count <= 0;
		});
	}
	view = {};
}

//---------------------------------------------------------------------------
Node::Node(const char* nodeName, const char *directory, const char* cmdFile) {
	ASSERT(nodeName != nullptr);
	ASSERT(cmdFile != nullptr);

	debug_printf(DEBUG_DEBUG_1, "Node::Node(%s, %s, %s)\n", nodeName, directory, cmdFile);

	_nodeName = nodeName;
	// Initialize _directory and _cmdFile in a de-duped stringSpace since
	// these strings may be repeated in thousands of nodes
	_directory = dedup_str(directory);
	_cmdFile = dedup_str(cmdFile);
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

	free_str(_directory);
	_directory = dedup_str(newDir);
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
	if (GetNoop()) { proc = 0; }

	if (proc >= static_cast<int>(_gotEvents.size())) {
		_gotEvents.resize(proc + 1, 0);
	}
	return (_gotEvents[proc] & IDLE_MASK) != 0;
}

//---------------------------------------------------------------------------
void
Node::SetProcIsIdle(int proc, bool isIdle) {
	if (GetNoop()) { proc = 0; }

	SetStateChangeTime();

	if (proc >= static_cast<int>(_gotEvents.size())) {
		_gotEvents.resize(proc + 1, 0);
	}

	if (isIdle) {
		_gotEvents[proc] |= IDLE_MASK;
	} else {
		_gotEvents[proc] &= ~IDLE_MASK;
	}
}

//---------------------------------------------------------------------------
void
Node::SetProcEvent(int proc, int event) {
	if (GetNoop()) { proc = 0; }

	SetStateChangeTime();

	if (proc >= static_cast<int>(_gotEvents.size())) {
			_gotEvents.resize(proc + 1, 0);
	}

	_gotEvents[proc] |= event;
}

//---------------------------------------------------------------------------
void
Node::PrintProcIsIdle() {
	int proc = 0;
	for (auto state : _gotEvents) {
		debug_printf(DEBUG_QUIET, "Node(%s)::_isIdle[%d]: %d\n",
		             GetNodeName(), proc++, (state & IDLE_MASK) != 0);
	}
}

//---------------------------------------------------------------------------
int
Node::CountChildren() const {
	int count = 0;
	if (_child != NO_ID) {
		if (_multiple_children) {
			Edge * edge = Edge::ById(_child);
			ASSERT(edge);
			count = (int)edge->size();
		} else {
			count = 1;
		}
	}
	return count;
}

//---------------------------------------------------------------------------
bool
Node::ParentComplete(const Node* parent) {
	bool fail = true;
	int num_waiting = 0;

	bool already_done = _parents_done;
	if (_parent != NO_ID) {
		if (_multiple_parents) {
			WaitEdge * edge = WaitEdge::ById(_parent);
			fail = ! edge->MarkDone(parent->GetNodeID(), already_done);
			num_waiting = edge->Waiting();
			_parents_done = (num_waiting == 0);
		} else {
			num_waiting = _parents_done ? 0 : 1;
			if (parent->GetNodeID() == _parent) {
				fail = false;
				_parents_done = true;
				num_waiting = 0;
			}
		}
	}

	if (fail) {
		debug_printf(DEBUG_QUIET, "ERROR: ParentComplete(%s) failed for child node %s: num_waiting=%d\n",
		             parent ? parent->GetNodeName() : "nullptr", this->GetNodeName(), num_waiting);
	}
	return ! IsWaiting();
}

//---------------------------------------------------------------------------
int
Node::PrintParents(std::string& buf, size_t bufmax, const Dag* dag, const char* sep) const {
	int count = 0;
	if (_parent != NO_ID) {
		if (_multiple_parents) {
			Edge * edge = Edge::ById(_parent);
			ASSERT(edge);
			if ( ! edge->_ary.empty()) {
				for (int & it : edge->_ary) {
					if (buf.size() >= bufmax)
						break;

					Node * parent = dag->FindNodeByNodeID(it);
					ASSERT(parent != nullptr);

					if (count > 0) buf += sep;
					buf += parent->GetNodeName();
					++count;
				}
			}
		} else {
			Node* parent = dag->FindNodeByNodeID(_parent);
			ASSERT(parent != nullptr);
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
	if (_child != NO_ID) {
		if (_multiple_children) {
			Edge * edge = Edge::ById(_child);
			ASSERT(edge);
			if ( ! edge->_ary.empty()) {
				for (int & it : edge->_ary) {
					if (buf.size() >= bufmax)
						break;

					Node * child = dag->FindNodeByNodeID(it);
					ASSERT(child != nullptr);

					if (count > 0) buf += sep;
					buf += child->GetNodeName();
					++count;
				}
			}
		} else {
			Node* child = dag->FindNodeByNodeID(_child);
			ASSERT(child != nullptr);
			buf += child->GetNodeName();
			++count;
		}
	}
	return count;
}

//---------------------------------------------------------------------------
// tell children that the parent is complete, and call the given function
// for children that have no more incomplete parents
int Node::NotifyChildren(Dag& dag, bool(*pfn)(Dag& dag, Node* child)) {
	int count = 0;
	if (_child != NO_ID) {
		if (_multiple_children) {
			Edge * edge = Edge::ById(_child);
			ASSERT(edge);
			if ( ! edge->_ary.empty()) {
				for (int & it : edge->_ary) {
					Node * child = dag.FindNodeByNodeID(it);
					ASSERT(child != nullptr);
					if (child->ParentComplete(this)) {
						if (pfn) pfn(dag, child);
					}
				}
			}
		} else {
			Node* child = dag.FindNodeByNodeID(_child);
			ASSERT(child != nullptr);
			if (child->ParentComplete(this)) {
				if (pfn) pfn(dag, child);
			}
		}
	}
	return count;
}

//---------------------------------------------------------------------------
//Recursively visit all descendant nodes and set to status FUTILE
//Return the number of jobs set to FUTILE
int
Node::SetDescendantsToFutile(Dag& dag) {
	int count = 0;
	if (_child != NO_ID) {
		if (_multiple_children) {
			Edge * edge = Edge::ById(_child);
			ASSERT(edge);
			if ( ! edge->_ary.empty()) {
				for (int & it : edge->_ary) {
					Node * child = dag.FindNodeByNodeID(it);
					ASSERT(child != nullptr);
					//If Status is already futile or the node is preDone don't try
					//to set status and update counts
					if (child->GetStatus() == Node::STATUS_FUTILE) {
						continue;
					} else if ( ! child->IsPreDone()) {
						ASSERT( ! child->CanSubmit());
						if (child->SetStatus(Node::STATUS_FUTILE)) { count++; }
						else {
							debug_printf(DEBUG_NORMAL,"Error: Failed to set node %s to status %s\n",
							             child->GetNodeName(), status_t_names[Node::STATUS_FUTILE]);
						}
					}
					count += child->SetDescendantsToFutile(dag);
				}
			}
		} else {
			Node* child = dag.FindNodeByNodeID(_child);
			ASSERT(child != nullptr);
			//If Status is already futile or the node is preDone don't try
			//to set status and update counts
			if (child->GetStatus() == Node::STATUS_FUTILE) {
				return 0;
			} else if ( ! child->IsPreDone()) {
				ASSERT( ! child->CanSubmit());
				if (child->SetStatus(Node::STATUS_FUTILE)) { count++; }
				else {
					debug_printf(DEBUG_NORMAL,"Error: Failed to set node %s to status %s\n",
					             child->GetNodeName(), status_t_names[Node::STATUS_FUTILE]);
				}
			}
			count += child->SetDescendantsToFutile(dag);
		}
	}
	return count;
}

//---------------------------------------------------------------------------
// visit all of the children, either marking them, or checking for cycles
int
Node::VisitChildren(Dag& dag, int(*pfn)(Dag& dag, Node* parent, Node* child, void* args), void* args) {
	int retval = 0;
	if (_child != NO_ID) {
		if (_multiple_children) {
			Edge * edge = Edge::ById(_child);
			ASSERT(edge);
			if (! edge->_ary.empty()) {
				for (int & it : edge->_ary) {
					Node * child = dag.FindNodeByNodeID(it);
					ASSERT(child != nullptr);
					retval += pfn(dag, this, child, args);
				}
			}
		} else {
			Node* child = dag.FindNodeByNodeID(_child);
			ASSERT(child != nullptr);
			retval += pfn(dag, this, child, args);
		}
	}
	return retval;
}

//---------------------------------------------------------------------------
bool
Node::CanAddParent(const Node* parent, std::string &whynot) {
	if ( ! parent) {
		whynot = "parent == nullptr";
		return false;
	}

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

		// we don't currently allow a new parent to be added to a
		// child that has already been started (unless the parent is
		// already marked STATUS_DONE, e.g., when rebuilding from a
		// rescue DAG) -- but this restriction might be lifted in the
		// future once we figure out the right way for the DAG to
		// respond...
	if (_Status != STATUS_READY && parent->GetStatus() != STATUS_DONE) {
		formatstr(whynot, "%s child may not be given a new %s parent",
		          GetStatusName(), parent->GetStatusName());
		return false;
	}
	whynot = "n/a";
	return true;
}

//---------------------------------------------------------------------------
bool
Node::CanAddChildren(const std::vector<Node*>& children, std::string &whynot) {
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

	for (auto child : children) {
			if ( ! child->CanAddParent(this, whynot)) {
			return false;
		}
	}

	return true;
}

//---------------------------------------------------------------------------
bool
Node::AddVar(const char *name, const char *value, const char* filename, int lineno, bool prepend) {
	auto name_v = dedup_str(name);
	auto value_v = dedup_str(value);
	for (auto& var : varsFromDag) {
		if (name_v == var._name) {
			debug_printf(DEBUG_NORMAL, "Warning: VAR \"%s\" is already defined in node \"%s\" (Discovered at file \"%s\", line %d)\n",
			             name_v.data(), GetNodeName(), filename, lineno);
			check_warning_strictness(DAG_STRICT_3);
			debug_printf(DEBUG_NORMAL, "Warning: Setting VAR \"%s\" = \"%s\"\n", name_v.data(), value_v.data());
			free_str(var._value);
			var._value = value_v;
			return true;
		}
	}
	varsFromDag.emplace_back(name_v, value_v, prepend);
	return true;
}

//---------------------------------------------------------------------------
int
Node::PrintVars(std::string &vars) {
	int num_vars = 0;
	for (auto & it : varsFromDag) {
		vars.push_back(' ');
		vars.append(it._name);
		vars.push_back('=');
		vars.push_back('\"');
		// now we print the value, but we have to re-escape certain characters
		const char * p = it._value.data();
		while (*p) {
			char c = *p++;
			if (c == '\"' || c == '\\') {
				vars.push_back('\\');
			}
			vars.push_back(c);
		}
		vars.push_back('\"');
		++num_vars;
	}
	return num_vars;
}

//---------------------------------------------------------------------------
bool
Node::AddChildren(const std::vector<Node*>& children, std::string &whynot) {
	// check if all of this can be our child, and if all are ok being our children
	if ( ! CanAddChildren(children, whynot)) {
		return false;
	}

	// optimize the insertion case for more than a single child
	// into current a single or empty edge
	if (more_than_one(children) && ! _multiple_children) {
		NodeID_t id = _child;
		Edge * edge = Edge::PromoteToMultiple(_child, _multiple_children, id);

		// count the children so we can reserve space in the edge array
		int num_children = (id == NO_ID) ? 0 : 1;
		for (auto it = children.begin(); it != children.end(); ++it) { ++num_children; }
		edge->_ary.reserve(num_children);

		// populate the edge array, since we know that children is sorted we can just push_back here.
		for (auto child : children) {
				edge->_ary.push_back(child->GetNodeID());
			// count the parents of the children so that we can allocate space in AdjustEdges
			child->_numparents += 1;
		}
		if (id != NO_ID) { edge->Add(id); }
		return true;
	}

	for (auto child : children) {
		// if we have no children, add this as a direct child
		if (_child == NO_ID) {
			_multiple_children = false;
			_child = child->GetNodeID();
			// count the parents of the children so that we can allocate space in AdjustEdges
			child->_numparents += 1;
			continue;
		}

		NodeID_t id = NO_ID;
		Edge * edge = Edge::PromoteToMultiple(_child, _multiple_children, id);
		if (id != NO_ID) {
			// insert the old _child id as the first id in the collection
			edge->Add(id);
		}
		if ( ! edge->Add(child->GetNodeID())) {
			debug_printf(DEBUG_NORMAL,
				"Warning: parent %s already has child %s\n",
				GetNodeName(), child->GetNodeName());
			check_warning_strictness(DAG_STRICT_3);
		} else {
			// count parents - used by AdjustEdges to reserve space
			child->_numparents += 1;
		}
	}
	return true;
}

//---------------------------------------------------------------------------
void
Node::BeginAdjustEdges(Dag* /*dag*/) {
	// resize parents to fit _numparents
	if (_numparents > 1 && _parent == NO_ID) {
		_parent = WaitEdge::NewWaitEdge(_numparents);
		_multiple_parents = true;
	}
	// clearout the parent lists and done flags, we will set them in AdjustEdges
	if (_multiple_parents) {
		WaitEdge * wedge = WaitEdge::ById(_parent);
		wedge->MarkAllWaiting();
	} else {
		ASSERT(_numparents <= 1);
		_parent = NO_ID; // this will set set to the single parent by AdjustEdges
	}
	_parents_done = false;
}

//---------------------------------------------------------------------------
// helper for AdjustEdges, assumes that _parent has been resized by BeginAdjustEdges
// we can't mark parents done here, that can only happen after we built all of the parents
void
Node::AdjustEdges_AddParentToChild(Dag* dag, NodeID_t child_id, Node* parent) {
	NodeID_t parent_id = parent->GetNodeID();
	Node * child = dag->FindNodeByNodeID(child_id);
	ASSERT(child != nullptr);
	if (child->_parent == NO_ID) {
		child->_parent = parent_id;
	} else if ( ! child->_multiple_parents) {
		if (child->_parent == parent_id) {
			debug_printf(DEBUG_QUIET, "notice : parent %d already added to single-parent child %d\n", parent_id, child_id);
		} else {
			debug_printf(DEBUG_QUIET, "WARNING : attempted to add parent %d to single-parent child %d that already has parent %d\n",
			             parent_id, child_id, child->_parent);
		}
	} else {
		ASSERT(child->_numparents > 1);
		ASSERT(child->_multiple_parents);
		WaitEdge* wedge = WaitEdge::ById(child->_parent);
		wedge->Add(parent_id);
	}
}

//---------------------------------------------------------------------------
// update the waiting edges to contain the unfinished parents
void
Node::AdjustEdges(Dag* dag) {
	// build parents from children
	if (_child != NO_ID) {
		if (_multiple_children) {
			Edge * edge = Edge::ById(_child);
			ASSERT(edge);
			if ( ! edge->_ary.empty()) {
				for (int & it : edge->_ary) {
					AdjustEdges_AddParentToChild(dag, it, this);
				}
			}
		} else {
			AdjustEdges_AddParentToChild(dag, _child, this);
		}
	}
}

//---------------------------------------------------------------------------
void
Node::FinalizeAdjustEdges(Dag* /*dag*/) {
	// check _numparents against number of edges
	if (_numparents == 0) {
		ASSERT(_parent == NO_ID);
		_parents_done = true;
	}
	if (_numparents == 1) {
		ASSERT(!_multiple_parents);
	}
	if (_numparents > 1) {
		WaitEdge * wedge = WaitEdge::ById(_parent);
		ASSERT(wedge);
		ASSERT(_numparents == (int)wedge->size());
	}

	ASSERT(GetStatus() != STATUS_DONE);
}

//---------------------------------------------------------------------------
bool
Node::AddScript(Script* script) {
	if ( ! script) { return false; }

	script->SetNode(this);

	// Check if a script of the same type has already been assigned to this node
	const char *old_script_name = nullptr;
	const char *type_name;
	switch(script->GetType()) {
		case ScriptType::PRE:
			old_script_name = GetPreScriptName();
			type_name = "PRE";
			_scriptPre = script;
			break;
		case ScriptType::POST:
			old_script_name = GetPostScriptName();
			type_name = "POST";
			_scriptPost = script;
			break;
		case ScriptType::HOLD:
			old_script_name = GetHoldScriptName();
			type_name = "HOLD";
			_scriptHold = script;
			break;
	}

	if (old_script_name) {
		debug_printf(DEBUG_NORMAL, "Warning: node %s already has %s script <%s> assigned; changing to <%s>\n",
		             GetNodeName(), type_name, old_script_name, script->GetCmd());
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
		_jobTag = dedup_str(tmpJobTag);
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

	if (proc >= static_cast<int>(_gotEvents.size())) {
		_gotEvents.resize(proc + 1, 0);
	}

	if ((_gotEvents[proc] & HOLD_MASK) != HOLD_MASK) {
		_gotEvents[proc] |= HOLD_MASK;

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

	if (proc >= static_cast<int>(_gotEvents.size()) || (_gotEvents[proc] & HOLD_MASK) != HOLD_MASK) {
		if (warn) {
			dprintf(D_FULLDEBUG, "Received release event for node %s, but job %d.%d is not on hold\n",
			        GetNodeName(), GetCluster(), GetProc());
		}
		return false; // We never marked this as being on hold
	}

	_gotEvents[proc] &= ~HOLD_MASK;
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
	for (auto state : _gotEvents) {
		if (state != (EXEC_MASK | ABORT_TERM_MASK)) {
			debug_printf(DEBUG_NORMAL, "Warning for node %s: unexpected _gotEvents value for proc %d: %d!\n",
			             GetNodeName(), proc, (int)state);
			check_warning_strictness(DAG_STRICT_2);
		}
		++ proc;
	}

	_gotEvents.clear();
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

	if (_gotEvents.size() == 0) {
		debug_printf(DEBUG_NORMAL, "ERROR: Node %s is internally in submitted state with no recorded job events!\n",
		             nodeName);
		return false;
	}

	for (auto& state : _gotEvents) {
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

//---------------------------------------------------------------------------
void
Node::WriteRetriesToRescue(FILE *fp, bool reset_retries) {
	if (retry_max > 0) {
		ASSERT(retries <= retry_max);

		int retriesLeft = (retry_max - retries);

		if (GetStatus() == Node::STATUS_ERROR && retries < retry_max &&
		    have_retry_abort_val && retval == retry_abort_val)
		{
			fprintf(fp, "# %d of %d retries performed; remaining attempts aborted after node returned %d\n",
			        retries, retry_max, retval);
		} else if ( ! reset_retries) {
				fprintf(fp, "# %d of %d retries already performed; %d remaining\n",
				        retries, retry_max, retriesLeft);
		}

		fprintf(fp, "RETRY %s %d", GetNodeName(), reset_retries ? retry_max : retriesLeft);
		if (have_retry_abort_val) {
			fprintf(fp, " UNLESS-EXIT %d", retry_abort_val);
		}
		fprintf(fp, "\n");
	}
}
