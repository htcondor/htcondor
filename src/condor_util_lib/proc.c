/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

 
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


int		DontDisplayTime;


#define SECOND	1
#define MINUTE	(60 * SECOND)
#define HOUR	(60 * MINUTE)
#define DAY		(24 * HOUR)

char	*
format_time( fp_secs )
float		fp_secs;
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


#if defined(NEW_PROC) && !defined(WIN32)
/*
  Functions for building and deleteing a version 3 proc structure.
  ConstructProc returns a proc with all fields except cmd, args, in, out,
  err, remote_usage, exit_status.
*/

PROC	*
ConstructProc( n_cmds, proc_template )
int		n_cmds;
PROC	*proc_template;
{
	PROC	*proc;

	proc = (PROC *) MALLOC( sizeof(PROC) );
	if ( proc == NULL ) {
		return NULL;
	}

	if ( proc_template != NULL ) {
		*proc = *proc_template;
	}

	proc->n_cmds = n_cmds;
	proc->cmd = (char **) MALLOC( sizeof( char *) * n_cmds );
	if ( proc->cmd == NULL) {
		FREE( proc );
		return NULL;
	}
	proc->args = (char **) MALLOC( sizeof( char *) * n_cmds );
	if ( proc->args == NULL) {
		FREE( proc->cmd );
		FREE( proc );
		return NULL;
	}
	proc->in = (char **) MALLOC( sizeof( char *) * n_cmds );
	if ( proc->in == NULL) {
		FREE( proc->cmd );
		FREE( proc->args );
		FREE( proc );
		return NULL;
	}
	proc->out = (char **) MALLOC( sizeof( char *) * n_cmds );
	if ( proc->out == NULL) {
		FREE( proc->cmd );
		FREE( proc->args );
		FREE( proc->in );
		FREE( proc );
		return NULL;
	}
	proc->err = (char **) MALLOC( sizeof( char *) * n_cmds );
	if ( proc->err == NULL) {
		FREE( proc->cmd );
		FREE( proc->args );
		FREE( proc->in );
		FREE( proc->out );
		FREE( proc );
		return NULL;
	}
	proc->remote_usage = (struct rusage *) MALLOC( sizeof(struct rusage) * 
												  n_cmds );
	if ( proc->remote_usage == NULL) {
		FREE( proc->cmd );
		FREE( proc->args );
		FREE( proc->in );
		FREE( proc->out );
		FREE( proc->err );
		FREE( proc );
		return NULL;
	}
	proc->exit_status = (int *) MALLOC( sizeof(int) * n_cmds ); 

	if ( proc->err == NULL) {
		FREE( proc->cmd );
		FREE( proc->args );
		FREE( proc->in );
		FREE( proc->out );
		FREE( proc->err );
		FREE( proc->remote_usage );
		FREE( proc );
		return NULL;
	}
	return proc;
}

void
DestructProc( proc )
PROC	*proc;
{
	FREE( proc->cmd );
	FREE( proc->args );
	FREE( proc->in );
	FREE( proc->out );
	FREE( proc->err );
	FREE( proc->remote_usage );
	FREE( proc );
}
#endif /* NEW_PROC */

