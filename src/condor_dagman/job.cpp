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

//---------------------------------------------------------------------------
JobID_t Job::_jobID_counter = 0;  // Initialize the static data memeber

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
    "STATUS_READY    ",
    "STATUS_PRERUN   ",
    "STATUS_SUBMITTED",
	"STATUS_POSTRUN  ",
    "STATUS_DONE     ",
    "STATUS_ERROR    ",
};

//---------------------------------------------------------------------------
// NOTE: must be kept in sync with the job_type_t enum
const char* Job::_job_type_names[] = {
    "Condor",
    "Stork",
    "No-Op",
};

//---------------------------------------------------------------------------
Job::~Job() {
	delete [] _directory;
	delete [] _cmdFile;
	delete [] _dagFile;
    // NOTE: we cast this to char* because older MS compilers
    // (contrary to the ISO C++ spec) won't allow you to delete a
    // const.  This has apparently been fixed in Visual C++ .NET, but
    // as of 6/2004 we don't yet use that.  For details, see:
    // http://support.microsoft.com/support/kb/articles/Q131/3/22.asp
	delete [] _jobName;
	delete [] _logFile;
	delete varNamesFromDag;
	delete varValsFromDag;
	delete _scriptPre;
	delete _scriptPost;
}

Job::
Job( const job_type_t jobType, const char* jobName, const char *directory,
			const char* cmdFile, bool prohibitMultiJobs ) :
	_jobType( jobType )
{
	Init( jobName, directory, cmdFile, prohibitMultiJobs );
}


void Job::
Init( const char* jobName, const char* directory, const char* cmdFile,
			bool prohibitMultiJobs )
{
	ASSERT( jobName != NULL );
	ASSERT( cmdFile != NULL );

	debug_printf( DEBUG_DEBUG_1, "Job::Init(%s, %s, %s)\n", jobName,
				directory, cmdFile );

    _scriptPre = NULL;
    _scriptPost = NULL;
    _Status = STATUS_READY;
	_isIdle = false;
	countedAsDone = false;
	_waitingCount = 0;

    _jobName = strnewp (jobName);
	_directory = strnewp (directory);
    _cmdFile = strnewp (cmdFile);
	_dagFile = NULL;
	_throttleInfo = NULL;

	if ( (_jobType == TYPE_CONDOR) && prohibitMultiJobs ) {
		MyString	errorMsg;
		int queueCount = MultiLogFiles::getQueueCountFromSubmitFile(
					MyString( _cmdFile ), MyString( _directory ),
					errorMsg );
		if ( queueCount == -1 ) {
			debug_printf( DEBUG_NORMAL, "ERROR in "
						"MultiLogFiles::getQueueCountFromSubmitFile(): %s\n",
						errorMsg.Value() );
			main_shutdown_rescue( EXIT_ERROR );
		} else if ( queueCount != 1 ) {
			debug_printf( DEBUG_NORMAL, "ERROR: node %s job queues %d "
						"job procs, but DAGMAN_PROHIBIT_MULTI_JOBS is "
						"set\n", _jobName, queueCount );
			main_shutdown_rescue( EXIT_ERROR );
		}
	}

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
	_visited = false;
	_dfsOrder = -1; // so Coverity is happy

	_queuedNodeJobProcs = 0;

	_hasNodePriority = false;
	_nodePriority = 0;

		// Note: we use "" for the directory here because when this method
		// is called we should *already* be in the directory from which
		// this job is to be run.
    MyString logFile = MultiLogFiles::loadLogFileNameFromSubFile(_cmdFile, "");
		// Note: _logFile is needed only for POST script events (as of
		// 2005-06-23).
		// This will go away once the lazy log file code is fully
		// implemented.  wenger 2008-12-19.
    _logFile = strnewp (logFile.Value());

	varNamesFromDag = new List<MyString>;
	varValsFromDag = new List<MyString>;

	snprintf( error_text, JOB_ERROR_TEXT_MAXLEN, "unknown" );

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

	delete [] _directory;

	_directory = strnewp(newdir.Value());
}

//---------------------------------------------------------------------------
bool Job::Remove (const queue_t queue, const JobID_t jobID) {
    _queues[queue].Rewind();
    JobID_t currentJobID;
    while(_queues[queue].Next(currentJobID)) {
        if (currentJobID == jobID) {
            _queues[queue].DeleteCurrent();
            return true;
        }
    }
    return false;   // Element Not Found
}  

//---------------------------------------------------------------------------
bool
Job::CheckForLogFile() const
{
    MyString logFile = MultiLogFiles::loadLogFileNameFromSubFile( _cmdFile,
				_directory );
	bool result = (logFile != "");
	return result;
}

//---------------------------------------------------------------------------
void Job::Dump () const {
    dprintf( D_ALWAYS, "---------------------- Job ----------------------\n");
    dprintf( D_ALWAYS, "      Node Name: %s\n", _jobName );
    dprintf( D_ALWAYS, "         NodeID: %d\n", _jobID );
    dprintf( D_ALWAYS, "    Node Status: %s\n", GetStatusName() );
    dprintf( D_ALWAYS, "Node return val: %d\n", retval );
	if( _Status == STATUS_ERROR ) {
		dprintf( D_ALWAYS, "          Error: %s\n", error_text );
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
		dprintf( D_ALWAYS, " %7s Job ID: (%d)\n", JobTypeString(),
				 _CondorID._cluster );
	}
  
    for (int i = 0 ; i < 3 ; i++) {
        dprintf( D_ALWAYS, "%15s: ", queue_t_names[i] );
        SimpleListIterator<JobID_t> iList (_queues[i]);
        JobID_t jobID;
        while( iList.Next( jobID ) ) {
			dprintf( D_ALWAYS | D_NOHEADER, "%d, ", jobID );
		}
        dprintf( D_ALWAYS | D_NOHEADER, "<END>\n" );
    }
}

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
    if (job == NULL) dprintf( D_ALWAYS, "(UNKNOWN)");
    else job->Print(condorID);
}

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

	if( _waitingCount < 0 ) {
		dprintf( D_ALWAYS, "BADNESS 10000: _waitingCount = %d\n",
				 _waitingCount );
		result = false;
	}

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
		// TODO: add some state transition sanity-checking here?
	_Status = newStatus;
	return true;
}


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
		_waitingCount++;
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

		// we don't currently allow a new parent to be added to a
		// child that has already been started (unless the parent is
		// already marked STATUS_DONE, e.g., when rebuilding from a
		// rescue DAG) -- but this restriction might be lifted in the
		// future once we figure out the right way for the DAG to
		// respond...
	if( _Status != STATUS_READY && parent->GetStatus() != STATUS_DONE ) {
		whynot.sprintf( "%s child may not be given a new %s parent",
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
		debug_printf( DEBUG_QUIET,
					"Warning: parent %s already has child %s\n",
					GetJobName(), child->GetJobName() );
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

	whynot = "n/a";
	return true;
}


bool
Job::TerminateSuccess()
{
	_Status = STATUS_DONE;
	return true;
} 

bool
Job::TerminateFailure()
{
	_Status = STATUS_ERROR;
	return true;
} 

bool
Job::Add( const queue_t queue, const JobID_t jobID )
{
	if( _queues[queue].IsMember( jobID ) ) {
		dprintf( D_ALWAYS,
				 "ERROR: can't add Job ID %d to DAG: already present!",
				 jobID );
		return false;
	}
	return _queues[queue].Append(jobID);
}

bool
Job::AddPreScript( const char *cmd, MyString &whynot )
{
return AddScript( false, cmd, whynot );
}

bool
Job::AddPostScript( const char *cmd, MyString &whynot )
{
	return AddScript( true, cmd, whynot );
}

bool
Job::AddScript( bool post, const char *cmd, MyString &whynot )
{
	if( !cmd || strcmp( cmd, "" ) == 0 ) {
		whynot = "missing script name";
		return false;
	}
	if( post ? _scriptPost : _scriptPre ) {
		whynot.sprintf( "%s script already assigned (%s)",
						post ? "POST" : "PRE", GetPreScriptName() );
		return false;
	}
	Script* script = new Script( post, cmd, this );
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
Job::IsActive() const
{
	if( _Status == STATUS_PRERUN ||
		_Status == STATUS_SUBMITTED ||
		_Status == STATUS_POSTRUN ) {
		return true;
	}
	return false;
}

const char*
Job::GetStatusName() const
{
		// Put in bounds check here?
	return status_t_names[_Status];
}


bool
Job::HasChild( Job* child ) {
	if( !child ) {
		return false;
	}
	return _queues[Q_CHILDREN].IsMember( child->GetJobID() );
}

bool
Job::HasParent( Job* parent ) {
	if( !parent ) {
		return false;
	}
	return _queues[Q_PARENTS].IsMember( parent->GetJobID() );
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
	JobID_t candidate;
    _queues[queue].Rewind();
    while( _queues[queue].Next( candidate ) ) {
        if( candidate == job ) {
            _queues[queue].DeleteCurrent();
			if ( _queues[queue].IsMember( job ) ) {
				EXCEPT( "Job %d still in queue %d after deletion!!",
							job, queue );
			}
			whynot = "n/a";
			return true;
        }
    }
	whynot = "no such dependency";
	return false;
}


Job::job_type_t
Job::JobType() const
{
    return _jobType;
}


const char*
Job::JobTypeString() const
{
    return _job_type_names[_jobType];
}


/*
const char* Job::JobIdString() const
{

}
*/


int
Job::NumParents() const
{
	int n = _queues[Q_PARENTS].Number();
	return n;
}

int
Job::NumChildren() const
{
	int n = _queues[Q_CHILDREN].Number();
	return n;
}

void
Job::SetCategory( const char *categoryName, ThrottleByCategory &catThrottles )
{
	MyString	tmpName( categoryName );

	if ( (_throttleInfo != NULL) &&
				(tmpName != *(_throttleInfo->_category)) ) {
		debug_printf( DEBUG_NORMAL, "Warning: new category %s for node %s "
					"overrides old value %s\n", categoryName, GetJobName(),
					_throttleInfo->_category->Value() );
	}

	ThrottleByCategory::ThrottleInfo *throttleInfo =
				catThrottles.GetThrottleInfo( &tmpName );
	if ( throttleInfo != NULL ) {
		_throttleInfo = throttleInfo;
	} else {
		_throttleInfo = catThrottles.AddCategory( &tmpName );
	}
}

void
Job::PrefixName(const MyString &prefix)
{
	MyString tmp = _jobName;

	tmp = prefix + tmp;

	delete[] _jobName;

	_jobName = strnewp(tmp.Value());
}


// iterate across the Job's var values, and for any which have $(JOB) in them, 
// substitute it. This substitution is draconian and will always happen.
void
Job::ResolveVarsInterpolations(void)
{
	MyString *val;

	varValsFromDag->Rewind();
	while( (val = varValsFromDag->Next()) != NULL ) {
		// XXX No way to escape $(JOB) in case, for some crazy reason, you
		// want a filename component actually to be '$(JOB)'.
		// It isn't hard to fix, I'll do it later.
		val->replaceString("$(JOB)", GetJobName());
	}
}

//---------------------------------------------------------------------------
void
Job::SetDagFile(const char *dagFile)
{
	delete _dagFile;
	_dagFile = strnewp( dagFile );
}

#if LAZY_LOG_FILES
//---------------------------------------------------------------------------
bool
Job::MonitorLogFile( ReadMultipleUserLogs &logReader, bool recovery )
{
	bool result = false;

    MyString logFileStr = MultiLogFiles::loadLogFileNameFromSubFile(
				_cmdFile, _directory );
	if ( logFileStr == "" ) {
		debug_printf( DEBUG_QUIET, "ERROR: Unable to get log file from "
					"submit file %s (node %s)\n", _cmdFile, GetJobName() );
		result = false;
	} else {
		delete [] _logFile; // temporary
		_logFile = strnewp( logFileStr.Value() );
		CondorError errstack;
		result = logReader.monitorLogFile( _logFile, !recovery, errstack );
		if ( !result ) {
			errstack.pushf( "DAGMan::Job", 0/*TEMP*/, "ERROR: Unable to "
						"monitor log file for node %s", GetJobName() );
			debug_printf( DEBUG_QUIET, "%s\n", errstack.getFullText() );
		}
	}

	return result;
}

//---------------------------------------------------------------------------
bool
Job::UnmonitorLogFile( ReadMultipleUserLogs &logReader )
{
	CondorError errstack;
	bool result = logReader.unmonitorLogFile( _logFile, errstack );
	if ( !result ) {
		errstack.pushf( "DAGMan::Job", 0/*TEMP*/, "ERROR: Unable to "
					"unmonitor log " "file for node %s", GetJobName() );
		debug_printf( DEBUG_QUIET, "%s\n", errstack.getFullText() );
	}

#if 0 // uncomment once lazy log file code is fully implemented
	delete [] _logFile;
	_logFile = NULL;
#endif

	return result;
}

#endif // LAZY_LOG_FILES
