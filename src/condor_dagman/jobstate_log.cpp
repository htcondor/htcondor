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
#include "condor_string.h"  /* for strnewp() */
#include "jobstate_log.h"
#include "debug.h"
//TEMPTEMP? #include "MyString.h"

const char *JobstateLog::SUBMIT_NAME = "SUBMIT";
const char *JobstateLog::EXECUTE_NAME = "EXECUTE";
const char *JobstateLog::GLOBUS_SUBMIT_NAME = "GLOBUS_SUBMIT";
const char *JobstateLog::GRID_SUBMIT_NAME = "GRID_SUBMIT";
const char *JobstateLog::JOB_TERMINATED_NAME = "JOB_TERMINATED";
const char *JobstateLog::JOB_SUCCESS_NAME = "JOB_SUCCESS";
const char *JobstateLog::JOB_FAILURE_NAME = "JOB_FAILURE";
const char *JobstateLog::POST_SCRIPT_TERMINATED_NAME =
			"POST_SCRIPT_TERMINATED";
const char *JobstateLog::POST_SCRIPT_STARTED_NAME = "POST_SCRIPT_STARTED";
const char *JobstateLog::POST_SCRIPT_SUCCESS_NAME = "POST_SCRIPT_SUCCESS";
const char *JobstateLog::POST_SCRIPT_FAILURE_NAME = "POST_SCRIPT_FAILURE";

//---------------------------------------------------------------------------
JobstateLog::JobstateLog( const char *jobstateLogFile )
{
	ASSERT( jobstateLogFile );

	_jobstateLogFile = strnewp( jobstateLogFile );
}

//---------------------------------------------------------------------------
JobstateLog::~JobstateLog()
{
	delete [] _jobstateLogFile;
}

//---------------------------------------------------------------------------
void
JobstateLog::WriteEvent( const ULogEvent *event, Job *node )
{
	ASSERT( node );
        //TEMPTEMP -- need to generate POST_SCRIPT_STARTED and POST_SCRIPT_SUCCESS "events" in the jobstate.log file...
		//TEMPTEMP -- print to jobstate log here??
		//TEMPTEMP -- shit -- I need to remove the "ULOG" from the beginning of the event type...
		//TEMPTEMP -- oh, yeah -- we also have to filter out events pegasus doesn't care about...

	const char *eventName = NULL;
	switch ( event->eventNumber ) {
	case ULOG_SUBMIT:
		eventName = SUBMIT_NAME;
		break;

	case ULOG_EXECUTE:
		eventName = EXECUTE_NAME;
		break;

	case ULOG_GLOBUS_SUBMIT:
		eventName = GLOBUS_SUBMIT_NAME;
		break;

	case ULOG_GRID_SUBMIT:
		eventName = GRID_SUBMIT_NAME;
		break;

	case ULOG_JOB_TERMINATED:
		eventName = JOB_TERMINATED_NAME;
		break;

	case ULOG_POST_SCRIPT_TERMINATED:
		eventName = POST_SCRIPT_TERMINATED_NAME;
		break;

	default:
		// All other events we don't want to log...
		break;
	}

	if ( eventName != NULL ) {
		MyString condorID;
		condorID.sprintf( "%d.%d", event->cluster, event->proc );
		Write( node, eventName, condorID.Value() );
	}
}

//---------------------------------------------------------------------------
void
JobstateLog::WriteJobSuccessOrFailure( Job *node )
{
	ASSERT( node );

	const char *eventName = node->retval == 0 ?
				JOB_SUCCESS_NAME : JOB_FAILURE_NAME;
	MyString retval;
	retval.sprintf( "%d", node->retval );

	Write( node, eventName, retval.Value() );
}

//---------------------------------------------------------------------------
void
JobstateLog::Write( Job *node, const char *eventName,
			const char *condorID )
{
	FILE *outfile = safe_fopen_wrapper( _jobstateLogFile, "a" );
	if ( !outfile ) {
       	debug_printf( DEBUG_QUIET, "Could not open %s for writing.\n",
					_jobstateLogFile );
	} else {
		time_t eventTime = time( NULL );
		fprintf( outfile, "%lu %s %s %s %s -\n",
					(unsigned long)eventTime, node->GetJobName(),
					eventName, condorID, node->PegasusSite() );
		fclose( outfile );
	}
}
