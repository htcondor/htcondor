/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
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

#if defined(OSF1)
#	define _OSF_SOURCE
#endif

#if defined(OSF1) || defined(AIX32)
#	define _BSD
#endif

#include <sys/wait.h>
#include "proc.h"
#include "debug.h"

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

char *Notifications[] = {
	"Never", "Always", "Complete", "Error"
};

char	*Status[] = {
	"Unexpanded", "Idle", "Running", "Removed", "Completed", "Submission Err"
};


#ifdef NEW_PROC
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

