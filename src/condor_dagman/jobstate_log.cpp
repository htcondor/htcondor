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
#include "MyString.h"
#include "debug.h"
#include "jobstate_log.h"
#include "dagman_main.h"

const char *JobstateLog::JOB_SUCCESS_NAME = "JOB_SUCCESS";
const char *JobstateLog::JOB_FAILURE_NAME = "JOB_FAILURE";
const char *JobstateLog::PRE_SCRIPT_STARTED_NAME = "PRE_SCRIPT_STARTED";
const char *JobstateLog::PRE_SCRIPT_SUCCESS_NAME = "PRE_SCRIPT_SUCCESS";
const char *JobstateLog::PRE_SCRIPT_FAILURE_NAME = "PRE_SCRIPT_FAILURE";
const char *JobstateLog::POST_SCRIPT_STARTED_NAME = "POST_SCRIPT_STARTED";
const char *JobstateLog::POST_SCRIPT_SUCCESS_NAME = "POST_SCRIPT_SUCCESS";
const char *JobstateLog::POST_SCRIPT_FAILURE_NAME = "POST_SCRIPT_FAILURE";

const CondorID JobstateLog::_defaultCondorID;

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
JobstateLog::WriteDagmanStarted( const CondorID &DAGManJobId )
{
	MyString info;

	info.sprintf( "INTERNAL *** DAGMAN_STARTED %d.%d ***",
				DAGManJobId._cluster, DAGManJobId._proc );
	Write( NULL, info );
}

//---------------------------------------------------------------------------
void
JobstateLog::WriteDagmanFinished( int exitCode )
{
	MyString info;

	info.sprintf( "INTERNAL *** DAGMAN_FINISHED %d ***", exitCode );
	Write( NULL, info );
}

//---------------------------------------------------------------------------
void
JobstateLog::WriteRecoveryStarted()
{
	MyString info;

	info.sprintf( "INTERNAL *** RECOVERY_STARTED ***" );
	Write( NULL, info );
}

//---------------------------------------------------------------------------
void
JobstateLog::WriteRecoveryFinished()
{
	MyString info;

	info.sprintf( "INTERNAL *** RECOVERY_FINISHED ***" );
	Write( NULL, info );
}

//---------------------------------------------------------------------------
void
JobstateLog::WriteEvent( const ULogEvent *event, Job *node )
{
	ASSERT( node );

	const char *prefix = "ULOG_";
	const char *eventName = ULogEventNumberNames[event->eventNumber];
	if ( strstr( eventName, prefix ) != eventName ) {
       	debug_printf( DEBUG_QUIET, "Warning: didn't find expected prefix "
					"%s in event name %s\n", prefix, eventName );
	} else {
		eventName = eventName + strlen( prefix );
	}

	if ( eventName != NULL ) {
		MyString condorID;
			// Make sure Condor ID is valid.
		if ( event->cluster != _defaultCondorID._cluster ) {
			condorID.sprintf( "%d.%d", event->cluster, event->proc );
		} else {
			condorID = "-";
		}
		struct tm eventTm = event->eventTime;
		time_t eventTime = mktime( &eventTm );
		Write( &eventTime, node, eventName, condorID.Value() );
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

	Write( NULL, node, eventName, retval.Value() );
}

//---------------------------------------------------------------------------
void
JobstateLog::WriteScriptStarted( Job *node, bool isPost )
{
	ASSERT( node );

	const char *eventName = isPost ? POST_SCRIPT_STARTED_NAME :
				PRE_SCRIPT_STARTED_NAME;
	Write( NULL, node, eventName, "-" );
}

//---------------------------------------------------------------------------
void
JobstateLog::WriteScriptSuccessOrFailure( Job *node, bool isPost )
{
	ASSERT( node );

	const char *eventName;
	if ( isPost ) {
		eventName = (node->retval == 0) ? POST_SCRIPT_SUCCESS_NAME :
					POST_SCRIPT_FAILURE_NAME;
	} else {
		eventName = (node->retval == 0) ? PRE_SCRIPT_SUCCESS_NAME :
					PRE_SCRIPT_FAILURE_NAME;
	}

	Write( NULL, node, eventName, "-" );
}

//---------------------------------------------------------------------------
void
JobstateLog::WriteSubmitFailure( Job *node )
{
	Write( NULL, node, "SUBMIT_FAILED", "-" );
}

//---------------------------------------------------------------------------
void
JobstateLog::Write( const time_t *eventTimeP, Job *node,
			const char *eventName, const char *condorID )
{
	MyString info;

	info.sprintf( "%s %s %s %s - %d", node->GetJobName(), eventName,
				condorID, node->PegasusSite(),
				node->GetPegasusSequenceNum() );
	Write( eventTimeP, info );
}

//---------------------------------------------------------------------------
void
JobstateLog::Write( const time_t *eventTimeP, const MyString &info )
{
	FILE *outfile = safe_fopen_wrapper( _jobstateLogFile, "a" );
	if ( !outfile ) {
       	debug_printf( DEBUG_QUIET,
					"Could not open jobstate log file %s for writing.\n",
					_jobstateLogFile );
		main_shutdown_graceful();
		return;
	}

	time_t eventTime;
	if ( eventTimeP != NULL ) {
		eventTime = *eventTimeP;
	} else {
		eventTime = time( NULL );
	}
	fprintf( outfile, "%lu %s\n",
				(unsigned long)eventTime, info.Value() );
	fclose( outfile );
}
