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

#define _POSIX_SOURCE
#include "condor_common.h"

#if !defined(WIN32)
#include <netdb.h>
#include <pwd.h>

#if defined(IRIX331)
#define __EXTENSIONS__
#include <signal.h>
#define BADSIG SIG_ERR
#undef __EXTENSIONS__
#else
#include <signal.h>
#endif

#include <sys/socket.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <netinet/in.h>

#include "condor_fix_wait.h"
#include "condor_fdset.h"
#endif // !defined WIN32

#if defined(HPUX9)
#include "fake_flock.h"
#endif

#if defined(Solaris)
#include "fake_flock.h"
#endif

#include "debug.h"
#include "trace.h"
#include "except.h"
#include "sched.h"
#include "clib.h"
#include "exit.h"


#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_qmgr.h"
#include "scheduler.h"
#include "condor_adtypes.h"
#include "condor_uid.h"
#include "condor_config.h"

#if defined(BSD43) || defined(DYNIX)
#	define WEXITSTATUS(x) ((x).w_retcode)
#	define WTERMSIG(x) ((x).w_termsig)
#endif

extern "C"
{
	void	dprintf_config(char*, int);
	void	dprintf(int, char*...);
	char*	param(char*);
	int		param_in_pattern(char*, char*);
	int		SetSyscalls() { return 0; }
	int		ReadLog(char*);
}
extern	void	mark_jobs_idle();

static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */

// global variables to control the daemon's running and logging
char*		Spool;							// spool directory
char*		mySubSystem = "SCHEDD";

// global objects
Scheduler	scheduler;
Scheduler*	sched = &scheduler;		// for non-member functions to access data
char		Name[MAXHOSTNAMELEN];
int			ScheddName = 0;

void Init();

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
 
	for(ptr = argv + 1; *ptr; ptr++)
	{
		if(ptr[0][0] != '-')
		{
			usage(argv[0]);
		}
		switch(ptr[0][1])
		{
		  case 'n':
			strcpy(Name, *(++ptr)); 
			ScheddName++;
			break;
		  default:
			usage(argv[0]);
		}
	}
	
	ClassAd *ScheddClassad = new ClassAd();
	config( ScheddClassad );

	Init();
	
	// if a name if not specified, assume the name of the machine
	if(!ScheddName)
	{
		gethostname(Name, MAXHOSTNAMELEN);
	}

	ScheddClassad->SetMyTypeName(SCHEDD_ADTYPE);
	ScheddClassad->SetTargetTypeName("");

	// throw the name into the machine ad
	{
		char	expr[200];

		sprintf(expr, "Name = \"%s\"", Name);
		ScheddClassad->Insert(expr);
	}

	sprintf(job_queue_name, "%s/job_queue.log", Spool);
	InitJobQueue(job_queue_name);
	mark_jobs_idle();

	// initialize all the modules
	scheduler.Init();
	scheduler.SetClassAd(ScheddClassad);
	scheduler.Register(daemonCore);
	scheduler.SetCommandPort(daemonCore->InfoCommandPort());
	
	scheduler.timeout();

	return 0;
} 

void Init()
{
	Spool = param("SPOOL");
	if(!Spool)
	{
		EXCEPT("No spool directory specified");
	}
}


int
main_config()
{
	scheduler.reconfig();
}


int
main_shutdown_fast()
{
	scheduler.shutdown_fast();
}


int
main_shutdown_graceful()
{
	scheduler.shutdown_graceful();
}

