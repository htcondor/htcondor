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

 


#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "proc.h"

#if defined(AIX31) || defined(AIX32)
#include <time.h>
#endif


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
	src = proc->cmd;
	dst = cmd;
	while( len && *src ) {
		*dst++ = *src++;
		len -= 1;
	}
	if( len > 0 ) {
		*dst++ = ' ';
		len -= 1;
	}
	src = proc->args;
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
		format_time((float)proc->remote_usage.ru_utime.tv_sec),
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
	time_t arch_time = 0; /* 8 byte time_t versus 4 byte int for ctime() */
	(void)putchar( '\n' );
	printf( "Structure Version: %d\n", proc->version_num );
	printf( "Id: %d.%d\n", proc->id.cluster, proc->id.proc );
	printf( "Owner: %s\n", proc->owner );
	arch_time = proc->q_date;
	printf( "Queue Date: %s", ctime( &arch_time ) );

	if( proc->status == COMPLETED ) {
		if( proc->completion_date == 0 ) {
			printf( "Completion Date: (not recorded)\n" );
		} else {
			arch_time = proc->completion_date;
			printf( "Completion Date: %s", ctime(&arch_time));
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
