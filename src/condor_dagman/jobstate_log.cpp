//TEMPTEMP -- should JOB_SUCCESS, etc., have the same timestamp as the corresponding JOB_TERMINATED, etc., event? -- need to reset job timestamp on retry...
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
	_lastTimestampWritten = 0;
}

//---------------------------------------------------------------------------
JobstateLog::~JobstateLog()
{
	delete [] _jobstateLogFile;
}

//---------------------------------------------------------------------------
void
JobstateLog::InitializeRecovery()
{
		//
		// Find the timestamp of the last "real" event written to the
		// jobstate.log file.  Any events that we see in recovery mode
		// that have an earlier timestamp should *not* be re-written
		// to the jobstate.log file.  Any events with later timestamps
		// should be written.  Events with equal timestamps need to be
		// tested individually.
		//

	FILE *infile = safe_fopen_wrapper( _jobstateLogFile, "r" );
	if ( !infile ) {
		//TEMPTEMP -- should this be a fatal error?? -- what if we end up in recovery mode before we've written any events? -- hmm -- we probably should at least have DAGMAN_STARTED...
       	debug_printf( DEBUG_QUIET,
					"Could not open jobstate log file %s for reading.\n",
					_jobstateLogFile );
		main_shutdown_graceful();
		return;
	}

	MyString line;
	off_t startOfLastTimestamp = 0;

	while ( true ) {
		off_t currentOffset = ftell( infile );
		if ( !line.readLine( infile ) ) {
			break;
		}

		line.Tokenize();
		const char* timestamp = line.GetNextToken( " ", false );
		const char* nodeName = line.GetNextToken( " ", false );

			// We don't want to look at "INTERNAL" events here, or we'll
			// get goofed up by our own DAGMAN_STARTED event, etc.
		if ( strcmp( nodeName, "INTERNAL") != 0 ) {
			time_t newTimestamp;
			sscanf( timestamp, "%lu", &newTimestamp );
			if ( newTimestamp > _lastTimestampWritten ) {
				startOfLastTimestamp = currentOffset;
				_lastTimestampWritten = newTimestamp;
			}
		}
	}

		//
		// Now find all lines that match the last timestamp, and put
		// them into a hash table for future reference.
		//

	//TEMPTEMP -- check return value? -- 0 is okay
	fseek( infile, startOfLastTimestamp, SEEK_SET );
	while ( line.readLine( infile ) ) {
       	debug_printf( DEBUG_QUIET, "DIAG (last timestamp) line <%s>\n", line.Value() );//TEMPTEMP
		//TEMPTEMP -- skip "INTERNAL" ones, or else only process ones with matching timestamp...
	}

	fclose( infile );
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
	MyString info( "INTERNAL *** RECOVERY_STARTED ***" );
	Write( NULL, info );
}

//---------------------------------------------------------------------------
void
JobstateLog::WriteRecoveryFinished()
{
	MyString info( "INTERNAL *** RECOVERY_FINISHED ***" );
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
		CondorID2Str( event->cluster, event->proc, condorID );
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

	time_t timestamp = node->GetLastEventTime();
	Write( &timestamp, node, eventName, retval.Value() );
}

//---------------------------------------------------------------------------
void
JobstateLog::WriteScriptStarted( Job *node, bool isPost )
{
	ASSERT( node );

	const char *eventName = isPost ? POST_SCRIPT_STARTED_NAME :
				PRE_SCRIPT_STARTED_NAME;
	MyString condorID( "-" );
	if ( isPost ) {
			// See Dag::PostScriptReaper().
		int procID = node->GetNoop() ? node->_CondorID._proc : 0;
		CondorID2Str( node->_CondorID._cluster, procID, condorID );
	}
	time_t timestamp = node->GetLastEventTime();
	Write( &timestamp, node, eventName, condorID.Value() );
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

	MyString condorID( "-" );
	if ( isPost ) {
			// See Dag::PostScriptReaper().
		int procID = node->GetNoop() ? node->_CondorID._proc : 0;
		CondorID2Str( node->_CondorID._cluster, procID, condorID );
	}

	time_t timestamp = node->GetLastEventTime();
	Write( &timestamp, node, eventName, condorID.Value() );
}

//---------------------------------------------------------------------------
void
JobstateLog::WriteSubmitFailure( Job *node )
{
	time_t timestamp = node->GetLastEventTime();
	Write( &timestamp, node, "SUBMIT_FAILED", "-" );
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
		// Avoid "re-writing" events in recovery mode:
		// If the event time is *after* _lastTimestampWritten, we
		// write the event.  If the event time is *before*
		// _lastTimestampWritten, we don't write the event.  If
		// the times are equal, we have to do a further test down
		// below.
	if ( (eventTimeP != NULL) && (*eventTimeP < _lastTimestampWritten) ) {
		return;
	}

	FILE *outfile = safe_fopen_wrapper( _jobstateLogFile, "a" );
	if ( !outfile ) {
       	debug_printf( DEBUG_QUIET,
					"Could not open jobstate log file %s for writing.\n",
					_jobstateLogFile );
		main_shutdown_graceful();
		return;
	}

	time_t eventTime;
	if ( eventTimeP != NULL && *eventTimeP != 0 ) {
		eventTime = *eventTimeP;
	} else {
		eventTime = time( NULL );
	}
	fprintf( outfile, "%lu %s\n",
				(unsigned long)eventTime, info.Value() );
	fclose( outfile );
}

//---------------------------------------------------------------------------
void
JobstateLog::CondorID2Str( int cluster, int proc, MyString &idStr )
{
		// Make sure Condor ID is valid.
	if ( cluster != _defaultCondorID._cluster ) {
		idStr.sprintf( "%d.%d", cluster, proc );
	} else {
		idStr = "-";
	}
}
