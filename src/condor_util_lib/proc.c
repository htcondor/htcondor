/* 
** Copyright 1986, 1987, 1988, 1989, 1990, 1991 by the Condor Design Team
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the Condor Design Team not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the Condor
** Design Team make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE CONDOR DESIGN TEAM DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE CONDOR DESIGN TEAM BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Authors:  Allan Bricker and Michael J. Litzkow,
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 


#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#if defined(OSF1)
#define _OSF_SOURCE
#define _BSD
#endif
#include <sys/wait.h>
#include "proc.h"
#include "debug.h"

#if defined(AIX31) || defined(AIX32)
#include <time.h>
#endif

#include "clib.h"

int		DontDisplayTime;

display_proc_short( proc )
PROC	*proc;
{
	char		activity;
	struct tm	*tm, *localtime();
	char		*format_time();
	char		cmd[23];
	int			len;
	char		*src, *dst;

	switch( proc->status ) {
		case UNEXPANDED:
			activity = 'U';
			break;
		case IDLE:
			activity = 'I';
			break;
		case RUNNING:
			activity = 'R';
			break;
		case COMPLETED:
			activity = 'C';
			break;
		case REMOVED:
			activity = 'X';
			break;
		default:
			activity = ' ';
	}

	tm = localtime( (time_t *)&proc->q_date );

		/* put as much of the cmd and program args as will fit in 18 spaces */
	len = 18;
#if defined(NEW_PROC)
	src = proc->cmd[0];
#else
	src = proc->cmd;
#endif
	dst = cmd;
	while( len && *src ) {
		*dst++ = *src++;
		len -= 1;
	}
	if( len > 0 ) {
		*dst++ = ' ';
		len -= 1;
	}
#if defined( NEW_PROC )
	src = proc->args[0];
#else
	src = proc->args;
#endif
	while( len && *src ) {
		*dst++ = *src++;
		len -= 1;
	}
	*dst = '\0';

		/* trunc the owner's login down to 14 spaces */
	if( strlen(proc->owner) > 14 )
		proc->owner[14]= '\0';


	printf( "%4d.%-3d %-14s %2d/%-2d %02d:%02d %s %-2c %-3d %-4.1f %-18s\n",
		proc->id.cluster, proc->id.proc, proc->owner,
		(tm->tm_mon)+1, tm->tm_mday, tm->tm_hour, tm->tm_min,
		/* format_time(proc->cpu_time), */ 
#if defined(NEW_PROC)
		format_time((float)proc->remote_usage[0].ru_utime.tv_sec),
#else
		format_time((float)proc->remote_usage.ru_utime.tv_sec),
#endif
		activity, proc->prio, proc->image_size/1024.0, cmd );
			
}

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

display_proc_long( proc )
PROC	*proc;
{
	switch( proc->version_num ) {
#if defined(NEW_PROC)
		case 2:
			display_v2_proc_long( (V2_PROC *)proc );
			break;
		case 3:
			display_v3_proc_long( proc );
			break;
#else
		case 2:
			display_v2_proc_long( proc );
#endif
		default:
			printf( "Unknown proc structure version number (%d)\n",
													proc->version_num );
			break;
	}
}

#if defined(NEW_PROC)
display_v3_proc_long( proc )
PROC	*proc;
{
	int		i;

	(void)putchar( '\n' );
	printf( "Structure Version: %d\n", proc->version_num );
	printf( "Id: %d.%d\n", proc->id.cluster, proc->id.proc );
	switch( proc->universe ) {
		case STANDARD:
			printf( "Universe: Standard\n" );
			break;
		case PIPE:
			printf( "Universe: Pipe\n" );
			break;
		case LINDA:
			printf( "Universe: Linda\n" );
			break;
		case PVM:
			printf( "Universe: PVM\n" );
			break;
		default:
			printf( "Universe: UNKNOWN (%d)\n", proc->universe );
	}
	printf( "Checkpoint: %s\n", proc->checkpoint ? "TRUE" : "FALSE" );
	printf( "Remote Syscalls: %s\n", proc->remote_syscalls ? "TRUE" : "FALSE" );
	printf( "Owner: %s\n", proc->owner );
	printf( "Queue Date: %s", ctime( (time_t *)&proc->q_date ) );

	if( proc->status == COMPLETED ) {
		if( proc->completion_date == 0 ) {
			printf( "Completion Date: (not recorded)\n" );
		} else {
			printf( "Completion Date: %s",
								ctime((time_t *)&proc->completion_date));
		}
	} else {
		printf( "Completion Date: (not completed)\n" );
	}

	printf( "Status: %s\n", Status[proc->status] );
	printf( "Priority: %d\n", proc->prio );
	printf( "Notification: %s\n", Notifications[proc->notification]);

	if( proc->image_size ) {
		printf( "Virtual Image Size: %d kilobytes\n", proc->image_size );
	} else {
		printf( "Virtual Image Size: (not recorded)\n" );
	}
	printf( "Env: %s\n", proc->env );


		/* Per command information */
	for( i=0; i<proc->n_cmds; ) {
		printf( "\n" );
		printf( "Cmd: %s\n", proc->cmd[i] );
		printf( "Args: %s\n", proc->args[i] );
		printf( "In: %s\n", proc->in[i] );
		printf( "Out: %s\n", proc->out[i] );
		printf( "Err: %s\n", proc->err[i] );
		if( !DontDisplayTime ) {	/* called by condor_submit */
			printf( "Remote User Time:     %s\n",
				format_time((float)proc->remote_usage[i].ru_utime.tv_sec) );
			printf( "Remote System Time:   %s\n",
				format_time((float)proc->remote_usage[i].ru_stime.tv_sec) );
			printf( "Total Remote Time:    %s\n",
				format_time((float)proc->remote_usage[i].ru_utime.tv_sec +
									proc->remote_usage[i].ru_stime.tv_sec) );
		}
		if( ++i == proc->n_cmds ) {
			printf( "\n" );
		}
	}

		/* Per pipe information */
	if ( proc->universe == PIPE ) {
		for( i=0; i<proc->n_pipes; ) {
			printf( "\n" );
			printf( "Writer: %d\n", proc->pipe[i].writer );
			printf( "Reader: %d\n", proc->pipe[i].reader );
			printf( "Name: %s\n", proc->pipe[i].name );
			if( ++i == proc->n_pipes ) {
				printf( "\n" );
			}
		}
	} else if ( proc->universe == PVM ) {
		printf("\n");
		printf("Machine Requirements: %d..", (proc->min_needed));
		if (proc->max_needed == -1) {
			printf("%s\n", "infinite");
		} else {
			printf("%d\n", proc->max_needed);
		}
	}

	printf( "RootDir: %s\n", proc->rootdir );
	printf( "Initial Working Directory: %s\n", proc->iwd );
	printf( "Requirements: %s\n", proc->requirements );
	printf( "Preferences: %s\n", proc->preferences );

		/* If called by "condor_submit", then all cpu times are known to be
		   zero, so don't display them. */
	if( DontDisplayTime ) {
		return;
	}

	/*
	**	Should print rusage...
	printf( "Cpu Time: %f\n", proc->cpu_time );
	*/
	printf( "Local User Time:      %s\n",
		format_time((float)proc->local_usage.ru_utime.tv_sec) );
	printf( "Local System Time:    %s\n",
		format_time((float)proc->local_usage.ru_stime.tv_sec) );
	printf( "Total Local Time:     %s\n",
		format_time((float)proc->local_usage.ru_utime.tv_sec +
							proc->local_usage.ru_stime.tv_sec) );

}
#endif

display_v2_proc_long( proc )
#if defined(NEW_PROC)
V2_PROC	*proc;
#else
PROC	*proc;
#endif
{
	(void)putchar( '\n' );
	printf( "Structure Version: %d\n", proc->version_num );
	printf( "Id: %d.%d\n", proc->id.cluster, proc->id.proc );
	printf( "Owner: %s\n", proc->owner );
	printf( "Queue Date: %s", ctime( (time_t *)&proc->q_date ) );

	if( proc->status == COMPLETED ) {
		if( proc->completion_date == 0 ) {
			printf( "Completion Date: (not recorded)\n" );
		} else {
			printf( "Completion Date: %s",
								ctime((time_t *)&proc->completion_date));
		}
	} else {
		printf( "Completion Date: (not completed)\n" );
	}

	printf( "Status: %s\n", Status[proc->status] );
	printf( "Priority: %d\n", proc->prio );
	printf( "Notification: %s\n", Notifications[proc->notification]);

	if( proc->image_size ) {
		printf( "Virtual Image Size: %d kilobytes\n", proc->image_size );
	} else {
		printf( "Virtual Image Size: (not recorded)\n" );
	}

	printf( "Cmd: %s\n", proc->cmd );
	printf( "Args: %s\n", proc->args );
	printf( "Env: %s\n", proc->env );
	printf( "In: %s\n", proc->in );
	printf( "Out: %s\n", proc->out );
	printf( "Err: %s\n", proc->err );
	printf( "RootDir: %s\n", proc->rootdir );
	printf( "Initial Working Directory: %s\n", proc->iwd );
	printf( "Requirements: %s\n", proc->requirements );
	printf( "Preferences: %s\n", proc->preferences );

		/* If we're called by "condor", then all cpu times are known to be
		   zero, so don't display them. */
	if( DontDisplayTime ) {
		return;
	}

	/*
	**	Should print rusage...
	printf( "Cpu Time: %f\n", proc->cpu_time );
	*/
	printf( "Local User Time:      %s\n",
		format_time((float)proc->local_usage.ru_utime.tv_sec) );
	printf( "Local System Time:    %s\n",
		format_time((float)proc->local_usage.ru_stime.tv_sec) );
	printf( "Total Local Time:     %s\n",
		format_time((float)proc->local_usage.ru_utime.tv_sec +
							proc->local_usage.ru_stime.tv_sec) );

	printf( "Remote User Time:     %s\n",
		format_time((float)proc->remote_usage.ru_utime.tv_sec) );
	printf( "Remote System Time:   %s\n",
		format_time((float)proc->remote_usage.ru_stime.tv_sec) );
	printf( "Total Remote Time:    %s\n",
		format_time((float)proc->remote_usage.ru_utime.tv_sec +
							proc->remote_usage.ru_stime.tv_sec) );
}

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
	proc->exit_status = (int *) MALLOC( sizeof(union wait) * n_cmds );
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
