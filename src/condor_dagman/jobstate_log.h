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

#include "condor_event.h"
#include "job.h"

class JobstateLog {
public:
	/** Constructor.
	TEMPTEMP -- document
	*/
	JobstateLog( const char *jobstateLogFile );

	/** Destructor.
	*/
	~JobstateLog();

	//TEMPTEMP -- document
	//TEMPTEMP -- don't return void??
	void WriteEvent( const ULogEvent *event, Job *node );

	void WriteJobSuccessOrFailure( Job *node );

	//TEMPTEMP -- add more method(s) for writing the POST_SCRIPT_STARTED and POST_SCRIPT_SUCCESS|POST_SCRIPT_FAILED "events"

	const char *LogFile() { return _jobstateLogFile; }

private:
	void Write( Job *node, const char *eventName,
				const char *condorID );

	char *_jobstateLogFile;

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
