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

#if defined(OSF1)
#	define _OSF_SOURCE
#endif

#if defined(OSF1) || defined(AIX32)
#	define _BSD
#endif

#ifndef WIN32
#include <sys/wait.h>
#endif
#include "proc.h"
#include "condor_debug.h"

#if defined(AIX31) || defined(AIX32)
#include <time.h>
#endif

#include "util_lib_proto.h"

int		DontDisplayTime;


#define SECOND	1
#define MINUTE	(60 * SECOND)
#define HOUR	(60 * MINUTE)
#define DAY		(24 * HOUR)

char	*
format_time( float fp_secs )
{
	int		days;
	int		hours;
	int		min;
	int		secs;
	int		tot_secs = fp_secs;
	static char	answer[25];

	days = tot_secs / DAY;
	tot_secs %= DAY;
	hours = tot_secs / HOUR;
	tot_secs %= HOUR;
	min = tot_secs / MINUTE;
	secs = tot_secs % MINUTE;

	(void)sprintf( answer, "%3d+%02d:%02d:%02d", days, hours, min, secs );
	return answer;
}


static const char* JobStatusNames[] = {
    "UNEXPANDED",
    "IDLE",
    "RUNNING",
    "REMOVED",
    "COMPLETED",
	"HELD",
    "SUBMISSION_ERR",
};


const char*
getJobStatusString( int status )
{
	if( status<0 || status>=JOB_STATUS_MAX ) {
		return "UNKNOWN";
	}
	return JobStatusNames[status];
}


int
getJobStatusNum( const char* name )
{
	if( ! name ) {
		return -1;
	}
	if( stricmp(name,"UNEXPANDED") == MATCH ) {
		return UNEXPANDED;
	}
	if( stricmp(name,"IDLE") == MATCH ) {
		return IDLE;
	}
	if( stricmp(name,"RUNNING") == MATCH ) {
		return RUNNING;
	}
	if( stricmp(name,"REMOVED") == MATCH ) {
		return REMOVED;
	}
	if( stricmp(name,"COMPLETED") == MATCH ) {
		return COMPLETED;
	}
	if( stricmp(name,"HELD") == MATCH ) {
		return HELD;
	}
	if( stricmp(name,"SUBMISSION_ERR") == MATCH ) {
		return SUBMISSION_ERR;
	}
	return -1;
}


