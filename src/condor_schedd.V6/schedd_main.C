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
** Modified by : Dhaval N. Shah
**               University of Wisconsin, Computer Sciences Dept.
** Uses <IP:PORT> rather than hostnames
**
*/

#include "condor_common.h"
#include "condor_debug.h"
static char *_FileName_ = __FILE__;		/* Used by EXCEPT */
#include "condor_config.h"
#include "condor_attributes.h"
#include "my_hostname.h"

#include "sched.h"
#include "clib.h"
#include "exit.h"

#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_qmgr.h"
#include "scheduler.h"
#include "condor_adtypes.h"
#include "condor_uid.h"

#if defined(BSD43) || defined(DYNIX)
#	define WEXITSTATUS(x) ((x).w_retcode)
#	define WTERMSIG(x) ((x).w_termsig)
#endif

extern "C"
{
	int		ReadLog(char*);
}
extern	void	mark_jobs_idle();


// global variables to control the daemon's running and logging
char*		Spool = NULL;							// spool directory
char* 		JobHistoryFileName = NULL;
char*		Name = NULL;
char*		mySubSystem = "SCHEDD";

// global objects
Scheduler	scheduler;

void usage(char* name)
{
	dprintf( D_ALWAYS, "Usage: %s [-f] [-t] [-n schedd_name]", name); 
	exit( 1 );
}

int
main_init(int argc, char* argv[])
{
	char**		ptr; 
	char		job_queue_name[_POSIX_PATH_MAX];
	char	expr[200];
 
	for(ptr = argv + 1; *ptr; ptr++)
	{
		if(ptr[0][0] != '-')
		{
			usage(argv[0]);
		}
		switch(ptr[0][1])
		{
		  case 'n':
			Name = strdup(*(++ptr)); 
			break;
		  default:
			usage(argv[0]);
		}
	}
	
	// if a name if not specified, use the full hostname of the machine
	if(!Name) {
		Name = strdup( my_full_hostname() );
	}
	
		// Initialize all the modules
	scheduler.Init();
	scheduler.Register();

		// Initialize the job queue
	sprintf(job_queue_name, "%s/job_queue.log", Spool);
	InitJobQueue(job_queue_name);
	mark_jobs_idle();

	scheduler.timeout();

	return 0;
} 

int
main_config()
{
	scheduler.reconfig();
	return 0;
}


int
main_shutdown_fast()
{
	scheduler.shutdown_fast();
	return 0;
}


int
main_shutdown_graceful()
{
	scheduler.shutdown_graceful();
	return 0;
}

