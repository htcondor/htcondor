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
** Author:   Dhrubajyoti Borthakur
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 

#define _POSIX_SOURCE  // FIX

#if defined(IRIX62)
#include "condor_fdset.h"
#endif

#include "condor_common.h"

#if defined(Solaris251)
#define __EXTENSIONS__
#endif
#include <pwd.h>
#include <netdb.h>
#include <sys/stat.h>
#include <sys/file.h>
#include "condor_fix_timeval.h"  // FIX
#include <sys/resource.h>
#include <math.h>

#include "condor_constants.h"
#include "condor_debug.h"
#include "condor_expressions.h"
#include "condor_config.h"
#include "condor_uid.h"
#include "master.h"
#include "file_lock.h"

#if defined(HPUX9)
#include "fake_flock.h"
#define _BSD
#endif

static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */

typedef void (*SIGNAL_HANDLER)();
void install_sig_handler( int, SIGNAL_HANDLER );

// these are defined in master.C
extern int		NotFlag;
extern int		RestartsPerHour;
extern int 		MasterLockFD;
extern char *		_FileName_ ;
extern FileLock*	MasterLock;
extern int		doConfigFromServer; 
extern char*	config_location;

extern int		collector_runs_here();
extern int		negotiator_runs_here();
extern time_t	GetTimeStamp(char* file);
extern void		do_killpg(int, int);
extern void		do_kill( int pid, int sig );
extern int 	   	NewExecutable(char* file, time_t* tsp);
extern int		DoCleanup();
extern void		wait_all_children();

int		hourly_housekeeping(void);
extern "C"
{
#if	defined(SUNOS41) || defined(IRIX53)
	extern	int vfork();
#endif
	extern	int		event_mgr();
#if defined(HPUX9)
	extern	int		gethostname(char*, unsigned int);
#else
	extern	int		gethostname(char*, int);
#endif
	void	set_machine_status(int); 
}

// to add a new process as a condor daemon, just add one line in 
// the structure below. The first elemnt is the string that is 
// looked for in the config file for the executable, and the
// second element is the parameter looked for in the confit
// file for the name of the corresponding log file.If no log
// file need be there, then put a zero in the second column.
// The third parameter is the name of a condor_config variable
// that is ckecked before the process is created. If it is zero, 
// then the process is created always. If it is a valid name,
// this name shud be set to true in the condor_config file for the
// process to be created.

// make sure that the master does not start itself : set the flag off

// if this machine is a world-machine, then the standard schedd and
// startd are not started.

extern Daemons 		daemons;
extern int			ceiling;
extern float		e_factor;					// exponential factor
extern int			r_factor;					// recovering factor
extern TimerManager	tMgr;

daemon::daemon(char *name)
{
	char	buf[1000];

	name_in_config_file = strdup(name);
	daemon_name = strchr(name_in_config_file, '-');
	if(daemon_name)
	{
		daemon_name++;
		if(*daemon_name == '_')
		{
			daemon_name++;
			if(*daemon_name == '\0')
			{
				daemon_name = NULL;
			}
		}
		else
		{
			daemon_name = NULL;
		}
	} 
	
	sprintf(buf, "%s_LOG", name);
	log_filename_in_config_file = strdup(buf);
	sprintf(buf, "%s_FLAG", name);
	flag_in_config_file = param(buf);

	// Weiru
	// In the case that we have several for example schedds running on the
	// same machine, we specify SCHEDD__1, SCHEDD_2, ... in the config file.
	// We want to be able to specify only one executable for them as well as
	// different executables, or one flag for all of them as well as
	// different flags for each. So we instead of specifying in config file
	// SCHEDD__1 = /unsup/condor/bin/condor_schedd, SCHEDD__2 = /unsup/condor/
	// bin/condor_schedd, ... we can simply say SCHEDD = /unsup/...
	if(!flag_in_config_file && daemon_name)
	{
		*(daemon_name - 2) = '\0';
		sprintf(buf, "%s_FLAG", name_in_config_file);
		flag_in_config_file = param(buf);
		*(daemon_name - 2) = '_';
	} 
	process_name = 0;
	log_name = 0;
	if (strcmp(name, "MASTER") != MATCH) {
		flag = TRUE;
	} else {
		flag = FALSE;
	}
	runs_on_this_host();
	pid = 0;
	restarts = 0;
	newExec = FALSE; 
	timeStamp = 0;
	recoverTimer = -1;
	port = NULL;
	config_info_file = NULL;
	daemons.RegisterDaemon(this);
}


int
daemon::runs_on_this_host()
{
	char	*tmp;
	char	hostname[512];
	static char	**this_host_addr_list = 0;
	static int		host_addr_count;
	struct hostent	*hp;
	int		i, j;


	if ( flag_in_config_file != NULL ) {
		if (strncmp(flag_in_config_file, "BOOL_", 5) == MATCH) {
			tmp	= param( flag_in_config_file);
			if ( tmp && (*tmp == 't' || *tmp == 'T')) {
				flag = TRUE;
			} else {
				flag = FALSE;
			}
		} else {
			if (this_host_addr_list == 0) {
				if (gethostname(hostname, sizeof(hostname)) < 0) {
					EXCEPT( "gethostname(0x%x,%d)", hostname, 
						   sizeof(hostname));
				}
				if( (hp=gethostbyname(hostname)) == 0 ) {
					EXCEPT( "gethostbyname(%s)", hostname );
				}
				for (host_addr_count = 0; hp->h_addr_list[host_addr_count];
					 host_addr_count++ ) {
					/* Empty Loop */
				}
				this_host_addr_list = (char **) malloc(host_addr_count * 
													   sizeof (char *));
				for (i = 0; i < host_addr_count; i++) {
					this_host_addr_list[i] = (char *) malloc(hp->h_length);
					memcpy(this_host_addr_list[i], hp->h_addr_list[i],
						   hp->h_length);
				}
			}
			/* Get the name of the host on which this daemon should run */
			tmp = param( flag_in_config_file );
			if (!tmp) {
				dprintf(D_ALWAYS, "config file parameter %s not specified",
						flag_in_config_file);
				return FALSE;
			}
			flag = FALSE;
			hp = gethostbyname( tmp );
			if (hp == 0) {
				dprintf(D_ALWAYS, "Master couldn't lookup host %s\n", tmp);
				return FALSE;
			} 
			for (i = 0; i < host_addr_count; i++) {
				for (j = 0; hp->h_addr_list[j]; j++) {
					if (memcmp(this_host_addr_list[i], hp->h_addr_list[j],
							   hp->h_length) == MATCH) {
						flag = TRUE;
						break;
					}
				}
			}
		}
	}
	if(strcmp(name_in_config_file, "KBDD") == 0)
	// X_RUNS_HERE controls whether or not to run kbdd if it's presented in
	// the config file
	{
		tmp = param("X_RUNS_HERE");
		if(tmp)
		{
			if(*tmp == 'T' || *tmp == 't')
			{
				flag = TRUE;
			}
			else
			{
				flag = FALSE;
			}
		}
	}
	return flag;
}

Daemons::Daemons()
{
	daemon_ptr = 0;
	no_daemons  =  0;
	daemon_list_size = 0;
}


void
Daemons::RegisterDaemon(daemon *d)
{
	if (daemon_ptr == 0) {
		daemon_list_size = 10;
		daemon_ptr = (daemon **) malloc(daemon_list_size * sizeof(daemon *));
	}

	if (no_daemons >= daemon_list_size) {
		daemon_list_size *= 2;
		daemon_ptr = (daemon **) realloc(daemon_ptr, 
										daemon_list_size * sizeof(daemon *));
	}
	daemon_ptr[no_daemons] = d;
	no_daemons++;
}


void Daemons::InitParams()
{
	char	*tmp;

	for ( int i =0; i < no_daemons; i++) {
		daemon_ptr[i]->process_name = param(daemon_ptr[i]->name_in_config_file);
		if(daemon_ptr[i]->process_name == NULL && daemon_ptr[i]->daemon_name) {
			*(daemon_ptr[i]->daemon_name - 2) = '\0'; 
			daemon_ptr[i]->process_name = param(daemon_ptr[i]->name_in_config_file);
			*(daemon_ptr[i]->daemon_name - 2) = '_';
		}
		
		if ( daemon_ptr[i]->process_name == NULL && daemon_ptr[i]->flag ) {
			dprintf(D_ALWAYS, "Process not found in config file: %s\n", 
					daemon_ptr[i]->name_in_config_file);
			EXCEPT("Can't continue...");
		}

		// check that log file is necessary
		if ( daemon_ptr[i]->log_filename_in_config_file != NULL) {
			daemon_ptr[i]->log_name = 
					param(daemon_ptr[i]->log_filename_in_config_file);
			if ( daemon_ptr[i]->log_name == NULL && daemon_ptr[i]->flag ) {
				dprintf(D_ALWAYS, "Log file not found in config file: %s\n", 
						daemon_ptr[i]->log_filename_in_config_file);
#if 0
				{ EXCEPT("Log file name not found\n");}
#endif
			}
		}
	}


	// initialise master data structure
	int index = GetIndex("MASTER");
	if ( index == -1 ) {
		dprintf(D_ALWAYS, "InitParams:MASTER not specified\n");
		EXCEPT("InitParams:MASTER not Specifed\n");
	}
	daemon_ptr[index]->timeStamp = 
		GetTimeStamp(daemon_ptr[index]->process_name);
	daemon_ptr[index]->pid = getpid();

#if 0
	// if this is the world machine, then startd and startd are not run,
	// instead w-startd and w-schedd are run.
	index = GetIndex("W_SCHEDD");
	if ( index == -1 ) {
		dprintf(D_ALWAYS, " Know nothing about Condor Flock Configuration\n");
	} else {
		// if WORLD_RUNS_HERE has been set to TRUE
		if ( daemon_ptr[index]->flag == TRUE ) {
			index = GetIndex("SCHEDD");
			if ( index != -1 ) daemon_ptr[index]->flag = FALSE;
			index = GetIndex("STARTD");
			if ( index != -1 ) daemon_ptr[index]->flag = FALSE;
			index = GetIndex("KBDD");
			if ( index != -1 ) daemon_ptr[index]->flag = FALSE;
		}
	}
#endif
}

int Daemons::GetIndex(const char* name)
{
	for ( int i=0; i < no_daemons; i++)
		if ( strcmp(daemon_ptr[i]->name_in_config_file,name) == 0 )
			return i;	
	return -1;
}

void Daemons::StartAllDaemons()
{
	// initialise master data structure
	int index = GetIndex("MASTER");
	if ( index == -1 )
	{
		dprintf(D_ALWAYS, "StartAllDaemons :MASTER not specified\n");
		EXCEPT("StartAllDaemons :MASTER not Specifed\n");
	}
	daemon_ptr[index]->timeStamp = 
		GetTimeStamp(daemon_ptr[index]->process_name);
	daemon_ptr[index]->pid       = getpid();

	for ( int i=0; i < no_daemons; i++) {
		// create only those processes that exists in this machine
		if(daemon_ptr[i]->pid != 0)
		// the daemon is already started
		{
			continue;
		} 
		if ( daemon_ptr[i]->flag == FALSE ) continue;
		daemon_ptr[i]->StartDaemon();
		daemon_ptr[i]->timeStamp = GetTimeStamp(daemon_ptr[i]->process_name);
	}
}

int daemon::Restart()
{
	int		n;
	int		timer;

	if( NotFlag ) {
		dprintf( D_ALWAYS, "NOT Starting \"%s\"\n", process_name );
		return 0;
	}
	if(newExec == TRUE)
	{
		restarts = 0;
		newExec = FALSE; 
		return StartDaemon();
	}
	n = 9 + (int)ceil(pow(e_factor, restarts));
	if(n > ceiling)
	{
		n = ceiling;
	}
	restarts++;
	if(recoverTimer != -1)
	{
		dprintf(D_ALWAYS, "Cancelling recovering timer (%d) for %s\n", recoverTimer, process_name);
		tMgr.CancelTimer(recoverTimer);
		recoverTimer = -1;
	}
	timer = tMgr.NewTimer(this, n, (void*)StartDaemon);
	dprintf(D_ALWAYS, "restarting %s in %d seconds\n", process_name, n);
	dprintf(D_FULLDEBUG, "Added timer (%d) for restarting %s\n", timer,
		process_name);
	return 1;
}

int daemon::StartDaemon()
{
	char	*shortname;
	int		n;

	if( shortname = strrchr(process_name,'/') ) {
		shortname += 1;
	} else {
		shortname = process_name;
	}

	if( access(process_name,X_OK) != 0 ) {
		dprintf(D_ALWAYS, "%s: Cannot execute\n", process_name );
		// flag = FALSE;
		pid = -1; 
		return 0;
	}

	if( (pid = vfork()) < 0 ) {
		EXCEPT( "vfork()" );
	}

	if( pid == 0 ) {	/* The child */
		pid = getpid();
		if( setsid() == -1 ) {
			EXCEPT( "setsid(), errno = %d\n", errno );
		}

		if(strcmp(name_in_config_file, "CONFIG_SERVER") == 0)
		{
			if(port && config_info_file)
			{
				(void)execl( process_name, shortname, "-f", "-l", log_name, "-p", port, "-c", config_info_file, 0 );
			}
			else if(port)
			{
				(void)execl( process_name, shortname, "-f", "-l", log_name, "-p", port, 0 );
			}
			else if(config_info_file)
			{
				(void)execl( process_name, shortname, "-f", "-l", log_name, "-c", config_info_file, 0 );
			}
			else
			{
				(void)execl( process_name, shortname, "-f", "-l", log_name, 0 );
			}
		}
		else
		{
			if(doConfigFromServer && config_location)
			{
				if(daemon_name)
				{
					(void)execl( process_name, shortname, "-f", "-n", daemon_name, "-c", config_location, 0 );
				}
				else
				{
					(void)execl( process_name, shortname, "-f", "-c", config_location, 0 );
				} 
			} 
			else
			{
				if(daemon_name)
				{
					(void)execl( process_name, shortname, "-f", "-n", daemon_name, 0); 
				}
				else
				{
					(void)execl( process_name, shortname, "-f", 0 );
				} 
			} 
		}
		
		EXCEPT( "execl( %s, %s, -f, 0 )", process_name, shortname );
		return 0;
	} else { 			/* The parent */
		// if this is a restart, start recover process
		if(restarts > 0)
		{
			recoverTimer = tMgr.NewTimer(this, r_factor, (void*)Recover);
			dprintf(D_FULLDEBUG, "start recover timer (%d)\n", recoverTimer);
		}

		dprintf( D_ALWAYS, "Started \"%s\", pid and pgroup = %d\n",
			shortname, pid );
		return pid;
	}
}

void Daemons::Restart(int pid)
{
	// find out which daemon died
	for ( int i=0; i < no_daemons; i++) {
		if (( pid == daemon_ptr[i]->pid)&&(daemon_ptr[i]->flag == TRUE)) {
			// this is the daemon that died: schedule to restart it
			dprintf( D_ALWAYS, "The %s (process %d ) died\n",
						daemon_ptr[i]->name_in_config_file,pid);
			daemon_ptr[i]->pid = 0; 
			do_killpg( pid, SIGKILL ) ;
			// This is ok, since we're only calling Restart() when a
			// daemon has already exited.  So, killing the process
			// group with SIGKILL shouldn't do any harm and will clean
			// up any dead/hung processes that are children of the
			// daemon that died.  -Derek Wright 7/29/97

			// restart
			daemon_ptr[i]->Restart();
			return;
		}
	}
	dprintf( D_ALWAYS, "Child %d died, but not a daemon -- Ignored\n", pid);
}


void Daemons::RestartMaster()
{
	int			pid;
	int			index = GetIndex("MASTER");

	if ( index == -1 ) {
		dprintf(D_ALWAYS, "Restart Master:MASTER not specified\n");
		EXCEPT("Restart Master:MASTER not Specifed\n");
	}

	dprintf(D_ALWAYS, "RESTARTING MASTER (new executable)\n");
	install_sig_handler( SIGCHLD, (SIGNAL_HANDLER)SIG_IGN);

		/* Send SIGTERM to all daemons (NOT process groups) and let
		   them do their own clean up.  -Derek Wright 7/29/97 */
	for ( int i=0; i < no_daemons; i++) {
		if ((daemon_ptr[i]->flag == TRUE )&& ( daemon_ptr[i]->pid )
				&& ( index != i)) {
			do_kill(daemon_ptr[i]->pid, SIGTERM);
			dprintf(D_ALWAYS, "Killed %s pid = %d\n",
				daemon_ptr[i]->name_in_config_file, daemon_ptr[i]->pid);
		}
	}

		/* Wait until all children die */
	wait_all_children();

	if ( MasterLock->release() == FALSE ) {
		dprintf(D_ALWAYS,
				"Can't remove lock on \"%s\"\n",daemon_ptr[index]->log_name);
		EXCEPT( "file_lock(%d)", MasterLockFD );
	}
	dprintf( D_ALWAYS, "Unlocked file descriptor %d\n", MasterLockFD );
	(void)close( MasterLockFD );
	dprintf( D_ALWAYS, "Closed file descriptor %d\n", MasterLockFD );

	dprintf( D_ALWAYS, "Doing exec( \"%s\", \"condor_master\", 0 )", 
			daemon_ptr[index]->process_name);
	(void)execl(daemon_ptr[index]->process_name, "condor_master", 0);
	EXCEPT("execl(%s, condor_master, 0)",daemon_ptr[index]->process_name);
}

void Daemons::CheckForNewExecutable()
{
    int master = GetIndex("MASTER");

	dprintf(D_FULLDEBUG, "enter Daemons::CheckForNewExecutable\n");
    if ( master == -1 ) {
        dprintf(D_ALWAYS, "SigalrmHandler:MASTER not specified\n");
        EXCEPT("SigalrmHandler:MASTER not Specifed\n");
    }
    if( NewExecutable(daemon_ptr[master]->process_name,
                                    &daemon_ptr[master]->timeStamp) ) {
        dprintf(D_ALWAYS, " Restarting master\n");
        RestartMaster();
    }

    for (int i=0; i < no_daemons; i++) {
        if ( i== master ) continue;
		if (( daemon_ptr[i]->flag == TRUE ) && ( daemon_ptr[i]->pid )
				&& NewExecutable(daemon_ptr[i]->process_name,
									&daemon_ptr[i]->timeStamp)) {
			daemon_ptr[i]->restarts = 0;
			daemon_ptr[i]->newExec = TRUE;
			if(daemon_ptr[i]->pid > 0) {
				dprintf(D_ALWAYS,"%s was modified.  Killing %d\n", 
						daemon_ptr[i]->name_in_config_file, daemon_ptr[i]->pid);
				do_kill( daemon_ptr[i]->pid, SIGTERM );
			} else {
				daemon_ptr[i]->StartDaemon(); 
			}
		} 
	}

	dprintf(D_FULLDEBUG, "exit Daemons::CheckForNewExecutable\n");
}


char* Daemons::DaemonLog( int pid )
{
	// be careful : a pointer to data in this class is returned
	// posibility of getting tampered

	for ( int i=0; i < no_daemons; i++)
		if ( daemon_ptr[i]->pid == pid )
			return (daemon_ptr[i]->log_name);
	return "Unknown Program!!!";
}

char* Daemons::DaemonName( int pid )
{
	// be careful : a pointer to data in this class is returned
	// posibility of getting tampered

	for ( int i=0; i < no_daemons; i++)
		if ( daemon_ptr[i]->pid == pid )
			return (daemon_ptr[i]->process_name);
	return "Unknown Program!!!";
}

char* Daemons::SymbolicName( int pid )
{
	// be careful : a pointer to data in this class is returned
	// posibility of getting tampered

	for ( int i=0; i < no_daemons; i++)
		if ( daemon_ptr[i]->pid == pid )
			return (daemon_ptr[i]->name_in_config_file);
	return "Unknown Program!!!";
}

int Daemons::NumberRestarts(int pid)
{
	for ( int i=0; i < no_daemons; i++)
		if ( daemon_ptr[i]->pid == pid )
			return (daemon_ptr[i]->restarts);
	return -1;
}


void Daemons::SignalAll(int signal)
{
	// kills all daemons except the master itself
	for ( int i=0; i < no_daemons; i++)
		if ((daemon_ptr[i]->flag == TRUE ) && ( daemon_ptr[i]->pid )
		  && ( strcmp(daemon_ptr[i]->name_in_config_file,"MASTER") != 0))
			do_kill(daemon_ptr[i]->pid, signal);
}


int Daemons::IsDaemon(int pid)
{
	for ( int i=0; i < no_daemons; i++)
		if ( daemon_ptr[i]->pid == pid )
			return 1;
	return 0;
}
	
void daemon::Recover()
{
	restarts = 0;
	recoverTimer = -1; 
	dprintf(D_FULLDEBUG, "%s recovered\n", name_in_config_file);
}
