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


#ifndef _JOBSTATE_LOG_H
#define _JOBSTATE_LOG_H

// This class handles the writing of the jobstate.log file for Pegasus
// (this file was formerly created by the Pegasus tailstatd parsing
// the dagman.out file).

// Note: the jobstate.log file is meant to be machine-parseable, so
// no changes to the format of the output should be made without
// consulting the Pegasus developers.

// Note: if a jobstate.log file is specified, failing to write to the
// file is a fatal error.

#include "condor_event.h"
#include "job.h"

class JobstateLog {
public:
	/** Constructor.
		@param The name of the jobstate.log file to write to.
	*/
	JobstateLog( const char *jobstateLogFile );

	/** Destructor.
	*/
	~JobstateLog();

	/** Write the DAGMAN_STARTED "event".
		@param The Condor ID of the DAGMan job itself.
	*/
	void WriteDagmanStarted( const CondorID &DAGManJobId );

	/** Write an actual event.  Note that not all events will be written
		-- only the ones that Pegasus cares about.
		@param The event.
		@param The DAG node corresponding to the event.
	*/
	void WriteEvent( const ULogEvent *event, const Job *node );

	/** Write the JOB_SUCCESS or JOB_FAILURE "event".
		@param The DAG node corresponding to the "event".
	*/
	void WriteJobSuccessOrFailure( const Job *node );

	/** Write the POST_SCRIPT_STARTED "event".
		@param The DAG node corresponding to the "event".
	*/
	void WritePostStart( const Job *node );

	/** Write the POST_SCRIPT_SUCCESS or POST_SCRIPT_FAILURE "event".
		@param The DAG node corresponding to the "event".
	*/
	void WritePostSuccessOrFailure( const Job *node );

	/** Write the DAGMAN_FINISHED "event".
		@param The DAGMan exit code.
	*/
	void WriteDagmanFinished( int exitCode );

	/** Get the jobstate.log file.
		@return The jobstate.log file we're writing to.
	*/
	const char *LogFile() { return _jobstateLogFile; }

private:
	/** Write an event to the jobstate.log file.
		@param The DAG node corresponding to the "event".
		@param The event name.
		@param The Condor ID string (or other data).
	*/
	void Write( const Job *node, const char *eventName,
				const char *condorID );

	/** Write an event to the jobstate.log file.
		@param The string we want to write to the file.
	*/
	void Write( const MyString &info );

		// The jobstate.log file we're writing to.
	char *_jobstateLogFile;

		// The names of the events we're going to write.
	static const char *SUBMIT_NAME;
	static const char *EXECUTE_NAME;
	static const char *GLOBUS_SUBMIT_NAME;
	static const char *GRID_SUBMIT_NAME;
	static const char *JOB_TERMINATED_NAME;
	static const char *JOB_SUCCESS_NAME;
	static const char *JOB_FAILURE_NAME;
	static const char *POST_SCRIPT_TERMINATED_NAME;
	static const char *POST_SCRIPT_STARTED_NAME;
	static const char *POST_SCRIPT_SUCCESS_NAME;
	static const char *POST_SCRIPT_FAILURE_NAME;
};

#endif	// _JOBSTATE_LOG_H
