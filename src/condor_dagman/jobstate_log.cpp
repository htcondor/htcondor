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
#include "MyString.h"
#include "debug.h"
#include "jobstate_log.h"
#include "dagman_main.h"

	// The names of the pseudo-events we're going to write (for "real"
	// events, we use the event names defined in condor_event.h).
static const char *JOB_SUCCESS_NAME = "JOB_SUCCESS";
static const char *JOB_FAILURE_NAME = "JOB_FAILURE";
static const char *PRE_SCRIPT_STARTED_NAME = "PRE_SCRIPT_STARTED";
static const char *PRE_SCRIPT_SUCCESS_NAME = "PRE_SCRIPT_SUCCESS";
static const char *PRE_SCRIPT_FAILURE_NAME = "PRE_SCRIPT_FAILURE";
static const char *POST_SCRIPT_STARTED_NAME = "POST_SCRIPT_STARTED";
static const char *POST_SCRIPT_SUCCESS_NAME = "POST_SCRIPT_SUCCESS";
static const char *POST_SCRIPT_FAILURE_NAME = "POST_SCRIPT_FAILURE";
static const char *INTERNAL_NAME = "INTERNAL";
static const char *DAGMAN_STARTED_NAME = "DAGMAN_STARTED";
static const char *DAGMAN_FINISHED_NAME = "DAGMAN_FINISHED";
static const char *RECOVERY_STARTED_NAME = "RECOVERY_STARTED";
static const char *RECOVERY_FINISHED_NAME = "RECOVERY_FINISHED";
static const char *RECOVERY_FAILURE_NAME = "RECOVERY_FAILURE";
static const char *SUBMIT_FAILURE_NAME = "SUBMIT_FAILURE";

	// Default HTCondor ID to use to check for invalid IDs.
static const CondorID DEFAULT_CONDOR_ID;

//---------------------------------------------------------------------------
JobstateLog::JobstateLog()
{
	_jobstateLogFile = NULL;
	_outfile = NULL;
	_lastTimestampWritten = 0;
}

//---------------------------------------------------------------------------
JobstateLog::~JobstateLog()
{
	free( _jobstateLogFile );
	if ( _outfile ) {
		fclose( _outfile );
		_outfile = NULL;
	}
}

//---------------------------------------------------------------------------
void
JobstateLog::Flush()
{
	if ( !_jobstateLogFile ) {
		return;
	}

	if ( fflush( _outfile ) != 0 ) {
		debug_printf( DEBUG_QUIET,
					"Error flushing output to jobstate log file %s.\n",
					_jobstateLogFile );
		main_shutdown_graceful();
	}
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

// Note: for this to work correctly, it's vital that the events we generate
// in recovery mode exactly match how they were output in "non-recovery"
// mode, so we can compare timestamps, and, if the timestamp matches
// the last pre-recovery timestamp, the entire event string.
void
JobstateLog::InitializeRecovery()
{
	debug_printf( DEBUG_DEBUG_2, "JobstateLog::InitializeRecovery()\n" );

	if ( !_jobstateLogFile ) {
		return;
	}

		//
		// Find the timestamp of the last "real" event written to the
		// jobstate.log file.  Any events that we see in recovery mode
		// that have an earlier timestamp should *not* be re-written
		// to the jobstate.log file.  Any events with later timestamps
		// should be written.  Events with equal timestamps need to be
		// tested individually.
		//

	FILE *infile = safe_fopen_wrapper_follow( _jobstateLogFile, "r" );
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
		int seqNum;
		if ( ParseLine( line, newTimestamp, nodeName, seqNum ) ) {
				// We don't want to look at "INTERNAL" events here, or we'll
				// get goofed up by our own DAGMAN_STARTED event, etc.
			if ( nodeName != INTERNAL_NAME ) {
					// Note: we don't absolutely rely on the timestamps
					// being in order -- the > below rather than == is
					// important in that case.
				if ( newTimestamp > _lastTimestampWritten ) {
					startOfLastTimestamp = currentOffset;
					_lastTimestampWritten = newTimestamp;
				}
			}
		}
	}

	debug_printf( DEBUG_DEBUG_2, "_lastTimestampWritten: %lu\n",
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
		int seqNum;
		if ( ParseLine( line, newTimestamp, nodeName, seqNum ) ) {
			if ( (newTimestamp == _lastTimestampWritten) &&
						(nodeName != INTERNAL_NAME) ) {
				_lastTimestampLines.insert( line );
				debug_printf( DEBUG_DEBUG_2,
							"Appended <%s> to _lastTimestampLines\n",
							line.c_str() );
			}
		}
	}

	fclose( infile );
}

//---------------------------------------------------------------------------
// Here we re-read the jobstate.log file to find out what sequence number
// we should start with when running a rescue DAG.
void
JobstateLog::InitializeRescue()
{
	debug_printf( DEBUG_DEBUG_2, "JobstateLog::InitializeRescue()\n" );

	if ( !_jobstateLogFile ) {
		return;
	}

	FILE *infile = safe_fopen_wrapper_follow( _jobstateLogFile, "r" );
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

	int maxSeqNum = 0;
	MyString line;

	while ( line.readLine( infile ) ) {
		time_t newTimestamp;
		MyString nodeName;
		int seqNum;
		if ( ParseLine( line, newTimestamp, nodeName, seqNum ) ) {
			maxSeqNum = MAX( maxSeqNum, seqNum );
		}
	}

	fclose( infile );

	debug_printf( DEBUG_DEBUG_2,
				"Max sequence num in jobstate.log file: %d\n", maxSeqNum );

	Job::SetJobstateNextSequenceNum( maxSeqNum + 1 );
}

//---------------------------------------------------------------------------
void
JobstateLog::WriteDagmanStarted( const CondorID &DAGManJobId )
{
	if ( !_jobstateLogFile ) {
		return;
	}

	MyString info;
	info.formatstr( "%s *** %s %d.%d ***", INTERNAL_NAME, DAGMAN_STARTED_NAME,
				DAGManJobId._cluster, DAGManJobId._proc );

	Write( NULL, info );
	Flush();
}

//---------------------------------------------------------------------------
void
JobstateLog::WriteDagmanFinished( int exitCode )
{
	if ( !_jobstateLogFile ) {
		return;
	}

	MyString info;
	info.formatstr( "%s *** %s %d ***", INTERNAL_NAME, DAGMAN_FINISHED_NAME,
				exitCode );

	Write( NULL, info );
	Flush();
}

//---------------------------------------------------------------------------
void
JobstateLog::WriteRecoveryStarted()
{
	if ( !_jobstateLogFile ) {
		return;
	}

	MyString info;
	info.formatstr( "%s *** %s ***", INTERNAL_NAME, RECOVERY_STARTED_NAME );
	Write( NULL, info );
}

//---------------------------------------------------------------------------
void
JobstateLog::WriteRecoveryFinished()
{
	if ( !_jobstateLogFile ) {
		return;
	}

	MyString info;
	info.formatstr( "%s *** %s ***", INTERNAL_NAME, RECOVERY_FINISHED_NAME );
	Write( NULL, info );
	Flush();
}

//---------------------------------------------------------------------------
void
JobstateLog::WriteRecoveryFailure()
{
	if ( !_jobstateLogFile ) {
		return;
	}

	MyString info;
	info.formatstr( "%s *** %s ***", INTERNAL_NAME, RECOVERY_FAILURE_NAME );
	Write( NULL, info );
	Flush();
}

//---------------------------------------------------------------------------
void
JobstateLog::WriteEvent( const ULogEvent *event, Job *node )
{
	if ( !_jobstateLogFile ) {
		return;
	}

	ASSERT( node );

	const char *prefix = "ULOG_";
	const char *eventName = event->eventName();
	if ( strstr( eventName, prefix ) != eventName ) {
       	debug_printf( DEBUG_QUIET, "Warning: didn't find expected prefix "
					"%s in event name %s\n", prefix, eventName );
		check_warning_strictness( DAG_STRICT_1 );
	} else {
		eventName = eventName + strlen( prefix );
	}

	if ( eventName != NULL ) {
		MyString condorID;
		CondorID2Str( event->cluster, event->proc, condorID );
		time_t eventTime = event->GetEventclock();
		Write( &eventTime, node, eventName, condorID.c_str() );
	}
}

//---------------------------------------------------------------------------
void
JobstateLog::WriteJobSuccessOrFailure( Job *node )
{
	if ( !_jobstateLogFile ) {
		return;
	}

	ASSERT( node );

	const char *eventName = node->retval == 0 ?
				JOB_SUCCESS_NAME : JOB_FAILURE_NAME;
	MyString retval;
	retval.formatstr( "%d", node->retval );

	time_t timestamp = node->GetLastEventTime();
	Write( &timestamp, node, eventName, retval.c_str() );
}

//---------------------------------------------------------------------------
void
JobstateLog::WriteScriptStarted( Job *node, ScriptType type )
{
	if ( !_jobstateLogFile ) {
		return;
	}

	// Do not log any HOLD script events
	if ( type == ScriptType::HOLD ) return;

	ASSERT( node );

	const char *eventName = NULL;
	if ( type == ScriptType::POST ) {
		eventName = POST_SCRIPT_STARTED_NAME;
	} else if ( type == ScriptType::PRE ) {
		eventName = PRE_SCRIPT_STARTED_NAME;
	}

	MyString condorID( "-" );
	if ( type == ScriptType::POST ) {
			// See Dag::PostScriptReaper().
		int procID = node->GetNoop() ? node->GetProc() : 0;
		CondorID2Str( node->GetCluster(), procID, condorID );
	}
	time_t timestamp = node->GetLastEventTime();
	Write( &timestamp, node, eventName, condorID.c_str() );
}

//---------------------------------------------------------------------------
void
JobstateLog::WriteScriptSuccessOrFailure( Job *node, ScriptType type )
{
	if ( !_jobstateLogFile ) {
		return;
	}

	// Do not log any HOLD script events
	if ( type == ScriptType::HOLD ) return;

	ASSERT( node );

	const char *eventName = NULL;
	if ( type == ScriptType::POST ) {
		eventName = (node->retval == 0) ? POST_SCRIPT_SUCCESS_NAME :
					POST_SCRIPT_FAILURE_NAME;
	} else if ( type == ScriptType::PRE ) {
		eventName = (node->retval == 0) ? PRE_SCRIPT_SUCCESS_NAME :
					PRE_SCRIPT_FAILURE_NAME;
	}

	MyString condorID( "-" );
	if ( type == ScriptType::POST ) {
			// See Dag::PostScriptReaper().
		int procID = node->GetNoop() ? node->GetProc() : 0;
		CondorID2Str( node->GetCluster(), procID, condorID );
	}

	time_t timestamp = node->GetLastEventTime();
	Write( &timestamp, node, eventName, condorID.c_str() );
}

//---------------------------------------------------------------------------
void
JobstateLog::WriteSubmitFailure( Job *node )
{
	if ( !_jobstateLogFile ) {
		return;
	}

	time_t timestamp = node->GetLastEventTime();
	Write( &timestamp, node, SUBMIT_FAILURE_NAME, "-" );
}

//---------------------------------------------------------------------------
void
JobstateLog::Write( const time_t *eventTimeP, Job *node,
			const char *eventName, const char *condorID )
{
	MyString info;

	info.formatstr( "%s %s %s %s - %d", node->GetJobName(), eventName,
				condorID, node->GetJobstateJobTag(),
				node->GetJobstateSequenceNum() );
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
	outline.formatstr( "%lu %s", (unsigned long)eventTime, info.c_str() );

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

	if ( !_outfile ) {
		_outfile = safe_fopen_wrapper_follow( _jobstateLogFile, "a" );
		if ( !_outfile ) {
       		debug_printf( DEBUG_QUIET,
						"Could not open jobstate log file %s for writing.\n",
						_jobstateLogFile );
			main_shutdown_graceful();
			return;
		}
	}

	fprintf( _outfile, "%s\n", outline.c_str() );
}

//---------------------------------------------------------------------------
void
JobstateLog::CondorID2Str( int cluster, int proc, MyString &idStr )
{
		// Make sure HTCondor ID is valid.
	if ( cluster != DEFAULT_CONDOR_ID._cluster ) {
		idStr.formatstr( "%d.%d", cluster, proc );
	} else {
		idStr = "-";
	}
}

//---------------------------------------------------------------------------
// This does only partial parsing -- only what we need for recovery mode
// and rescue initialization.
bool
JobstateLog::ParseLine( MyString &line, time_t &timestamp,
			MyString &nodeName, int &seqNum )
{
	line.chomp();
	MyStringTokener tok;
	tok.Tokenize(line.c_str());
	const char* timestampTok = tok.GetNextToken( " ", false );
	const char* nodeNameTok = tok.GetNextToken( " ", false );
	(void)tok.GetNextToken( " ", false ); // event name
	(void)tok.GetNextToken( " ", false ); // condor id
	(void)tok.GetNextToken( " ", false ); // job tag (pegasus site)
	(void)tok.GetNextToken( " ", false ); // unused
	const char* seqNumTok = tok.GetNextToken( " ", false );

	if ( (timestampTok == NULL) || (nodeNameTok == NULL) ) {
		debug_printf( DEBUG_QUIET, "Warning: error parsing "
					"jobstate.log file line <%s>\n", line.c_str() );
		check_warning_strictness( DAG_STRICT_1 );
		return false;
	}

		// fetch the number, and get a pointer to the first char after
		// if the pointer did not advance, then there was no number to parse.
	char *pend;
	timestamp = (time_t)strtoll(timestampTok, &pend, 10);

	if (pend == timestampTok) {
		debug_printf( DEBUG_QUIET, "Warning: error reading "
					"timestamp in jobstate.log file line <%s>\n",
					line.c_str() );
		check_warning_strictness( DAG_STRICT_1 );
		return false;
	}

	nodeName = nodeNameTok;

	seqNum = 0;
	if ( seqNumTok ) {
		seqNum = (int)strtol(seqNumTok, &pend, 10);
		if (pend == seqNumTok) {
			debug_printf( DEBUG_QUIET, "Warning: error reading "
						"sequence number in jobstate.log file line <%s>\n",
						line.c_str() );
			check_warning_strictness( DAG_STRICT_1 );
			return false;
		}
	}

	return true;
}
