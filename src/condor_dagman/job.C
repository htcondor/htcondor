/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "job.h"
#include "condor_string.h"
#include "condor_debug.h"
#include "read_multiple_logs.h"

//---------------------------------------------------------------------------
JobID_t Job::_jobID_counter = 0;  // Initialize the static data memeber

//---------------------------------------------------------------------------
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
Job::~Job() {
	delete [] _cmdFile;
    // NOTE: we cast this to char* because older MS compilers
    // (contrary to the ISO C++ spec) won't allow you to delete a
    // const.  This has apparently been fixed in Visual C++ .NET, but
    // as of 6/2004 we don't yet use that.  For details, see:
    // http://support.microsoft.com/support/kb/articles/Q131/3/22.asp
	delete [] (char*)_jobName;
	delete [] _logFile;
	delete varNamesFromDag;
	delete varValsFromDag;
}

//---------------------------------------------------------------------------
Job::Job (const char *jobName, const char *cmdFile):
	job_type (CONDOR_JOB),
    _scriptPre  (NULL),
    _scriptPost (NULL),
    _Status     (STATUS_READY),
	countedAsDone (false),
	_waitingCount (0)
{
	ASSERT( jobName != NULL );
	ASSERT( cmdFile != NULL );
    _jobName = strnewp (jobName);
    _cmdFile = strnewp (cmdFile);

    // _condorID struct initializes itself

    // jobID is a primary key (a database term).  All should be unique
    _jobID = _jobID_counter++;

    retry_max = 0;
    retries = 0;
	_visited = false;

    MyString logFile = ReadMultipleUserLogs::loadLogFileNameFromSubFile(_cmdFile);
    _logFile = strnewp (logFile.Value());

	varNamesFromDag = new List<MyString>;
	varValsFromDag = new List<MyString>;
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
void Job::Dump () const {
    dprintf( D_ALWAYS, "---------------------- Job ----------------------\n");
    dprintf( D_ALWAYS, "      Node Name: %s\n", _jobName );
    dprintf( D_ALWAYS, "         NodeID: %d\n", _jobID );
    dprintf( D_ALWAYS, "    Node Status: %s\n", status_t_names[_Status] );
	if( _Status == STATUS_ERROR ) {
		dprintf( D_ALWAYS, "          Error: %s\n",
				 error_text ? error_text : "unknown" );
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
	//-->DAP
	if( _CondorID._cluster == -1 ) {
	  if (job_type == CONDOR_JOB){
	    dprintf( D_ALWAYS, "  Condor Job ID: [not yet submitted]\n" );
	  }
	  else if (job_type == DAP_JOB){
	    dprintf( D_ALWAYS, "     DaP Job ID: [not yet submitted]\n" );
	  }
	}
	else {
	  if (job_type == CONDOR_JOB){
	  dprintf( D_ALWAYS, "  Condor Job ID: (%d.%d.%d)\n", _CondorID._cluster,
		   _CondorID._proc, _CondorID._subproc );
	  }
	  else if (job_type == DAP_JOB){
	    dprintf( D_ALWAYS, "     DaP Job ID: (%d)\n", _CondorID._cluster);
	  }
	}
	//<--DAP



  
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
Job::AddParent( Job* parent )
{
	ASSERT( parent != NULL );

		// we don't currently allow a new parent to be added to a
		// child that has already been started (unless the parent is
		// already marked STATUS_DONE, e.g., when rebuilding from a
		// rescue DAG) -- but this restriction might be lifted in the
		// future once we figure out the right way for the DAG to
		// respond...
	if( _Status != STATUS_READY && parent->GetStatus() != STATUS_DONE ) {
		debug_printf( DEBUG_QUIET, "ERROR: Job::AddParent(): can't add a new "
					  "%s parent node (%s) to a %s child (%s)!\n",
					  parent->GetStatusName(), parent->GetJobName(),
					  this->GetStatusName(), this->GetJobName() );
		return false;
	}

	if( !Add( Q_PARENTS, parent->GetJobID() ) ) {
		return false;
	}
    if( !Add( Q_WAITING, parent->GetJobID() ) ) {
		return false;
	}
	_waitingCount++;
    return true;
}

bool
Job::AddChild( Job* child )
{
    ASSERT( child  != NULL );
    if( !Add( Q_CHILDREN, child->GetJobID() ) ) {
		return false;
	}
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
	Script* script = new Script( post, cmd, _jobName );
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
			ASSERT( _queues[queue].IsMember( job ) == false );
			whynot = "n/a";
			return true;
        }
    }
	whynot = "no such dependency";
	return false;
}

const int
Job::NumParents()
{
	int n = _queues[Q_PARENTS].Number();
	return n;
}
