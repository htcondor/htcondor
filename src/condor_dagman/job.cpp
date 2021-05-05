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


#include "condor_common.h"
#include "job.h"
#include "condor_string.h"
#include "condor_debug.h"
#include "dagman_main.h"
#include "read_multiple_logs.h"
#include "throttle_by_category.h"
#include "dag.h"
#include "dagman_metrics.h"
#include <set>
#include <forward_list>

static const char *JOB_TAG_NAME = "+job_tag_name";
static const char *PEGASUS_SITE = "+pegasus_site";

StringSpace Job::stringSpace;

//---------------------------------------------------------------------------
JobID_t Job::_jobID_counter = 0;  // Initialize the static data memeber
int Job::NOOP_NODE_PROCID = INT_MAX;
int Job::_nextJobstateSeqNum = 1;

#ifdef MEMORY_HOG
#else
//EdgeID_t Edge::_edgeId_counter = 0; // Initialize the static data memmber
std::deque<std::unique_ptr<Edge>> Edge::_edgeTable;
#endif


#ifdef DEAD_CODE
//---------------------------------------------------------------------------
// NOTE: this must be kept in sync with the queue_t enum
const char *Job::queue_t_names[] = {
    "Q_PARENTS",
    "Q_WAITING",
    "Q_CHILDREN",
};
#endif

//---------------------------------------------------------------------------
// NOTE: this must be kept in sync with the status_t enum
const char * Job::status_t_names[] = {
    "STATUS_NOT_READY",
    "STATUS_READY    ",
    "STATUS_PRERUN   ",
    "STATUS_SUBMITTED",
    "STATUS_POSTRUN  ",
    "STATUS_DONE     ",
    "STATUS_ERROR    "
};

//---------------------------------------------------------------------------
Job::~Job() {

	// We _should_ free_dedup() here, but it turns out both Job and
	// stringSpace objects are static, and thus the order of desrtuction when
	// dagman shuts down is unknown (global desctructors).  Thus dagman
	// may end up segfaulting on exit.  Since Job objects are never destroyed
	// by dagman until it is exiting, no need to free_dedup.
	//
    // stringSpace.free_dedup(_directory); _directory = NULL;
    // stringSpace.free_dedup(_cmdFile); _cmdFile = NULL;

    free(_dagFile); _dagFile = NULL;
    free(_jobName); _jobName = NULL;

#ifdef DEAD_CODE
	varsFromDag->Rewind();
	NodeVar *var;
	while ( (var = varsFromDag->Next()) ) {
		delete var;
	}
	delete varsFromDag;
#else
	// similarly, freeing the strings from the VARS is problematic
	// (and also a waste of time because we are exiting the process now anyway)
#endif

	delete _scriptPre;
	delete _scriptPost;
	delete _scriptHold;

	free(_jobTag);
}

//---------------------------------------------------------------------------
Job::Job( const char* jobName, const char *directory, const char* cmdFile )
	: _scriptPre(NULL)
	, _scriptPost(NULL)
	, _scriptHold(NULL)
	, retry_max(0)
	, retries(0)
	, _submitTries(0)
	, retval(-1)
	, retry_abort_val(0xdeadbeef)
	, abort_dag_val(-1)
	, abort_dag_return_val(-1)
	, _dfsOrder(-1)
	, _visited(false)
	, have_retry_abort_val(false)
	, have_abort_dag_val(false)
	, have_abort_dag_return_val(false)
	, is_cluster(false)
	, countedAsDone(false)
	, _noop(false)
	, _hold(false)
	, _type(NodeType::JOB)

#ifdef DEAD_CDE
	, varsFromDag(new List<NodeVar>)
#else
#endif
	, _queuedNodeJobProcs(0)
	, _numSubmittedProcs(0)
	, _explicitPriority(0)
	, _effectivePriority(_explicitPriority)
	, _timesHeld(0)
	, _jobProcsOnHold(0)

	, _directory(NULL)
	, _cmdFile(NULL)
	, _submitDesc(NULL)
	, _dagFile(NULL)
	, _jobName(NULL)

	, _Status(STATUS_READY)
#ifdef MEMORY_HOG
#else
	, _parent(NO_ID)
	, _child(NO_ID)
	, _numparents(0)
	, _multiple_parents(false)
	, _multiple_children(false)
	, _parents_done(false)
	, _spare(false)
#endif
	, _jobID(-1)
	, _jobstateSeqNum(0)
	, _preskip(PRE_SKIP_INVALID)
	, _lastEventTime(0)
	, _throttleInfo(NULL)
	, _jobTag(NULL)
{
	ASSERT( jobName != NULL );
	ASSERT( cmdFile != NULL );

	debug_printf( DEBUG_DEBUG_1, "Job::Job(%s, %s, %s)\n", jobName, directory, cmdFile);

	_jobName = strdup (jobName);

	// Initialize _directory and _cmdFile in a de-duped stringSpace since
	// these strings may be repeated in thousands of nodes
	_directory = stringSpace.strdup_dedup(directory);
	ASSERT(_directory);
	_cmdFile = stringSpace.strdup_dedup(cmdFile);
	ASSERT(_cmdFile);

	// _condorID struct initializes itself

	// jobID is a primary key (a database term).  All should be unique
	_jobID = _jobID_counter++;

	error_text.clear();

	return;
}

//---------------------------------------------------------------------------
void
Job::PrefixDirectory(MyString &prefix)
{
	MyString newdir;

	// don't add an unnecessary prefix
	if (prefix == ".") {
		return;
	}
	
	// If the job DIR is absolute, leave it alone
	if (_directory[0] == '/') {
		return;
	}

	// otherwise, prefix it.

	newdir += prefix;
	newdir += "/";
	newdir += _directory;

    stringSpace.free_dedup(_directory);

	_directory = stringSpace.strdup_dedup(newdir.c_str());
    ASSERT(_directory);
}

#ifdef DEAD_CODE
//---------------------------------------------------------------------------
bool Job::Remove (const queue_t queue, const JobID_t jobID)
{
	if (_queues[queue].erase(jobID) == 0) {
		return false; // element not found
	}

	return true;
}  
#endif

//---------------------------------------------------------------------------
void Job::Dump ( const Dag *dag ) const {
    dprintf( D_ALWAYS, "---------------------- Job ----------------------\n");
    dprintf( D_ALWAYS, "      Node Name: %s\n", _jobName );
    dprintf( D_ALWAYS, "           Noop: %s\n", _noop ? "true" : "false" );
    dprintf( D_ALWAYS, "         NodeID: %d\n", _jobID );
    dprintf( D_ALWAYS, "    Node Status: %s\n", GetStatusName() );
    dprintf( D_ALWAYS, "Node return val: %d\n", retval );
	if( _Status == STATUS_ERROR ) {
		dprintf( D_ALWAYS, "          Error: %s\n", error_text.c_str() );
	}
    dprintf( D_ALWAYS, "Job Submit File: %s\n", _cmdFile );
	if( _scriptPre ) {
		dprintf( D_ALWAYS, "     PRE Script: %s\n", _scriptPre->GetCmd() );
	}
	if( _scriptPost ) {
		dprintf( D_ALWAYS, "    POST Script: %s\n", _scriptPost->GetCmd() );
	}
	if( _scriptHold ) {
		dprintf( D_ALWAYS, "    HOLD Script: %s\n", _scriptHold->GetCmd() );
	}
	if( retry_max > 0 ) {
		dprintf( D_ALWAYS, "          Retry: %d\n", retry_max );
	}
	if( _CondorID._cluster == -1 ) {
		dprintf( D_ALWAYS, " %7s Job ID: [not yet submitted]\n",
				 JobTypeString() );
	}
	else {
		dprintf( D_ALWAYS, " %7s Job ID: (%d.%d.%d)\n", JobTypeString(),
				 _CondorID._cluster, _CondorID._proc, _CondorID._subproc );
	}

#ifdef DEAD_CODE
    for (int i = 0 ; i < 3 ; i++) {
        dprintf( D_ALWAYS, "%15s: ", queue_t_names[i] );

		std::set<JobID_t>::const_iterator qit;
		for (qit = _queues[i].begin(); qit != _queues[i].end(); qit++) {
			Job *node = dag->Dag::FindNodeByNodeID( *qit );
			dprintf( D_ALWAYS | D_NOHEADER, "%s, ", node->GetJobName() );
		}
        dprintf( D_ALWAYS | D_NOHEADER, "<END>\n" );
    }
#else
	std::string parents, children;
	PrintParents(parents, 1024, dag, " ");
	PrintChildren(children, 1024, dag, " ");
	dprintf(D_ALWAYS, "PARENTS: %s WAITING: %d CHILDREN: %s\n", parents.c_str(), (int)IsWaiting(), children.c_str());
#endif
}

#if 0 // not used -- wenger 2015-02-17
//---------------------------------------------------------------------------
void Job::Print (bool condorID) const {
    dprintf( D_ALWAYS, "ID: %4d Name: %s", _jobID, _jobName);
    if (condorID) {
        dprintf( D_ALWAYS, "  CondorID: (%d.%d.%d)", _CondorID._cluster,
				 _CondorID._proc, _CondorID._subproc );
    }
}

//---------------------------------------------------------------------------
void job_print (Job * job, bool condorID) {
    if (job == NULL) {
		dprintf( D_ALWAYS, "(UNKNOWN)");
	} else {
		job->Print(condorID);
	}
}
#endif

const char*
Job::GetPreScriptName() const
{
	if( !_scriptPre ) {
		return NULL;
	}
	return _scriptPre->GetCmd();
}

const char*
Job::GetPostScriptName() const
{
	if( !_scriptPost ) {
		return NULL;
	}
	return _scriptPost->GetCmd();
}

const char*
Job::GetHoldScriptName() const
{
	if( !_scriptHold ) {
		return NULL;
	}
	return _scriptHold->GetCmd();
}

bool
Job::SanityCheck() const
{
	bool result = true;

	if( countedAsDone == true && _Status != STATUS_DONE ) {
		dprintf( D_ALWAYS, "BADNESS 10000: countedAsDone == true but "
				 "_Status != STATUS_DONE\n" );
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


bool
Job::SetStatus( status_t newStatus )
{
	debug_printf( DEBUG_DEBUG_1, "Job(%s)::_Status = %s\n",
		GetJobName(), status_t_names[_Status] );

	debug_printf( DEBUG_DEBUG_1, "Job(%s)::SetStatus(%s)\n",
		GetJobName(), status_t_names[newStatus] );
	
	_Status = newStatus;
		// TODO: add some state transition sanity-checking here?
	return true;
}

//---------------------------------------------------------------------------
bool
Job::GetProcIsIdle( int proc )
{
	if ( GetNoop() ) {
		proc = 0;
	}

#ifdef DEAD_CODE
	if ( proc >= static_cast<int>( _isIdle.size() ) ) {
		_isIdle.resize( proc+1, false );
	}
	return _isIdle[proc];
#else
	if (proc >= static_cast<int>(_gotEvents.size())) {
		_gotEvents.resize(proc + 1, 0);
	}
	return (_gotEvents[proc] & IDLE_MASK) != 0;
#endif
}

//---------------------------------------------------------------------------
void
Job::SetProcIsIdle( int proc, bool isIdle )
{
	if ( GetNoop() ) {
		proc = 0;
	}

#ifdef DEAD_CODE
	if ( proc >= static_cast<int>( _isIdle.size() ) ) {
		_isIdle.resize( proc+1, false );
	}
	_isIdle[proc] = isIdle;
#else
	if (proc >= static_cast<int>(_gotEvents.size())) {
		_gotEvents.resize(proc + 1, 0);
	}
	if (isIdle) {
		_gotEvents[proc] |= IDLE_MASK;
	} else {
		_gotEvents[proc] &= ~IDLE_MASK;
	}
#endif
}

//---------------------------------------------------------------------------
void
Job::SetProcEvent( int proc, int event )
{
	if ( GetNoop() ) {
		proc = 0;
	}

	if (proc >= static_cast<int>(_gotEvents.size())) {
			_gotEvents.resize(proc + 1, 0);
	}

	_gotEvents[proc] |= event;
}

//---------------------------------------------------------------------------
void
Job::PrintProcIsIdle()
{
#ifdef DEAD_CODE
	for ( int proc = 0;
				proc < static_cast<int>( _isIdle.size() ); ++proc ) {
		debug_printf( DEBUG_QUIET, "  Job(%s)::_isIdle[%d]: %d\n",
					GetJobName(), proc, _isIdle[proc] );
	}
#else
	for (int proc = 0;
		proc < static_cast<int>(_gotEvents.size()); ++proc) {
		debug_printf(DEBUG_QUIET, "  Job(%s)::_isIdle[%d]: %d\n",
			GetJobName(), proc, (_gotEvents[proc] & IDLE_MASK) != 0);
	}
#endif
}

#ifdef DEAD_CODE

//---------------------------------------------------------------------------
bool
Job::AddParent( Job* parent )
{
	bool success;
	MyString whynot;
	success = AddParent( parent, whynot );
	if( !success ) {
		debug_printf( DEBUG_QUIET,
					  "ERROR: AddParent( %s ) failed for node %s: %s\n",
					  parent ? parent->GetJobName() : "(null)",
					  this->GetJobName(), whynot.Value() );
	}
	return success;
}


bool
Job::AddParent( Job* parent, MyString &whynot )
{
	if( !this->CanAddParent( parent, whynot ) ) {
		return false;
	}

	if( HasParent( parent ) ) {
		debug_printf( DEBUG_QUIET,
					"Warning: child %s already has parent %s\n",
					GetJobName(), parent->GetJobName() );
		check_warning_strictness( DAG_STRICT_3 );
		return true;
	}

	if( !Add( Q_PARENTS, parent->GetJobID() ) ) {
		whynot = "unknown error appending to PARENTS queue";
		return false;
	}
    if( parent->GetStatus() != STATUS_DONE ) {
		if( !Add( Q_WAITING, parent->GetJobID() ) ) {
            // this node's dependency queues are now out of sync and
            // thus the DAG state is FUBAR, so we should bail...
			EXCEPT( "Failed to add parent %s to job %s",
						parent->GetJobName(), GetJobName() );
			return false;
		}
	}
	whynot = "n/a";
    return true;
}
#else

// visit all of the children, either marking them, or checking for cycles
int Job::CountChildren() const
{
	int count = 0;
#ifdef MEMORY_HOG
	count = (int)_children.size();
#else
	if (_child != NO_ID) {
		if (_multiple_children) {
			Edge * edge = Edge::ById(_child);
			ASSERT(edge);
		#if 1 // if the child list has a size method
			count = (int)edge->size();
		#else // if it does not
			for (auto it = edge->_children.begin(); it != edge->_children.end(); ++it) {
				++count;
			}
		#endif
		} else {
			count = 1;
		}
	}
#endif
	return count;
}


bool Job::ParentComplete(Job * parent)
{
	bool fail = true;
	int num_waiting = 0;
#ifdef MEMORY_HOG
	fail = _waiting.erase(parent->GetJobID()) != 1;
	num_waiting = (int)_waiting.size();
#else

 #if 1
	bool already_done = _parents_done;
	if (_parent != NO_ID) {
		if (_multiple_parents) {
			WaitEdge * edge = WaitEdge::ById(_parent);
			fail = ! edge->MarkDone(parent->GetJobID(), already_done);
			num_waiting = edge->Waiting();
			_parents_done = num_waiting == 0;
		} else {
			num_waiting = _parents_done ? 0 : 1;
			if (parent->GetJobID() == _parent) {
				fail = false;
				_parents_done = true;
				num_waiting = 0;
			}
		}
	}
 #else
	static int log_count = 0;
	if (_waiting != NO_ID) {
		if (_multiple_waiting) {
			Edge * edge = Edge::ById(_waiting);
			PRAGMA_REMIND("if Edge instances are shared, it's ok to fail to remove a parent here..")
			fail = ! edge->Remove(parent->GetJobID());
			if (fail) {
				if (++log_count < 10) {
					debug_printf(DEBUG_QUIET,
						"ERROR: ParentComplete( %s ) failed for multi-parent child node %s: waiting=%d %d parent=%d %d\n",
						parent ? parent->GetJobName() : "(null)",
						this->GetJobName(),
						_waiting, (int)edge->size(),
						_parent, (int)Edge::ById(_parent)->size());
				}
				fail = false;
			}
			if (edge->empty()) {
				_waiting = NO_ID;
				_multiple_waiting = false;
				num_waiting = 0;
			} else if (fail) {
				num_waiting = (int)edge->size();
			}
		} else {
			num_waiting = 1;
			if (parent->GetJobID() == _waiting) {
				fail = false;
				_waiting = NO_ID;
				num_waiting = 0;
			}
		}
		if (fail) {
			if (++log_count < 10) {
				debug_printf(DEBUG_QUIET,
					"ERROR: ParentComplete( %s ) failed for multi-parent child node %s: waiting=%d %d parent=%d %d\n",
					parent ? parent->GetJobName() : "(null)",
					this->GetJobName(),
					_waiting, num_waiting,
					_parent, (int)Edge::ById(_parent)->size());
			}
			fail = false;
		}
	}
 #endif
#endif
	if (fail) {
		debug_printf(DEBUG_QUIET,
			"ERROR: ParentComplete( %s ) failed for child node %s: num_waiting=%d\n",
			parent ? parent->GetJobName() : "(null)",
			this->GetJobName(),
			num_waiting);
	}
	return ! IsWaiting();
}

int Job::PrintParents(std::string & buf, size_t bufmax, const Dag* dag, const char * sep) const
{
	int count = 0;
#ifdef MEMORY_HOG
	for (auto it = _parents.begin(); it != _parents.end(); ++it) {
		if (buf.size() >= bufmax)
			break;

		Job * parent = dag->FindNodeByNodeID(*it);
		ASSERT(parent != NULL);

		if (count > 0) buf += sep;
		buf += parent->GetJobName();
		++count;
	}
#else
	if (_parent != NO_ID) {
		if (_multiple_parents) {
			Edge * edge = Edge::ById(_parent);
			ASSERT(edge);
			if ( ! edge->_ary.empty()) {
				for (auto it = edge->_ary.begin(); it != edge->_ary.end(); ++it) {
					if (buf.size() >= bufmax)
						break;

					Job * parent = dag->FindNodeByNodeID(*it);
					ASSERT(parent != NULL);

					if (count > 0) buf += sep;
					buf += parent->GetJobName();
					++count;
				}
			}
		} else {
			Job* parent = dag->FindNodeByNodeID(_parent);
			ASSERT(parent != NULL);
			buf += parent->GetJobName();
			++count;
		}
	}
#endif
	return count;
}

int Job::PrintChildren(std::string & buf, size_t bufmax, const Dag* dag, const char * sep) const
{
	int count = 0;
#ifdef MEMORY_HOG
	for (auto it = _children.begin(); it != _children.end(); ++it) {
		if (buf.size() >= bufmax)
			break;

		Job * child = dag->FindNodeByNodeID(*it);
		ASSERT(child != NULL);

		if (count > 0) buf += sep;
		buf += child->GetJobName();
		++count;
	}
#else
	if (_child != NO_ID) {
		if (_multiple_children) {
			Edge * edge = Edge::ById(_child);
			ASSERT(edge);
			if ( ! edge->_ary.empty()) {
				for (auto it = edge->_ary.begin(); it != edge->_ary.end(); ++it) {
					if (buf.size() >= bufmax)
						break;

					Job * child = dag->FindNodeByNodeID(*it);
					ASSERT(child != NULL);

					if (count > 0) buf += sep;
					buf += child->GetJobName();
					++count;
				}
			}
		} else {
			Job* child = dag->FindNodeByNodeID(_child);
			ASSERT(child != NULL);
			buf += child->GetJobName();
			++count;
		}
	}
#endif
	return count;
}

// tell children that the parent is complete, and call the given function
// for children that have no more incomplete parents
//
int Job::NotifyChildren(Dag& dag, bool(*pfn)(Dag& dag, Job* child))
{
	int count = 0;
#ifdef MEMORY_HOG
	for (auto it = _children.begin(); it != _children.end(); ++it) {
		Job * child = dag.FindNodeByNodeID(*it);
		ASSERT(child != NULL);
		if (child->ParentComplete(this)) { // returns ! child->IsWaiting()
			if (pfn) pfn(dag, child);
		}
	}
#else
	if (_child != NO_ID) {
		if (_multiple_children) {
			Edge * edge = Edge::ById(_child);
			ASSERT(edge);
			if ( ! edge->_ary.empty()) {
				for (auto it = edge->_ary.begin(); it != edge->_ary.end(); ++it) {
					Job * child = dag.FindNodeByNodeID(*it);
					ASSERT(child != NULL);
					if (child->ParentComplete(this)) {
						if (pfn) pfn(dag, child);
					}
				}
			}
		} else {
			Job* child = dag.FindNodeByNodeID(_child);
			ASSERT(child != NULL);
			if (child->ParentComplete(this)) {
				if (pfn) pfn(dag, child);
			}
		}
	}
#endif
	return count;
}

// visit all of the children, either marking them, or checking for cycles
int Job::VisitChildren(Dag& dag, int(*pfn)(Dag& dag, Job* parent, Job* child, void* args), void* args)
{
	int retval = 0;
#ifdef MEMORY_HOG
	for (auto it = _children.begin(); it != _children.end(); ++it) {
		Job * child = dag.FindNodeByNodeID(*it);
		ASSERT(child != NULL);
		retval += pfn(dag, this, child, args);
	}
#else
	if (_child != NO_ID) {
		if (_multiple_children) {
			Edge * edge = Edge::ById(_child);
			ASSERT(edge);
			if (! edge->_ary.empty()) {
				for (auto it = edge->_ary.begin(); it != edge->_ary.end(); ++it) {
					Job * child = dag.FindNodeByNodeID(*it);
					ASSERT(child != NULL);
					retval += pfn(dag, this, child, args);
				}
			}
		} else {
			Job* child = dag.FindNodeByNodeID(_child);
			ASSERT(child != NULL);
			retval += pfn(dag, this, child, args);
		}
	}
#endif
	return retval;
}


#endif

bool
Job::CanAddParent( Job* parent, MyString &whynot )
{
	if( !parent ) {
		whynot = "parent == NULL";
		return false;
	}
	if( GetType() == NodeType::FINAL ) {
		whynot = "Tried to add a parent to a Final node";
		return false;
	}

		// we don't currently allow a new parent to be added to a
		// child that has already been started (unless the parent is
		// already marked STATUS_DONE, e.g., when rebuilding from a
		// rescue DAG) -- but this restriction might be lifted in the
		// future once we figure out the right way for the DAG to
		// respond...
	if( _Status != STATUS_READY && parent->GetStatus() != STATUS_DONE ) {
		whynot.formatstr( "%s child may not be given a new %s parent",
						this->GetStatusName(), parent->GetStatusName() );
		return false;
	}
	whynot = "n/a";
	return true;
}

bool Job::CanAddChildren(std::forward_list<Job*> & children, MyString &whynot)
{
	if ( GetType() == NodeType::FINAL ) {
		whynot = "Tried to add a child to a final node";
		return false;
	}
	if ( GetType() == NodeType::PROVISIONER ) {
		whynot = "Tried to add a child to a provisioner node";
		return false;
	}

	for (auto it = children.begin(); it != children.end(); ++it) {
		Job* child = *it;
		if ( ! child->CanAddParent(this, whynot)) {
			return false;
		}
	}

	return true;
}

bool Job::AddVar(const char *name, const char *value, const char * filename, int lineno)
{
	name = dedup_str(name);
	value = dedup_str(value);
	auto last_var = varsFromDag.before_begin();
	for (auto it = varsFromDag.begin(); it != varsFromDag.end(); ++it) {
		last_var = it;
		// because we dedup the names, we can just compare the pointers here.
		if (name == it->_name) {
			debug_printf(DEBUG_NORMAL, "Warning: VAR \"%s\" "
				"is already defined in job \"%s\" "
				"(Discovered at file \"%s\", line %d)\n",
					name, GetJobName(), filename, lineno);
				check_warning_strictness(DAG_STRICT_3);
				debug_printf(DEBUG_NORMAL, "Warning: Setting VAR \"%s\" = \"%s\"\n", name, value);
				it->_value = value;
				return true;
		}
	}
	varsFromDag.emplace_after(last_var, name, value);
	return true;
}

int Job::PrintVars(std::string &vars)
{
	int num_vars = 0;
	for (auto it = varsFromDag.begin(); it != varsFromDag.end(); ++it) {
		vars.push_back(' ');
		vars.append(it->_name);
		vars.push_back('=');
		vars.push_back('\"');
		// now we print the value, but we have to re-escape certain characters
		const char * p = it->_value;
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

#ifdef DEAD_CODE
bool
Job::AddChild( Job* child )
{
	bool success;
	MyString whynot;
	success = AddChild( child, whynot );
	if( !success ) {
		debug_printf( DEBUG_QUIET,
					  "ERROR: AddChild( %s ) failed for node %s: %s\n",
					  child ? child->GetJobName() : "(null)",
					  this->GetJobName(), whynot.Value() );
	}
	return success;
}


bool
Job::AddChild( Job* child, MyString &whynot )
{
	if( !this->CanAddChild( child, whynot ) ) {
		return false;
	}

	if( HasChild( child ) ) {
		debug_printf( DEBUG_NORMAL,
					"Warning: parent %s already has child %s\n",
					GetJobName(), child->GetJobName() );
		check_warning_strictness( DAG_STRICT_3 );
		return true;
	}

	if( !Add( Q_CHILDREN, child->GetJobID() ) ) {
		whynot = "unknown error appending to CHILDREN queue";
		return false;
	}
	whynot = "n/a";
    return true;
}
#else


bool Job::AddChildren(std::forward_list<Job*> &children, MyString &whynot)
{
	// check if all of this can be our child, and if all are ok being our children
	if ( ! CanAddChildren(children, whynot)) {
		return false;
	}

	// optimize the insertion case for more than a single child
	// into current a single or empty edge
	if (more_than_one(children) && ! _multiple_children) {
		JobID_t id = _child;
		Edge * edge = Edge::PromoteToMultiple(_child, _multiple_children, id);

		// count the children so we can reserve space in the edge array
		int num_children = (id == NO_ID) ? 0 : 1;
		for (auto it = children.begin(); it != children.end(); ++it) { ++num_children; }
		edge->_ary.reserve(num_children);

		// populate the edge array, since we know that children is sorted we can just push_back here.
		for (auto it = children.begin(); it != children.end(); ++it) {
			Job* child = *it;
			edge->_ary.push_back(child->GetJobID());
			// count the parents of the children so that we can allocate space in AdjustEdges
			child->_numparents += 1;
		}
		if (id != NO_ID) { edge->Add(id); }
		return true;
	}

	for (auto it = children.begin(); it != children.end(); ++it) {
		Job* child = *it;

#ifdef MEMORY_HOG
		auto ret = _children.insert(child->GetJobID());
		if (ret.second == false) {
			debug_printf(DEBUG_NORMAL,
				"Warning: parent %s already has child %s\n",
				GetJobName(), child->GetJobName());
			check_warning_strictness(DAG_STRICT_3);
		} else {
			child->addParent(this);
		}
#else
		// if we have no children, add this as a direct child
		if (_child == NO_ID) {
			_multiple_children = false;
			_child = child->GetJobID();
			// count the parents of the children so that we can allocate space in AdjustEdges
			child->_numparents += 1;
			continue;
		}

		JobID_t id = NO_ID;
		Edge * edge = Edge::PromoteToMultiple(_child, _multiple_children, id);
		if (id != NO_ID) {
			// insert the old _child id as the first id in the collection
			edge->Add(id);
		}
		if ( ! edge->Add(child->GetJobID())) {
			debug_printf(DEBUG_NORMAL,
				"Warning: parent %s already has child %s\n",
				GetJobName(), child->GetJobName());
			check_warning_strictness(DAG_STRICT_3);
		} else {
			// count parents - used by AdjustEdges to reserve space
			child->_numparents += 1;
		}
#endif
	}
	return true;
}

void Job::BeginAdjustEdges(Dag* /*dag*/)
{
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

// helper for AdjustEdges, assumes that _parent has been resized by BeginAdjustEdges
// we can't mark parents done here, that can only happen after we built all of the parents
void Job::AdjustEdges_AddParentToChild(Dag* dag, JobID_t child_id, Job* parent)
{
	JobID_t parent_id = parent->GetJobID();
	Job * child = dag->FindNodeByNodeID(child_id);
	ASSERT(child != NULL);
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

// update the waiting edges to contain the unfinished parents
void Job::AdjustEdges(Dag* dag)
{
#ifdef MEMORY_HOG
	for (auto it = _parents.begin(); it != _parents.end(); ++it) {
		Job * job = dag->FindNodeByNodeID(*it);
		if (job->GetStatus() == STATUS_DONE) {
			_waiting.erase(*it);
		} else {
			_waiting.insert(*it);
		}
	}
#else
	// build parents from children
	if (_child != NO_ID) {
		if (_multiple_children) {
			Edge * edge = Edge::ById(_child);
			ASSERT(edge);
			if ( ! edge->_ary.empty()) {
				for (auto it = edge->_ary.begin(); it != edge->_ary.end(); ++it) {
					AdjustEdges_AddParentToChild(dag, *it, this);
				}
			}
		} else {
			AdjustEdges_AddParentToChild(dag, _child, this);
		}
	}
#endif
}


#if 0
// diabled becuase marking the children as done here confuses bootstrap
void Job::AdjustEdges_NotifyChild(Dag* dag, JobID_t child_id, Job* parent)
{
	JobID_t parent_id = parent->GetJobID();
	Job * child = dag->FindNodeByNodeID(child_id);
	ASSERT(child != NULL);
	bool already_done = child->_parents_done;
	if (child->_parent == NO_ID) {
		child->_parents_done = true;
	} else if (child->_parent == parent_id) {
		child->_parents_done = true;
	} else {
		ASSERT(child->_numparents > 1);
		ASSERT(child->_multiple_parents);
		WaitEdge* wedge = WaitEdge::ById(child->_parent);
		wedge->MarkDone(parent_id, already_done);
		child->_parents_done = ! wedge->Waiting();
	}
}

#endif

void Job::FinalizeAdjustEdges(Dag* /*dag*/)
{
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

	// if I'm done, tell my children
#if 0
	// if we do this here, bootstrap will get confused
	//  this code is left for reference - but disabled.
	if (GetStatus() == STATUS_DONE) {
		if (_child != NO_ID) {
			if (_multiple_children) {
				Edge * edge = Edge::ById(_child);
				ASSERT(edge);
				if (! edge->_ary.empty()) {
					for (auto it = edge->_ary.begin(); it != edge->_ary.end(); ++it) {
						AdjustEdges_NotifyChild(dag, *it, this);
					}
				}
			} else {
				AdjustEdges_NotifyChild(dag, _child, this);
			}
		}
	}
#else
	ASSERT(GetStatus() != STATUS_DONE);
#endif
}


#endif

bool
Job::CanAddChild( Job* child, MyString &whynot ) const
{
	if( !child ) {
		whynot = "child == NULL";
		return false;
	}
	if( GetType() == NodeType::FINAL ) {
		whynot = "Tried to add a child to a final node";
		return false;
	}
	if( GetType() == NodeType::PROVISIONER ) {
		whynot = "Tried to add a child to a provisioner node";
		return false;
	}
	whynot = "n/a";
	return true;
}


bool
Job::TerminateSuccess()
{
	SetStatus( STATUS_DONE );
	return true;
} 

bool
Job::TerminateFailure()
{
	SetStatus( STATUS_ERROR );
	return true;
} 

#ifdef DEAD_CODE
bool
Job::Add( const queue_t queue, const JobID_t jobID )
{
	std::pair<std::set<JobID_t>::iterator, bool> ret;

	ret = _queues[queue].insert(jobID);

	if (ret.second == false) {
		dprintf( D_ALWAYS,
				 "ERROR: can't add Job ID %d to DAG: already present!",
				 jobID );
		return false;
	}

	return true;
}
#else
#endif

bool
Job::AddScript( ScriptType script_type, const char *cmd, int defer_status, time_t defer_time, MyString &whynot )
{
	if( !cmd || strcmp( cmd, "" ) == 0 ) {
		whynot = "missing script name";
		return false;
	}

	// Check if a script of the same type has already been assigned to this node
	const char *old_script_name = NULL;
	const char *type_name;
	switch( script_type ) {
		case ScriptType::PRE:
			old_script_name = GetPreScriptName();
			type_name = "PRE";
			break;
		case ScriptType::POST:
			old_script_name = GetPostScriptName();
			type_name = "POST";
			break;
		case ScriptType::HOLD:
			old_script_name = GetHoldScriptName();
			type_name = "HOLD";
			break;
	}
	if( old_script_name ) {
		debug_printf( DEBUG_NORMAL,
			"Warning: node %s already has %s script <%s> assigned; changing "
			"to <%s>\n", GetJobName(), type_name, old_script_name, cmd );
	}

	Script* script = new Script( script_type, cmd, defer_status, defer_time, this );
	if( !script ) {
		dprintf( D_ALWAYS, "ERROR: out of memory!\n" );
			// we already know we're out of memory, so filling in
			// whynot will likely fail, but give it a shot...
		whynot = "out of memory!";
		return false;
	}
	if( script_type == ScriptType::POST ) {
		_scriptPost = script;
	}
	else if( script_type == ScriptType::PRE ) {
		_scriptPre = script;
	}
	else if( script_type == ScriptType::HOLD ) {
		_scriptHold = script;
	}
	whynot = "n/a";
	return true;
}

bool
Job::AddPreSkip( int exitCode, MyString &whynot )
{
	if ( exitCode < PRE_SKIP_MIN || exitCode > PRE_SKIP_MAX ) {
		whynot.formatstr( "PRE_SKIP exit code must be between %d and %d\n",
			PRE_SKIP_MIN, PRE_SKIP_MAX );
		return false;
	}

	if ( exitCode == 0 ) {
		debug_printf( DEBUG_NORMAL, "Warning: exit code 0 for a PRE_SKIP "
			"value is weird.\n");
	}

	if ( _preskip != PRE_SKIP_INVALID ) {
		debug_printf( DEBUG_NORMAL,
					"Warning: new PRE_SKIP value  %d for node %s overrides old value %d\n",
					exitCode, GetJobName(), _preskip );
		check_warning_strictness( DAG_STRICT_3 );
	}
	_preskip = exitCode;	

	whynot = "";
	return true;
}

bool
Job::IsActive() const
{
	return  _Status == STATUS_PRERUN || _Status == STATUS_SUBMITTED ||
		_Status == STATUS_POSTRUN;
}

const char*
Job::GetStatusName() const
{
		// Put in bounds check here?
	return status_t_names[_Status];
}

#ifdef DEAD_CODE

bool
Job::HasChild( Job* child ) {
	JobID_t cid;
	std::set<JobID_t>::iterator it;

	if( !child ) {
		return false;
	}

	cid = child->GetJobID();
	it = _queues[Q_CHILDREN].find(cid);

	if (it == _queues[Q_CHILDREN].end()) {
		return false;
	}

	return true;
}

bool
Job::HasParent( Job* parent ) {
	JobID_t pid;
	std::set<JobID_t>::iterator it;

	if( !parent ) {
		return false;
	}

	pid = parent->GetJobID();
	it = _queues[Q_PARENTS].find(pid);

	if (it == _queues[Q_PARENTS].end()) {
		return false;
	}

	return true;
}


bool
Job::RemoveChild( Job* child )
{
	bool success;
	MyString whynot;
	success = RemoveChild( child, whynot );
	if( !success ) {
		debug_printf( DEBUG_QUIET,
					  "ERROR: RemoveChild( %s ) failed for node %s: %s\n",
                      child ? child->GetJobName() : "(null)",
                      this->GetJobName(), whynot.Value() );
	}
	return success;
}


bool
Job::RemoveChild( Job* child, MyString &whynot )
{
	if( !child ) {
		whynot = "child == NULL";
		return false;
	}
	return RemoveDependency( Q_CHILDREN, child->GetJobID(), whynot );
}


bool
Job::RemoveParent( Job* parent, MyString &whynot )
{
	if( !parent ) {
		whynot = "parent == NULL";
		return false;
	}
	return RemoveDependency( Q_PARENTS, parent->GetJobID(), whynot );
}

bool
Job::RemoveDependency( queue_t queue, JobID_t job )
{
	MyString whynot;
	return RemoveDependency( queue, job, whynot );
}

bool
Job::RemoveDependency( queue_t queue, JobID_t job, MyString &whynot )
{
	if (_queues[queue].erase(job) == 0)
	{
		whynot = "no such dependency";
		return false;
	}

	whynot = "n/a";
	return true;
}

int
Job::NumParents() const
{
	return _queues[Q_PARENTS].size();
}

int
Job::NumChildren() const
{
	return _queues[Q_CHILDREN].size();
}
#else
#endif

void
Job::SetCategory( const char *categoryName, ThrottleByCategory &catThrottles )
{
	ASSERT( _type != NodeType::FINAL );

	MyString	tmpName( categoryName );

	if ( (_throttleInfo != NULL) &&
				(tmpName != *(_throttleInfo->_category)) ) {
		debug_printf( DEBUG_NORMAL, "Warning: new category %s for node %s "
					"overrides old value %s\n", categoryName, GetJobName(),
					_throttleInfo->_category->c_str() );
		check_warning_strictness( DAG_STRICT_3 );
	}

		// Note: we must assign a ThrottleInfo here even if the name
		// already matches, for the case of lifting splices.
	ThrottleByCategory::ThrottleInfo *oldInfo = _throttleInfo;

	ThrottleByCategory::ThrottleInfo *throttleInfo =
				catThrottles.GetThrottleInfo( &tmpName );
	if ( throttleInfo != NULL ) {
		_throttleInfo = throttleInfo;
	} else {
		_throttleInfo = catThrottles.AddCategory( &tmpName );
	}

	if ( oldInfo != _throttleInfo ) {
		if ( oldInfo != NULL ) {
			oldInfo->_totalJobs--;
		}
		_throttleInfo->_totalJobs++;
	}
}

void
Job::PrefixName(const MyString &prefix)
{
	MyString tmp = prefix + _jobName;

	free(_jobName);

	_jobName = strdup(tmp.c_str());
}


#ifdef DEAD_CODE
// iterate across the Job's var values, and for any which have $(JOB) in them, 
// substitute it. This substitution is draconian and will always happen.
void
Job::ResolveVarsInterpolations(void)
{
	NodeVar *var;

	varsFromDag->Rewind();
	while( (var = varsFromDag->Next()) != NULL ) {
		// XXX No way to escape $(JOB) in case, for some crazy reason, you
		// want a filename component actually to be '$(JOB)'.
		// It isn't hard to fix, I'll do it later.
		var->_value.replaceString("$(JOB)", GetJobName());
	}
}
#endif

//---------------------------------------------------------------------------
void
Job::SetDagFile(const char *dagFile)
{
	if (_dagFile) free(_dagFile);
	_dagFile = strdup( dagFile );
}

//---------------------------------------------------------------------------
const char *
Job::GetJobstateJobTag()
{
	if ( !_jobTag ) {
		MyString jobTagName = MultiLogFiles::loadValueFromSubFile(
					_cmdFile, _directory, JOB_TAG_NAME );
		if ( jobTagName == "" ) {
			jobTagName = PEGASUS_SITE;
		} else {
				// Remove double-quotes
			int begin = jobTagName[0] == '\"' ? 1 : 0;
			int last = jobTagName.length() - 1;
			int end = jobTagName[last] == '\"' ? last - 1 : last;
			jobTagName = jobTagName.substr( begin, 1 + end - begin );
		}

		MyString tmpJobTag = MultiLogFiles::loadValueFromSubFile(
					_cmdFile, _directory, jobTagName.c_str() );
		if ( tmpJobTag == "" ) {
			tmpJobTag = "-";
		} else {
				// Remove double-quotes
			int begin = tmpJobTag[0] == '\"' ? 1 : 0;
			int last = tmpJobTag.length() - 1;
			int end = tmpJobTag[last] == '\"' ? last - 1 : last;
			tmpJobTag = tmpJobTag.substr( begin, 1 + end - begin );
		}
		_jobTag = strdup( tmpJobTag.c_str() );
	}

	return _jobTag;
}

//---------------------------------------------------------------------------
int
Job::GetJobstateSequenceNum()
{
	if ( _jobstateSeqNum == 0 ) {
		_jobstateSeqNum = _nextJobstateSeqNum++;
	}

	return _jobstateSeqNum;
}

//---------------------------------------------------------------------------
void
Job::SetLastEventTime( const ULogEvent *event )
{
	_lastEventTime = event->GetEventclock();
}

//---------------------------------------------------------------------------
int
Job::GetPreSkip() const
{
	if( !HasPreSkip() ) {
		debug_printf( DEBUG_QUIET,
			"Evaluating PRE_SKIP... It is not defined.\n" );
	}
	return _preskip;
}

//---------------------------------------------------------------------------
bool
Job::SetCondorID(const CondorID& cid)
{
	bool ret = true;
	if(GetCluster() != -1) {
		debug_printf( DEBUG_NORMAL, "Reassigning the id of job %s from (%d.%d.%d) to "
			"(%d.%d.%d)\n", GetJobName(), GetCluster(), GetProc(), GetSubProc(),
			cid._cluster, cid._proc,cid._subproc );
			ret = false;
	}
	_CondorID = cid;
	return ret;	
}

//---------------------------------------------------------------------------
bool
Job::Hold(int proc) 
{
#ifdef DEAD_CODE
	if( proc >= static_cast<int>( _onHold.size() ) ) {
		_onHold.resize( proc+1, 0 );
	}
	if( !_onHold[proc] ) {
		_onHold[proc] = 1;
#else
	if (proc >= static_cast<int>(_gotEvents.size())) {
		_gotEvents.resize(proc + 1, 0);
	}
	if ((_gotEvents[proc] & HOLD_MASK) != HOLD_MASK) {
		_gotEvents[proc] |= HOLD_MASK;
#endif
		++_jobProcsOnHold;
		++_timesHeld;
		return true;
	} else {
		dprintf( D_FULLDEBUG, "Received hold event for node %s, and job %d.%d "
			"is already on hold!\n", GetJobName(), GetCluster(), proc );
	}
	return false;
}

//---------------------------------------------------------------------------
bool
Job::Release(int proc)
{
#ifdef DEAD_CODE
	if( proc >= static_cast<int>( _onHold.size() ) ) {
		dprintf( D_FULLDEBUG, "Received release event for node %s, but job %d.%d "
			"is not on hold\n", GetJobName(), GetCluster(), GetProc() );
		return false; // We never marked this as being on hold
	}
	if( _onHold[proc] ) {
		_onHold[proc] = 0;
#else
	//PRAGMA_REMIND("tj: this should also test the flags, not just the vector size")
	if (proc >= static_cast<int>(_gotEvents.size())) {
		dprintf(D_FULLDEBUG, "Received release event for node %s, but job %d.%d "
			"is not on hold\n", GetJobName(), GetCluster(), GetProc());
		return false; // We never marked this as being on hold
	}
	if (_gotEvents[proc] & HOLD_MASK) {
		_gotEvents[proc] &= ~HOLD_MASK;
#endif
		--_jobProcsOnHold;
		return true;
	}
	return false;
}

//---------------------------------------------------------------------------
// Note:  For multi-proc jobs, if one proc failed this was getting called
// immediately, which was not correct.  I changed how this was called, but
// we should probably put in code to make sure it's not called too soon,
// but is called...  wenger 2015-11-05
void
Job::Cleanup()
{
#ifdef DEAD_CODE
	std::vector<unsigned char> s;
	_onHold.swap(s); // Free memory in _onHold
#endif

	for ( int proc = 0; proc < static_cast<int>( _gotEvents.size() );
				proc++ ) {
		if ( _gotEvents[proc] != ( EXEC_MASK | ABORT_TERM_MASK ) ) {
			debug_printf( DEBUG_NORMAL,
					"Warning for node %s: unexpected _gotEvents value for proc %d: %d!\n",
					GetJobName(), proc, (int)_gotEvents[proc] );
			check_warning_strictness( DAG_STRICT_2 );
		}
	}

	std::vector<unsigned char> s2;
	_gotEvents.swap(s2); // Free memory in _gotEvents
	
#ifdef DEAD_CODE
	std::vector<unsigned char> s3;
	_isIdle.swap(s3); // Free memory in _isIdle
#endif
}
