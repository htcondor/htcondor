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

static const char *JOB_TAG_NAME = "+job_tag_name";
static const char *PEGASUS_SITE = "+pegasus_site";

//---------------------------------------------------------------------------
JobID_t Job::_jobID_counter = 0;  // Initialize the static data memeber
int Job::NOOP_NODE_PROCID = INT_MAX;
int Job::_nextJobstateSeqNum = 1;

//---------------------------------------------------------------------------
// NOTE: this must be kept in sync with the queue_t enum
const char *Job::queue_t_names[] = {
    "Q_PARENTS",
    "Q_WAITING",
    "Q_CHILDREN",
};

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
	free(_directory);
	free(_cmdFile);
	free(_dagFile);
	free(_jobName);

	varsFromDag->Rewind();
	NodeVar *var;
	while ( (var = varsFromDag->Next()) ) {
		delete var;
	}
	delete varsFromDag;

	delete _scriptPre;
	delete _scriptPost;

	free(_jobTag);
}

//---------------------------------------------------------------------------
Job::Job( const char* jobName,
			const char *directory, const char* cmdFile ) :
	_preskip( PRE_SKIP_INVALID ), _final( false )
{
	ASSERT( jobName != NULL );
	ASSERT( cmdFile != NULL );

	debug_printf( DEBUG_DEBUG_1, "Job::Job(%s, %s, %s)\n", jobName,
			directory, cmdFile );

	_scriptPre = NULL;
	_scriptPost = NULL;
	_Status = STATUS_READY;
	countedAsDone = false;

	_jobName = strdup (jobName);
	_directory = strdup (directory);
	_cmdFile = strdup (cmdFile);
	_dagFile = NULL;
	_throttleInfo = NULL;

    // _condorID struct initializes itself

		// jobID is a primary key (a database term).  All should be unique
	_jobID = _jobID_counter++;

	retry_max = 0;
	retries = 0;
	_submitTries = 0;
	retval = -1; // so Coverity is happy
	have_retry_abort_val = false;
	retry_abort_val = 0xdeadbeef;
	have_abort_dag_val = false;
	abort_dag_val = -1; // so Coverity is happy
	have_abort_dag_return_val = false;
	abort_dag_return_val = -1; // so Coverity is happy
	is_factory = false;
	_visited = false;
	_dfsOrder = -1; // so Coverity is happy

	_queuedNodeJobProcs = 0;
	_numSubmittedProcs = 0;

	_explicitPriority = 0;
	_effectivePriority = _explicitPriority;

	_noop = false;

	_jobTag = NULL;
	_jobstateSeqNum = 0;
	_lastEventTime = 0;

	varsFromDag = new List<NodeVar>;

	error_text = "";

	_timesHeld = 0;
	_jobProcsOnHold = 0;

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

	free(_directory);

	_directory = strdup(newdir.Value());
}

//---------------------------------------------------------------------------
bool Job::Remove (const queue_t queue, const JobID_t jobID)
{
	if (_queues[queue].erase(jobID) == 0) {
		return false; // element not found
	}

	return true;
}  

//---------------------------------------------------------------------------
void Job::Dump ( const Dag *dag ) const {
    dprintf( D_ALWAYS, "---------------------- Job ----------------------\n");
    dprintf( D_ALWAYS, "      Node Name: %s\n", _jobName );
    dprintf( D_ALWAYS, "           Noop: %s\n", _noop ? "true" : "false" );
    dprintf( D_ALWAYS, "         NodeID: %d\n", _jobID );
    dprintf( D_ALWAYS, "    Node Status: %s\n", GetStatusName() );
    dprintf( D_ALWAYS, "Node return val: %d\n", retval );
	if( _Status == STATUS_ERROR ) {
		dprintf( D_ALWAYS, "          Error: %s\n", error_text.Value() );
	}
    dprintf( D_ALWAYS, "Job Submit File: %s\n", _cmdFile );
	if( _scriptPre ) {
		dprintf( D_ALWAYS, "     PRE Script: %s\n", _scriptPre->GetCmd() );
	}
	if( _scriptPost ) {
		dprintf( D_ALWAYS, "    POST Script: %s\n", _scriptPost->GetCmd() );
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
  
    for (int i = 0 ; i < 3 ; i++) {
        dprintf( D_ALWAYS, "%15s: ", queue_t_names[i] );

		std::set<JobID_t>::const_iterator qit;
		for (qit = _queues[i].begin(); qit != _queues[i].end(); qit++) {
			Job *node = dag->Dag::FindNodeByNodeID( *qit );
			dprintf( D_ALWAYS | D_NOHEADER, "%s, ", node->GetJobName() );
		}
        dprintf( D_ALWAYS | D_NOHEADER, "<END>\n" );
    }
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

Job::status_t
Job::GetStatus() const
{
	return _Status;
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

	if ( proc >= static_cast<int>( _isIdle.size() ) ) {
		_isIdle.resize( proc+1, false );
	}
	return _isIdle[proc];
}

//---------------------------------------------------------------------------
void
Job::SetProcIsIdle( int proc, bool isIdle )
{
	if ( GetNoop() ) {
		proc = 0;
	}

	if ( proc >= static_cast<int>( _isIdle.size() ) ) {
		_isIdle.resize( proc+1, false );
	}
	_isIdle[proc] = isIdle;
}

//---------------------------------------------------------------------------
void
Job::PrintProcIsIdle()
{
	for ( int proc = 0;
				proc < static_cast<int>( _isIdle.size() ); ++proc ) {
		debug_printf( DEBUG_QUIET, "  Job(%s)::_isIdle[%d]: %d\n",
					GetJobName(), proc, _isIdle[proc] );
	}
}

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


bool
Job::CanAddParent( Job* parent, MyString &whynot )
{
	if( !parent ) {
		whynot = "parent == NULL";
		return false;
	}
	if(GetFinal()) {
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


bool
Job::CanAddChild( Job* child, MyString &whynot )
{
	if( !child ) {
		whynot = "child == NULL";
		return false;
	}
	if(GetFinal()) {
		whynot = "Tried to add a child to a final node";
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

bool
Job::AddScript( bool post, const char *cmd, int defer_status, time_t defer_time, MyString &whynot )
{
	if( !cmd || strcmp( cmd, "" ) == 0 ) {
		whynot = "missing script name";
		return false;
	}
	if( post ? _scriptPost : _scriptPre ) {
		const char *prePost = post ? "POST" : "PRE";
		const char *script = post ? GetPostScriptName() : GetPreScriptName();
		debug_printf( DEBUG_NORMAL,
					"Warning: node %s already has %s script <%s> assigned; changing to <%s>\n",
					GetJobName(), prePost, script, cmd );
		check_warning_strictness( DAG_STRICT_3 );
		delete (post ? _scriptPost : _scriptPre);
	}
	Script* script = new Script( post, cmd, defer_status, defer_time, this );
	if( !script ) {
		dprintf( D_ALWAYS, "ERROR: out of memory!\n" );
			// we already know we're out of memory, so filling in
			// whynot will likely fail, but give it a shot...
		whynot = "out of memory!";
		return false;
	}
	if( post ) {
		_scriptPost = script;
	}
	else {
		_scriptPre = script;
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

void
Job::SetCategory( const char *categoryName, ThrottleByCategory &catThrottles )
{
	ASSERT( !_final );

	MyString	tmpName( categoryName );

	if ( (_throttleInfo != NULL) &&
				(tmpName != *(_throttleInfo->_category)) ) {
		debug_printf( DEBUG_NORMAL, "Warning: new category %s for node %s "
					"overrides old value %s\n", categoryName, GetJobName(),
					_throttleInfo->_category->Value() );
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

	_jobName = strdup(tmp.Value());
}


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

//---------------------------------------------------------------------------
void
Job::SetDagFile(const char *dagFile)
{
	free(_dagFile);
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
			int last = jobTagName.Length() - 1;
			int end = jobTagName[last] == '\"' ? last - 1 : last;
			jobTagName = jobTagName.substr( begin, 1 + end - begin );
		}

		MyString tmpJobTag = MultiLogFiles::loadValueFromSubFile(
					_cmdFile, _directory, jobTagName.Value() );
		if ( tmpJobTag == "" ) {
			tmpJobTag = "-";
		} else {
				// Remove double-quotes
			int begin = tmpJobTag[0] == '\"' ? 1 : 0;
			int last = tmpJobTag.Length() - 1;
			int end = tmpJobTag[last] == '\"' ? last - 1 : last;
			tmpJobTag = tmpJobTag.substr( begin, 1 + end - begin );
		}
		_jobTag = strdup( tmpJobTag.Value() );
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
	if( proc >= static_cast<int>( _onHold.size() ) ) {
		_onHold.resize( proc+1, 0 );
	}
	if( !_onHold[proc] ) {
		_onHold[proc] = 1;
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
	if( proc >= static_cast<int>( _onHold.size() ) ) {
		dprintf( D_FULLDEBUG, "Received release event for node %s, but job %d.%d "
			"is not on hold\n", GetJobName(), GetCluster(), GetProc() );
		return false; // We never marked this as being on hold
	}
	if( _onHold[proc] ) {
		_onHold[proc] = 0;
		--_jobProcsOnHold;
		return true;
	}
	return false;
}

//---------------------------------------------------------------------------
void
Job::ExecMetrics( int proc, const struct tm &eventTime,
			DagmanMetrics *metrics )
{
	if ( proc >= static_cast<int>( _gotEvents.size() ) ) {
		_gotEvents.resize( proc+1, 0 );
	}

	if ( _gotEvents[proc] & ABORT_TERM_MASK ) {
		debug_printf( DEBUG_NORMAL,
					"Warning for node %s: got execute event for proc %d, but already have terminated or aborted event!\n",
					GetJobName(), proc );
		check_warning_strictness( DAG_STRICT_2 );
	}

	if ( !( _gotEvents[proc] & EXEC_MASK ) ) {
		_gotEvents[proc] |= EXEC_MASK;
		metrics->ProcStarted( eventTime );
	}
}

//---------------------------------------------------------------------------
void
Job::TermAbortMetrics( int proc, const struct tm &eventTime,
			DagmanMetrics *metrics )
{
	if ( proc >= static_cast<int>( _gotEvents.size() ) ) {
		debug_printf( DEBUG_NORMAL,
					"Warning for node %s: got terminated or aborted event for proc %d, but no execute event!\n",
					GetJobName(), proc );
		check_warning_strictness( DAG_STRICT_2 );

		_gotEvents.resize( proc+1, 0 );
	}

	if ( !( _gotEvents[proc] & ABORT_TERM_MASK ) ) {
		_gotEvents[proc] |= ABORT_TERM_MASK;
		metrics->ProcFinished( eventTime );
	}
}

//---------------------------------------------------------------------------
// Note:  For multi-proc jobs, if one proc failed this was getting called
// immediately, which was not correct.  I changed how this was called, but
// we should probably put in code to make sure it's not called too soon,
// but is called...  wenger 2015-11-05
void
Job::Cleanup()
{
	std::vector<unsigned char> s;
	_onHold.swap(s); // Free memory in _onHold

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
	
	std::vector<unsigned char> s3;
	_isIdle.swap(s3); // Free memory in _isIdle
}
