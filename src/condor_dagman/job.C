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

//---------------------------------------------------------------------------
JobID_t Job::_jobID_counter = 0;  // Initialize the static data memeber

//---------------------------------------------------------------------------
const char *Job::queue_t_names[] = {
    "Q_PARENTS",
    "Q_WAITING",
    "Q_CHILDREN",
};

//---------------------------------------------------------------------------
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
    _Status     (STATUS_READY)
{
	assert( jobName != NULL );
	assert( cmdFile != NULL );
    _jobName = strnewp (jobName);
    _cmdFile = strnewp (cmdFile);

    // _condorID struct initializes itself

    // jobID is a primary key (a database term).  All should be unique
    _jobID = _jobID_counter++;
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
    printf ("Job -------------------------------------\n");
    printf ("          JobID: %d\n", _jobID);
    printf ("       Job Name: %s\n", _jobName);
    printf ("     Job Status: %s\n", status_t_names[_Status]);
	if( _Status == STATUS_ERROR ) {
		printf( "          Error: %s\n", error_text ? error_text : "unknown" );
	}
    printf ("       Cmd File: %s\n", _cmdFile);
	if( _scriptPre ) {
		printf( "     PRE Script: %s\n", _scriptPre->GetCmd() );
	}
	if( _scriptPost ) {
		printf( "    POST Script: %s\n", _scriptPost->GetCmd() );
	}
    printf ("      Condor ID: ");
    _CondorID.Print();
    putchar('\n');
  
    for (int i = 0 ; i < 3 ; i++) {
        printf ("%15s: ", queue_t_names[i]);
        SimpleListIterator<JobID_t> iList (_queues[i]);
        JobID_t jobID;
        while (iList.Next(jobID)) printf ("%d, ", jobID);
        printf ("<END>\n");
    }
}

//---------------------------------------------------------------------------
void Job::Print (bool condorID) const {
    printf ("ID: %4d Name: %s", _jobID, _jobName);
    if (condorID) {
        printf ("  CondorID: ");
        _CondorID.Print();
    }
}

//---------------------------------------------------------------------------
void job_print (Job * job, bool condorID) {
    if (job == NULL) printf ("(UNKNOWN)");
    else job->Print(condorID);
}
