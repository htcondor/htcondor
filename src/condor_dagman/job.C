/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2001 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
 ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "job.h"
#include "condor_string.h"
#include "condor_debug.h"

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
    delete [] _jobName;
}

//---------------------------------------------------------------------------
Job::Job (const char *jobName, const char *cmdFile):
    _scriptPre  (NULL),
    _scriptPost (NULL),
    _Status     (STATUS_READY),
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
		// child that has already been started -- but this restriction
		// might be lifted in the future once we figure out the right
		// way for the DAG to respond...
	assert( _Status == Job::STATUS_READY );

	if( !Add( Job::Q_PARENTS, parent->GetJobID() ) ) {
		return false;
	}
    if( !Add( Job::Q_WAITING, parent->GetJobID() ) ) {
		return false;
	}
	_waitingCount++;
    return true;
}

bool
Job::AddChild( Job* child )
{
    ASSERT( child  != NULL );
    if( !Add( Job::Q_CHILDREN, child->GetJobID() ) ) {
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
	return status_t_names[_Status];
}
