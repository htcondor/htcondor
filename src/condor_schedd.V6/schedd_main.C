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
** Modified by Cai, Weiru
**			User condor_daemon_core 
*/

#define _POSIX_SOURCE
#include "condor_common.h"

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

#include "condor_fix_wait.h"

#if defined(HPUX9)
#include "fake_flock.h"
#endif

#if defined(Solaris)
#include "fake_flock.h"
#endif

#include <sys/stat.h>
#include <netinet/in.h>
#include "debug.h"
#include "trace.h"
#include "except.h"
#include "sched.h"
#include "clib.h"
#include "exit.h"
#include "dgram_io_handle.h" /* defines DRAM_IO_HANDLE */

#include "condor_fdset.h"

#include "condor_daemon_core.h"
#include "condor_qmgr.h"
#include "scheduler.h"

#if defined(BSD43) || defined(DYNIX)
#	define WEXITSTATUS(x) ((x).w_retcode)
#	define WTERMSIG(x) ((x).w_termsig)
#endif

extern "C"
{
	void	_EXCEPT_(char*...);
	void	dprintf_config(char*, int);
	void	dprintf(int, char*...);
	int		set_condor_euid();
	void	config(char*, CONTEXT*);
	char*	param(char*);
	int		boolean(char*, char*);
	int		SetSyscalls() {}
	int		ReadLog(char*);
}
extern	void	mark_jobs_idle();

static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */

// global variables to control the daemon's running and logging
char*		Log;							// log directory
char*		Spool;							// spool directory
int			Foreground = 0;
extern int	Termlog;

// global objects
Scheduler	scheduler;
DaemonCore	core(10, 10, 10);
char*		myName;
Scheduler*	sched = &scheduler;		// for non-member functions to access data
CONTEXT*	MachineContext;

void	Init();

void usage(char* name)
{
	dprintf( D_ALWAYS, "Usage: %s [-f] [-t]\n", name );
	exit( 1 );
}

main(int argc, char* argv[])
{
	char**		ptr; 
	ClassAd*	myAd; 
	char		job_queue_name[_POSIX_PATH_MAX];
	char		Name[MAXHOSTNAMELEN];
	int			ScheddName = 0;
	struct		utsname	name;
#ifdef WAIT_FOR_DEBUG
	int			i;
#endif

	myName = argv[0];
	if(getuid() == 0)
	{
	#ifdef NFSFIX
		// Must be condor to write to log files.
		set_condor_euid(__FILE__,__LINE__);
	#endif NFSFIX
	}

	MachineContext = create_context();
	config(argv[0], MachineContext);
#ifdef WAIT_FOR_DEBUG
	i = 1;
	while(i);
#endif
	myAd = new ClassAd(MachineContext); 
	Init();
	
	if(argc > 3)
	{
		usage(argv[0]);
	}
	for(ptr = argv + 1; *ptr; ptr++)
	{
		if(ptr[0][0] != '-')
		{
			usage(argv[0]);
		}
		switch(ptr[0][1])
		{
		  case 'f':
			Foreground = 1;
			break;
		  case 't':
			Termlog = 1;
			break;
		  case 'b':
			Foreground = 0;
			break;
		  case 'n':
			strcpy(Name, *(++ptr));
			++ptr;
			ScheddName++;
			break;
		  default:
			usage(argv[0]);
		}
	}

	// if a name if not specified, assume the name of the machine
	if(!ScheddName)
	{
		uname(&name);
		strcpy(Name, name.nodename);
	}

	// throw the name into the machine ad
	{
		char	expr[200];

		sprintf(expr, "Name = %s", Name);
		myAd->Insert(expr);
	}

    {
        EXPR *name_expr;
        char NameExpr [MAXHOSTNAMELEN + 10];
        sprintf (NameExpr, "Name = \"%s\"", Name);
        name_expr = scan (NameExpr);
        if (name_expr == NULL)
        {
                EXCEPT ("Could not scan expression: [%s]", NameExpr);
        }
        store_stmt (name_expr, MachineContext);
    }

	// This is so if we dump core it'll go in the log directory
	if(chdir(Log) < 0)
	{
		EXCEPT("chdir to log directory <%s>", Log);
	}
	
	// Arrange to run in background
	if(!Foreground)
	{
		if(fork())
			exit(0);
	}
	
	// Set up logging
	dprintf_config("SCHEDD", 2);
	dprintf(D_ALWAYS, "**************************************************\n");
	dprintf(D_ALWAYS, "***		  CONDOR_SCHEDD.V6 STARTING UP	      ***\n");
	dprintf(D_ALWAYS, "**************************************************\n");
	dprintf(D_ALWAYS, "\n");

	sprintf(job_queue_name, "%s/job_queue.log", Spool);
	ReadLog(job_queue_name);
	mark_jobs_idle();

	// initialize all the modules
	scheduler.Init();
	scheduler.SetClassAd(myAd);
	scheduler.Register(&core);
	core.OpenTcp(argv[0], SCHED_PORT);

	scheduler.timeout();						// UPDOWN
	
	core.Driver();
} 
	
void Init()
{
	char*	tmp;

    Log = param( "LOG" );
    if( Log == NULL )  {
        EXCEPT( "No log directory specified in config file\n" );
    }
	if( boolean("SCHEDD_DEBUG","Foreground") ) {
		Foreground = 1;
	}

	Spool = param("SPOOL");
	if(!Spool)
	{
		EXCEPT("No spool directory specified");
	}
}
