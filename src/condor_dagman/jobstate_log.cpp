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
// The purpose of this method is to allow us (in recovery mode) to avoid
// re-writing duplicates of events that we already wrote in the pre-recovery
// part of the jobstate.log file.  We do a two-part check: first, we find
// the timestamp of the last "real" (not "INTERNAL") pre-recovery event.
// Then, in recovery mode, any event that has a timestamp earlier than that
// should *not* be written; any event that has a later timestamp *should*
// be written.  The second part of the check is that we keep a copy of
// every pre-recovery line with the same timestamp as the last timestamp.
// Then, in recovery mode, if we get an event with a timestamp that matches
// the last pre-recovery timestamp, we check it against the list of "last
// timestamp" lines, and don't write it if it's in that list.
void
JobstateLog::InitializeRecovery()
{
	debug_printf( DEBUG_DEBUG_2, "JobstateLog::InitializeRecovery()\n" );

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
			// This is a fatal error, because by the time we get here,
			// we should, at the very least, have written the
			// DAGMAN_STARTED "event".
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

		time_t newTimestamp;
		MyString nodeName;
		if ( ParseLine( line, newTimestamp, nodeName ) ) {
				// We don't want to look at "INTERNAL" events here, or we'll
				// get goofed up by our own DAGMAN_STARTED event, etc.
			if ( nodeName != "INTERNAL" ) {
				if ( newTimestamp > _lastTimestampWritten ) {
					startOfLastTimestamp = currentOffset;
					_lastTimestampWritten = newTimestamp;
				}
			}
		}
	}

	debug_printf( DEBUG_DEBUG_2, "_lastTimestampWritten: %ld\n",
				(unsigned long)_lastTimestampWritten );

		//
		// Now find all lines that match the last timestamp, and put
		// them into a hash table for future reference.
		//
	if ( fseek( infile, startOfLastTimestamp, SEEK_SET ) != 0 ) {
		debug_printf( DEBUG_QUIET,
					"Error seeking in jobstate log file %s.\n",
					_jobstateLogFile );
	}

	while ( line.readLine( infile ) ) {
		time_t newTimestamp;
		MyString nodeName;
		if ( ParseLine( line, newTimestamp, nodeName ) ) {
			if ( (newTimestamp == _lastTimestampWritten) &&
						(nodeName != "INTERNAL") ) {
				_lastTimestampLines.insert( line );
				debug_printf( DEBUG_DEBUG_2,
							"Appended <%s> to _lastTimestampLines\n",
							line.Value() );
			}
		}
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
		//
		// Here for "fake" events like JOB_SUCCESS, the event will get
		// the timestamp of the last "real" event from the job; this is
		// so that we can correctly avoid re-writing the "fake" events
		// in recovery mode.
		//
	time_t eventTime;
	if ( eventTimeP != NULL && *eventTimeP != 0 ) {
		eventTime = *eventTimeP;
	} else {
		eventTime = time( NULL );
	}

		// Avoid "re-writing" events in recovery mode:
		// If the event time is *after* _lastTimestampWritten, we
		// write the event.  If the event time is *before*
		// _lastTimestampWritten, we don't write the event.  If
		// the times are equal, we have to do a further test down
		// below.
	if ( eventTime < _lastTimestampWritten ) {
		return;
	}

	MyString outline;
	outline.sprintf( "%lu %s", (unsigned long)eventTime, info.Value() );

		//
		// If this event's time matches the time of the last "real"
		// event in the pre-recovery part of the file, we check whether
		// this line is already in the pre-recovery part of the file,
		// and if it is we don't write it again.
		//
	if ( (eventTime == _lastTimestampWritten) &&
				(_lastTimestampLines.count( outline ) > 0) ) {
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

	fprintf( outfile, "%s\n", outline.Value() );

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

//---------------------------------------------------------------------------
bool
JobstateLog::ParseLine( MyString &line, time_t &timestamp,
			MyString &nodeName )
{
	line.chomp();
	line.Tokenize();
	const char* timestampTok = line.GetNextToken( " ", false );
	const char* nodeNameTok = line.GetNextToken( " ", false );

	if ( (timestampTok == NULL) || (nodeNameTok == NULL) ) {
		debug_printf( DEBUG_QUIET, "Warning: error parsing "
					"jobstate.log file line <%s>\n", line.Value() );
		return false;
	}

	int items = sscanf( timestampTok, "%lu", &timestamp );
	if ( items != 1 ) {
		debug_printf( DEBUG_QUIET, "Warning: error reading "
					"timestamp in jobstate.log file line <%s>\n",
					line.Value() );
		return false;
	}

	nodeName = nodeNameTok;

	return true;
}
