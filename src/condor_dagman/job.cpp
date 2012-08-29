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

	varNamesFromDag->Rewind();
	MyString *name;
	while ( (name = varNamesFromDag->Next()) ) {
		delete name;
	}
	delete varNamesFromDag;

	varValsFromDag->Rewind();
	MyString *val;
	while ( (val = varValsFromDag->Next()) ) {
		delete val;
	}
	delete varValsFromDag;

	delete _scriptPre;
	delete _scriptPost;

	delete [] _jobTag;
}

//---------------------------------------------------------------------------
Job::Job( const job_type_t jobType, const char* jobName,
			const char *directory, const char* cmdFile ) :
	_jobType( jobType ), _preskip( PRE_SKIP_INVALID ),
			_pre_status( NO_PRE_VALUE ), _final( false ), append_default_log(true)
{
	ASSERT( jobName != NULL );
	ASSERT( cmdFile != NULL );

	debug_printf( DEBUG_DEBUG_1, "Job::Job(%s, %s, %s)\n", jobName,
			directory, cmdFile );

	_scriptPre = NULL;
	_scriptPost = NULL;
	_Status = STATUS_READY;
	_isIdle = false;
	countedAsDone = false;

	_jobName = strnewp (jobName);
	_directory = strnewp (directory);
	_cmdFile = strnewp (cmdFile);
	_dagFile = NULL;
	_throttleInfo = NULL;
	_logIsMonitored = false;
	_useDefaultLog = false;

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

	_logFile = NULL;
	_logFileIsXml = false;

	_noop = false;

	_jobTag = NULL;
	_jobstateSeqNum = 0;
	_lastEventTime = 0;

	varNamesFromDag = new List<MyString>;
	varValsFromDag = new List<MyString>;

	snprintf( error_text, JOB_ERROR_TEXT_MAXLEN, "unknown" );

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

	delete [] _directory;

	_directory = strnewp(newdir.Value());
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
bool
Job::CheckForLogFile(bool usingDefault ) const
{
	bool tmpLogFileIsXml;
	MyString logFile = MultiLogFiles::loadLogFileNameFromSubFile( _cmdFile,
				_directory, tmpLogFileIsXml, usingDefault );
	return (logFile != "");
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
	 debug_printf( DEBUG_DEBUG_1, "Job(%s)::SetStatus(%s)\n",
	 			GetJobName(), status_t_names[newStatus] );

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
		whynot.formatstr( "%s script already assigned (%s)",
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
Job::AddPreSkip( int exitCode, MyString &whynot )
{
	if( exitCode < PRE_SKIP_MIN || exitCode > PRE_SKIP_MAX ) {
		whynot.formatstr( "PRE_SKIP exit code must be between %d and %d\n",
			PRE_SKIP_MIN, PRE_SKIP_MAX );
		return false;
	}

	if( exitCode == 0 ) {
		debug_printf( DEBUG_NORMAL, "Warning: exit code 0 for a PRE_SKIP "
			"value is weird.\n");
	}

	if( _preskip == PRE_SKIP_INVALID ) {
		_preskip = exitCode;	
	} else {
		whynot = "Two definitions of PRE_SKIP for a node.\n";
		return false;
	}
	whynot = "n/a";
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

//---------------------------------------------------------------------------
bool
Job::MonitorLogFile( ReadMultipleUserLogs &condorLogReader,
			ReadMultipleUserLogs &storkLogReader, bool nfsIsError,
			bool recovery, const char *defaultNodeLog, bool usingDefault )
{
	debug_printf( DEBUG_DEBUG_2,
				"Attempting to monitor log file for node %s\n",
				GetJobName() );

	if ( _logIsMonitored ) {
		debug_printf( DEBUG_DEBUG_1, "Warning: log file for node "
					"%s is already monitored\n", GetJobName() );
		return true;
	}

	ReadMultipleUserLogs &logReader = (_jobType == TYPE_CONDOR) ?
				condorLogReader : storkLogReader;

    std::string logFileStr;
	if ( _jobType == TYPE_CONDOR ) {
			// We check to see if the user has specified a log file
			// If not, we give him a default
    	MyString templogFileStr = MultiLogFiles::loadLogFileNameFromSubFile( _cmdFile,
					_directory, _logFileIsXml, usingDefault);
		logFileStr = templogFileStr.Value();
	} else {
		StringList logFiles;
		MyString tmpResult = MultiLogFiles::loadLogFileNamesFromStorkSubFile(
					_cmdFile, _directory, logFiles );
		if ( tmpResult != "" ) {
			debug_printf( DEBUG_QUIET, "Error getting Stork log file: %s\n",
						tmpResult.Value() );
			LogMonitorFailed();
			return false;
		} else if ( logFiles.number() != 1 ) {
			debug_printf( DEBUG_QUIET, "Error: %d Stork log files found "
						"in submit file %s; we want 1\n",
						logFiles.number(), _cmdFile );
			LogMonitorFailed();
			return false;
		} else {
			logFiles.rewind();
			logFileStr = logFiles.next();
		}
	}

	if ( logFileStr == "" ) {
		logFileStr = defaultNodeLog;
		_useDefaultLog = true;
			// Default User log is never XML
			// This could be specified in the submit file and should be
			// ignored.
		_logFileIsXml = false;
		debug_printf( DEBUG_NORMAL, "Unable to get log file from "
					"submit file %s (node %s); using default (%s)\n",
					_cmdFile, GetJobName(), logFileStr.c_str() );
		append_default_log = false;
	} else {
		append_default_log = usingDefault;
		if( append_default_log ) {
				// DAGman is not going to look at the user-specified log.
				// It will look at the defaultNode log.
			logFileStr = defaultNodeLog;
			_useDefaultLog = false;
			_logFileIsXml = false;
		}
	}

		// This function returns true if the log file is on NFS and
		// that is an error.  If the log file is on NFS, but nfsIsError
		// is false, it prints a warning but returns false.
	if ( MultiLogFiles::logFileNFSError( logFileStr.c_str(),
				nfsIsError ) ) {
		debug_printf( DEBUG_QUIET, "Error: log file %s on NFS\n",
					logFileStr.c_str() );
		LogMonitorFailed();
		return false;
	}

	delete [] _logFile;
		// Saving log file here in case submit file gets changed.
	_logFile = strnewp( logFileStr.c_str() );
	debug_printf( DEBUG_DEBUG_2, "Monitoring log file <%s> for node %s\n",
				GetLogFile(), GetJobName() );
	CondorError errstack;
	if ( !logReader.monitorLogFile( GetLogFile(), !recovery, errstack ) ) {
		errstack.pushf( "DAGMan::Job", DAGMAN_ERR_LOG_FILE,
					"ERROR: Unable to monitor log file for node %s",
					GetJobName() );
		debug_printf( DEBUG_QUIET, "%s\n", errstack.getFullText().c_str() );
		LogMonitorFailed();
		EXCEPT( "Fatal log file monitoring error!\n" );
		return false;
	}

	_logIsMonitored = true;

	return true;
}

//---------------------------------------------------------------------------
bool
Job::UnmonitorLogFile( ReadMultipleUserLogs &condorLogReader,
			ReadMultipleUserLogs &storkLogReader )
{
	debug_printf( DEBUG_DEBUG_2, "Unmonitoring log file <%s> for node %s\n",
				GetLogFile(), GetJobName() );

	if ( !_logIsMonitored ) {
		debug_printf( DEBUG_DEBUG_1, "Warning: log file for node "
					"%s is already unmonitored\n", GetJobName() );
		return true;
	}

	ReadMultipleUserLogs &logReader = (_jobType == TYPE_CONDOR) ?
				condorLogReader : storkLogReader;

	debug_printf( DEBUG_DEBUG_1, "Unmonitoring log file <%s> for node %s\n",
				GetLogFile(), GetJobName() );

	CondorError errstack;
	bool result = logReader.unmonitorLogFile( GetLogFile(), errstack );
	if ( !result ) {
		errstack.pushf( "DAGMan::Job", DAGMAN_ERR_LOG_FILE,
					"ERROR: Unable to unmonitor log " "file for node %s",
					GetJobName() );
		debug_printf( DEBUG_QUIET, "%s\n", errstack.getFullText().c_str() );
		EXCEPT( "Fatal log file monitoring error!\n" );
	}

	if ( result ) {
		delete [] _logFile;
		_logFile = NULL;
		_logIsMonitored = false;
	}

	return result;
}

//---------------------------------------------------------------------------
void
Job::LogMonitorFailed()
{
	if ( _Status != Job::STATUS_ERROR ) {
		_Status = Job::STATUS_ERROR;
		snprintf( error_text, JOB_ERROR_TEXT_MAXLEN,
					"Unable to monitor node job log file" );
		retval = Dag::DAG_ERROR_LOG_MONITOR_ERROR;
		if ( _scriptPost != NULL) {
				// let the script know the job's exit status
			_scriptPost->_retValJob = retval;
		}
	}
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
			jobTagName = jobTagName.Substr( begin, end );
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
			tmpJobTag = tmpJobTag.Substr( begin, end );
		}
		_jobTag = strnewp( tmpJobTag.Value() );
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
	struct tm eventTm = event->eventTime;
	_lastEventTime = mktime( &eventTm );
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
// If there is a cycle, could this enter an infinite loop?
// No: If there is a cycle, there will be equality, and recursion will stop
// It makes no sense to insert job priorities on linear DAGs;

// The scheme here is to copy the priority from parent nodes, if a parent node
// has priority higher than the job priority currently assigned to the node, or
// use the default priority of the DAG; otherwise, we use the priority from the
// DAG file. Priorities calculated by DAGman will ignore and override job
// priorities set in the submit file.

// DAGman fixes the default priorities in Dag::SetDefaultPriorities

void
Job::FixPriority(Dag& dag)
{
	std::set<JobID_t> parents = GetQueueRef(Q_PARENTS);
	for(std::set<JobID_t>::iterator p = parents.begin(); p != parents.end(); ++p){
		Job* parent = dag.FindNodeByNodeID(*p);
		if( parent->_hasNodePriority ) {
			// Nothing to do if parent priority is small
			if( parent->_nodePriority > _nodePriority ) {
				_nodePriority = parent->_nodePriority;
				_hasNodePriority = true;
			}
		}
	}
}
