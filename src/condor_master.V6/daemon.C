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
** Edited further by : Dhaval N Shah
** 	         University of Wisconsin, Computer Sciences Dept.
** Modified by: Cai, Weiru
** 			 University of Wisconsin, Computer Sciences Dept.
** Major clean-up, re-write by Derek Wright (7/10/97)
** 			 University of Wisconsin, Computer Sciences Dept.
** Another major clean-up.  Nearly a total re-write by Derek (1/14-15/98)
*/ 

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_uid.h"
#include "master.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "exit.h"
#include "my_hostname.h"

static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */

// these are defined in master.C
extern int		RestartsPerHour;
extern int 		MasterLockFD;
extern FileLock*	MasterLock;
extern int		master_exit(int);
extern int		update_interval;
extern int		check_new_exec_interval;
extern int		preen_interval;
extern int		new_bin_delay;
extern char*	FS_Preen;


#if 0
extern int		doConfigFromServer; 
extern char*	config_location;
#endif

extern time_t	GetTimeStamp(char* file);
extern int 	   	NewExecutable(char* file, time_t tsp);
extern void		tail_log( FILE*, char*, int );
extern void		update_collector();
extern int		run_preen(Service*);

#ifndef WANT_DC_PM
extern int standard_sigchld(Service *,int);
extern int all_reaper_sigchld(Service *,int);
#endif

int		hourly_housekeeping(void);

extern "C"
{
	void killkids( pid_t, int );
}

extern "C" 	char	*SigNames[];

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

// make sure that the master does not start itself : set runs_here off

extern Daemons 		daemons;
extern int			ceiling;
extern float		e_factor;					// exponential factor
extern int			r_factor;					// recovering factor
extern int			shutdown_graceful_timeout;
extern int			shutdown_fast_timeout;
extern char*		CondorAdministrator;
extern char*		MailerPgm;
extern int			Lines;
extern int			PublishObituaries;
extern int			StartDaemons;
extern char*		MasterName;

///////////////////////////////////////////////////////////////////////////
// daemon Class
///////////////////////////////////////////////////////////////////////////
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
	process_name = NULL;
	log_name = NULL;
	if( strcmp(name, "MASTER") == MATCH ) {
		runs_here = FALSE;
	} else {
		runs_here = TRUE;
	}
	runs_on_this_host();
	pid = 0;
	restarts = 0;
	newExec = FALSE; 
	timeStamp = 0;
	start_tid = -1;
	recover_tid = -1;
	stop_tid = -1;
	stop_fast_tid = -1;
 	hard_kill_tid = -1;
	stop_state = NONE;
#if 0
	port = NULL;
	config_info_file = NULL;
#endif
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
				runs_here = TRUE;
			} else {
				runs_here = FALSE;
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
			runs_here = FALSE;
			hp = gethostbyname( tmp );
			if (hp == 0) {
				dprintf(D_ALWAYS, "Master couldn't lookup host %s\n", tmp);
				return FALSE;
			} 
			for (i = 0; i < host_addr_count; i++) {
				for (j = 0; hp->h_addr_list[j]; j++) {
					if (memcmp(this_host_addr_list[i], hp->h_addr_list[j],
							   hp->h_length) == MATCH) {
						runs_here = TRUE;
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
				runs_here = TRUE;
			}
			else
			{
				runs_here = FALSE;
			}
		}
	}
	return runs_here;
}


void
daemon::Recover()
{
	restarts = 0;
	recover_tid = -1; 
	dprintf(D_FULLDEBUG, "%s recovered\n", name_in_config_file);
}


int
daemon::NextStart()
{
	int n;
	n = 9 + (int)ceil(pow(e_factor, restarts));
	if( n > ceiling ) {
		n = ceiling;
	}
	return n;
}


int daemon::Restart()
{
	int		n;

	if(newExec == TRUE) {
		restarts = 0;
		newExec = FALSE; 
		n = new_bin_delay;
	} else {
		n = NextStart();
		restarts++;
	}

	if( recover_tid != -1 ) {
		dprintf( D_FULLDEBUG, 
				 "Cancelling recovering timer (%d) for %s\n", 
				 recover_tid, process_name );
		daemonCore->Cancel_Timer( recover_tid );
		recover_tid = -1;
	}
	if( start_tid != -1 ) {
		daemonCore->Cancel_Timer( start_tid );
	}
	start_tid = daemonCore->
		Register_Timer( n, (TimerHandlercpp)daemon::DoStart,
						"daemon::DoStart()", this );
	dprintf(D_ALWAYS, "restarting %s in %d seconds\n", process_name, n);
	return 1;
}


// This little function is used so when the start timer goes off, we
// can set the start_tid back to -1 before we actually call Start().
void
daemon::DoStart()
{
	start_tid = -1;
	Start();
}


int
daemon::Start()
{
	char	*shortname;
	int command_port = TRUE;
#ifndef WANT_DC_PM
	int i, max_fds = getdtablesize();
#endif
	char argbuf[150];

	if( start_tid != -1 ) {
		daemonCore->Cancel_Timer( start_tid );
		start_tid = -1;
	}

	if( (shortname = strrchr(process_name,'/')) ) {
		shortname += 1;
	} else {
#ifdef WIN32
		// this is icky.  yuck.  nasty.  crappy.  can't everyone just get along?
		if ( shortname=strrchr(process_name,'\\') )
			shortname += 1;
		else
#endif
			shortname = process_name;
	}

	if( access(process_name,X_OK) != 0 ) {
		dprintf(D_ALWAYS, "%s: Cannot execute\n", process_name );
		pid = 0; 
		// Schedule to try and restart it a little later
		Restart();
		return 0;
	}

		// Collector needs to listen on a well known port, as does the
		// Negotiator.
	if ( strcmp(name_in_config_file,"COLLECTOR") == 0 ) {
		command_port = COLLECTOR_PORT;
	}
	if ( strcmp(name_in_config_file,"NEGOTIATOR") == 0 ) {
		command_port = NEGOTIATOR_PORT;
	}

#ifdef WANT_DC_PM

	if( (strcmp(name_in_config_file,"SCHEDD") == 0) && MasterName ) {
		sprintf( argbuf, "%s -f -n %s\0", shortname, MasterName );
	} else {
		sprintf( argbuf, "%s -f\0", shortname );
	}
	pid = daemonCore->Create_Process(
				process_name,	// program to exec
				argbuf,			// args
				PRIV_ROOT,		// privledge level
				1,				// which reaper ID to use; use default reaper
				command_port,	// port to use for command port; TRUE=choose one dynamically
				NULL,			// environment
				NULL,			// current working directory
				TRUE);			// new_process_group flag; we want a new group

	if ( pid == FALSE ) {
			// Create_Process failed!
		dprintf( D_ALWAYS,
				 "ERROR: Create_Process failed, trying to start %s\n",
				 process_name);
		pid = 0;
		return 0;
	}

#else
	if( (pid = vfork()) < 0 ) {
		EXCEPT( "vfork()" );
	}

	if( pid == 0 ) {	/* The child */
		pid = ::getpid();
		if( setsid() == -1 ) {
			dprintf( D_ALWAYS, "ERROR: setsid() failed, errno = %d\n", errno );
			exit( JOB_EXEC_FAILED );
		}

		// Close all inherited sockets and fds
		for (i=0; i<max_fds; i++)
			close(i);

		if( command_port != TRUE ) {
			sprintf( argbuf, "%d", command_port );
			(void)execl( process_name, shortname, "-f", "-p",
						 argbuf, 0 );
		} else {
			if( (strcmp(name_in_config_file,"SCHEDD") == 0) && MasterName ) {
				(void)execl( process_name, shortname, "-f", "-n", MasterName, 0 );
			} else {
				(void)execl( process_name, shortname, "-f", 0 );
			}
		}

		dprintf( D_ALWAYS, "ERROR: execl( %s, %s, -f, 0 ) failed, errno = %d\n",
				 process_name, shortname, errno );
		exit( JOB_EXEC_FAILED );
	}
#endif
	
	// if this is a restart, start recover timer
	if (restarts > 0) {
		recover_tid = daemonCore->Register_Timer(r_factor,(TimerHandlercpp)daemon::Recover,
			"daemon::Recover()",this);
		dprintf(D_FULLDEBUG, "start recover timer (%d)\n", recover_tid);
	}

	dprintf( D_ALWAYS, "Started \"%s\", pid and pgroup = %d\n",
		process_name, pid );

	// update the timestamp so we do not restart accidently a second time
	timeStamp = GetTimeStamp(process_name);

	return pid;	
}


void
daemon::Stop() 
{
	if( !strcmp(name_in_config_file, "MASTER") ) {
			// Never want to stop master.
		return;
	}

	if( stop_state == GRACEFUL ) {
			// We've already been here, just return.
		return;
	}
	stop_state = GRACEFUL;

#ifdef WANT_DC_PM
	Kill( DC_SIGTERM );
#else
	Kill( SIGTERM );
#endif

	stop_fast_tid = 
		daemonCore->Register_Timer( shutdown_graceful_timeout, 0, 
									(TimerHandlercpp)daemon::StopFast,
									"daemon::StopFast()", this );
}


void
daemon::StopFast()
{
	if( !strcmp(name_in_config_file, "MASTER") ) {
			// Never want to stop master.
		return;
	}

	if( stop_state == FAST ) {
			// We've already been here, just return.
		return;
	}
	stop_state = FAST;

	if( stop_fast_tid != -1 ) {
		dprintf( D_ALWAYS, 
				 "Timeout for graceful shutdown has expired for %s.\n", 
				 name_in_config_file );
		stop_fast_tid = -1;
	}

#ifdef WANT_DC_PM
	Kill( DC_SIGQUIT );
#else
	Kill( SIGQUIT );
#endif

	hard_kill_tid = 
		daemonCore->Register_Timer( shutdown_fast_timeout, 0, 
									(TimerHandlercpp)daemon::HardKill,
									"daemon::HardKill()", this );
}

void
daemon::HardKill()
{
	if( !strcmp(name_in_config_file, "MASTER") ) {
			// Never want to stop master.
		return;
	}

	if( stop_state == KILL ) {
			// We've already been here, just return.
		return;
	}
	stop_state = KILL;

	if( hard_kill_tid != -1 ) {
		dprintf( D_ALWAYS, 
				 "Timeout for fast shutdown has expired for %s.\n", 
				 name_in_config_file );
		hard_kill_tid = -1;
	}

#ifdef WANT_DC_PM
	Kill( DC_SIGKILL );
		// Can we do better than that for DC_PM?
#else
	killkids( pid, SIGKILL );
	Killpg( SIGKILL );
	dprintf( D_ALWAYS, 
			 "Sent SIGKILL to %s (pid %d) and all it's children.\n",
			 name_in_config_file, pid );
#endif
}


void
daemon::Exited( int status )
{
	char buf1[256];
	char buf2[256];
	sprintf( buf1, "The %s (pid %d) ", name_in_config_file, pid );
	if( WIFSIGNALED(status) ) {
		sprintf( buf2, "died with signal %d", WTERMSIG(status) );
	} else {
		sprintf( buf2, "exited with status %d", WEXITSTATUS(status) );
	}
	dprintf( D_ALWAYS, "%s%s\n", buf1, buf2 );

		// For good measure, try to clean up any dead/hung children of
		// The daemon that just died by sending SIGKILL to it's
		// process group.
#ifdef WANT_DC_PM
	Killpg( DC_SIGKILL );	
#else
	Killpg( SIGKILL ) ;	
#endif
		// Mark this daemon as gone.
	pid = 0;
	CancelAllTimers();
	stop_state = NONE;
}


void
daemon::Obituary( int status )
{
#if !defined(WIN32)		// until we add email support to WIN32 port...
    char    cmd[512];
    FILE    *mailer;

	/* If daemon with a serious bug gets installed, we may end up
	 ** doing many restarts in rapid succession.  In that case, we
	 ** don't want to send repeated mail to the CONDOR administrator.
	 ** This could overwhelm the administrator's machine.
	 */
    if ( restarts > 3 ) return;

    // always return for KBDD
    if( strcmp( name_in_config_file, "KBDD") == 0 ) {
        return;
	}

	// Just return if process was killed with SIGKILL.  This means the
	// admin did it, and thus no need to send email informing the
	// admin about something they did...
	if ( (WIFSIGNALED(status)) && (WTERMSIG(status) == SIGKILL) ) {
		return;
	}

	// Just return if process exited with status 0.  If everthing's
	// ok, why bother sending email?
	if ( (WIFEXITED(status)) && 
		 ( (WEXITSTATUS(status) == 0 ) || 
		   (WEXITSTATUS(status) == JOB_EXEC_FAILED ) ) ) {
		return;
	}

    dprintf( D_ALWAYS, "Sending obituary for \"%s\" to \"%s\"\n",
			process_name, CondorAdministrator );

	(void)sprintf( cmd, "%s -s \"%s\" %s", MailerPgm, 
				   "CONDOR Problem",
				   CondorAdministrator );

    if( (mailer=popen(cmd,"w")) == NULL ) {
        EXCEPT( "popen(\"%s\",\"w\")", cmd );
    }

    fprintf( mailer, "\n" );

    if( WIFSIGNALED(status) ) {
        fprintf( mailer, "\"%s\" on \"%s\" died due to signal %d\n",
				process_name, my_full_hostname(), WTERMSIG(status) );
        /*
		   fprintf( mailer, "(%s core was produced)\n",
		   status->w_coredump ? "a" : "no" );
		   */
    } else {
        fprintf( mailer,
				"\"%s\" on \"%s\" exited with status %d\n",
				process_name, my_full_hostname(), WEXITSTATUS(status) );
    }
    tail_log( mailer, log_name, Lines );

	/* Don't do a pclose here, it wait()'s, and may steal an
	 ** exit notification of one of our daemons.  Instead we'll clean
	 ** up popen's child in our SIGCHLD handler.
	 */
#if defined(HPUX9)
    /* on HPUX, however, do a pclose().  This is because pclose() on HPUX
	 ** will _not_ steal an exit notification, and just doing an fclose
	 ** can cause problems the next time we try HPUX's popen(). -Todd */
    pclose(mailer);
#else
    (void)fclose( mailer );
#endif

#endif	// of !defined(WIN32)
}


void
daemon::CancelAllTimers()
{
	if( stop_tid  != -1 ) {
		daemonCore->Cancel_Timer( stop_tid );
		stop_tid = -1;
	}
	if( stop_fast_tid  != -1 ) {
		daemonCore->Cancel_Timer( stop_fast_tid );
		stop_fast_tid = -1;
	}
	if( hard_kill_tid  != -1 ) {
		daemonCore->Cancel_Timer( hard_kill_tid );
		hard_kill_tid = -1;
	}
	if( recover_tid  != -1 ) {
		daemonCore->Cancel_Timer( recover_tid );
		recover_tid = -1;
	}
	if( start_tid != -1 ) {
		daemonCore->Cancel_Timer( start_tid );
		start_tid = -1;
	}
}


void
daemon::Killpg( int sig )
{
	if( (!pid) || (pid == 1) ) {
		return;
	}
#ifdef WANT_DC_PM
	dprintf( D_ALWAYS, "Killing process groups not supported with DC_PM.\n" );
	return;
#else
	priv_state priv = set_root_priv();
	(void)kill(-pid, sig );
	set_priv(priv);
#endif
	
}


void
daemon::Kill( int sig )
{
	if( (!pid) || (pid == -1) ) {
		return;
	}

	int status;

#ifdef WANT_DC_PM
	status = daemonCore->Send_Signal(pid,sig);
	if ( status == FALSE )
		status = -1;
	else
		status = 1;
#else
	priv_state priv = set_root_priv();
	status = kill( pid, sig );
	set_priv(priv);
#endif

	if( status < 0 ) {
		dprintf( D_ALWAYS, "ERROR: failed to send %s to pid %d\n",
				 SigNames[sig], pid );
	} else {
		dprintf( D_ALWAYS, "Sent %s to %s (pid %d)\n",
				 SigNames[sig], name_in_config_file, pid );
	}
}


void
daemon::Reconfig()
{
	if( stop_state != NONE ) {
			// If we're currently trying to shutdown this daemon,
			// there's no need to reconfig it.
		return;
	}
#ifdef WANT_DC_PM
	Kill( DC_SIGHUP );
#else
	Kill( SIGHUP );
#endif
}


///////////////////////////////////////////////////////////////////////////
//  Daemons Class
///////////////////////////////////////////////////////////////////////////

Daemons::Daemons()
{
	daemon_ptr = NULL;
	no_daemons  =  0;
	daemon_list_size = 0;
	check_new_exec_tid = -1;
	update_tid = -1;
	preen_tid = -1;
	reaper = NO_R;
	all_daemons_gone_action = MASTER_RESET;
	immediate_restart = FALSE;
	immediate_restart_master = FALSE;
}


void
Daemons::RegisterDaemon(daemon *d)
{
	int i;
	if( !daemon_ptr ) {
		daemon_list_size = 10;
		daemon_ptr = (daemon **) malloc(daemon_list_size * sizeof(daemon *));
		for( i=0; i<daemon_list_size; i++ ) {
			daemon_ptr[i] = NULL;
		}
	}

	if (no_daemons >= daemon_list_size) {
		i = daemon_list_size;
		daemon_list_size *= 2;
		daemon_ptr = (daemon **) realloc(daemon_ptr, 
										daemon_list_size * sizeof(daemon *));
		for( ; i<daemon_list_size; i++ ) {
			daemon_ptr[i] = NULL;
		}

	}
	daemon_ptr[no_daemons] = d;
	no_daemons++;
}


void
Daemons::InitParams()
{
	char* tmp = NULL;
	for( int i=0; i < no_daemons; i++ ) {
		if( daemon_ptr[i]->process_name ) {
			tmp = daemon_ptr[i]->process_name;
		}
		daemon_ptr[i]->process_name = param(daemon_ptr[i]->name_in_config_file);
		if( tmp && strcmp(daemon_ptr[i]->process_name, tmp) ) {
				// The path to this daemon has changed in the config
				// file, we will need to start the new version.
			daemon_ptr[i]->newExec = TRUE;
			free( tmp );
			tmp = NULL;
		}

			// check that log file is necessary
		if ( daemon_ptr[i]->log_filename_in_config_file != NULL) {
			if( daemon_ptr[i]->log_name ) {
				free( daemon_ptr[i]->log_name );
			}
			daemon_ptr[i]->log_name = 
					param(daemon_ptr[i]->log_filename_in_config_file);
			if ( daemon_ptr[i]->log_name == NULL && daemon_ptr[i]->runs_here ) {
				dprintf(D_ALWAYS, "Log file not found in config file: %s\n", 
						daemon_ptr[i]->log_filename_in_config_file);
			}
		}
	}
}


int
Daemons::GetIndex(const char* name)
{
	for ( int i=0; i < no_daemons; i++)
		if ( strcmp(daemon_ptr[i]->name_in_config_file,name) == 0 )
			return i;	
	return -1;
}


void
Daemons::CheckForNewExecutable()
{
    int master = GetIndex("MASTER");

	dprintf(D_FULLDEBUG, "enter Daemons::CheckForNewExecutable\n");
    if ( master == -1 ) {
        EXCEPT("CheckForNewExecutable: MASTER not specifed");
    }
	if( daemon_ptr[master]->newExec ) {
			// We already noticed the master has a new binary, so we
			// already started to restart it.  There's nothing else to
			// do here.
		return;
	}
    if( NewExecutable( daemon_ptr[master]->process_name,
					   daemon_ptr[master]->timeStamp ) ) {
		dprintf( D_ALWAYS,"%s was modified, restarting.\n", 
				 daemon_ptr[master]->process_name );
		daemon_ptr[master]->newExec = TRUE;
			// Begin the master restart procedure.
        RestartMaster();
		return;
    }

    for( int i=0; i < no_daemons; i++ ) {
		if( daemon_ptr[i]->runs_here ) {
			if( NewExecutable( daemon_ptr[i]->process_name,
							   daemon_ptr[i]->timeStamp ) ) {
				daemon_ptr[i]->newExec = TRUE;				
				if( immediate_restart ) {
						// If we want to avoid the new_binary_delay,
						// we can just set the newExec flag to false,
						// reset restarts to 0, and kill the daemon.
						// When it gets restarted, the new binary will
						// be used, but we won't think it's a new
						// binary, so we won't use the new_bin_delay.
					daemon_ptr[i]->newExec = FALSE;
					daemon_ptr[i]->restarts = 0;
				}
				if( daemon_ptr[i]->pid ) {
					dprintf( D_ALWAYS,"%s was modified, killing.\n", 
							 daemon_ptr[i]->process_name );
					daemon_ptr[i]->Stop();
				} else {
					if( immediate_restart ) {
							// This daemon isn't running now, but
							// there's a new binary.  Cancel the
							// current start timer and restart it
							// now. 
						daemon_ptr[i]->CancelAllTimers();
						daemon_ptr[i]->Restart();
					}
				}
			}
		}
	}
	dprintf(D_FULLDEBUG, "exit Daemons::CheckForNewExecutable\n");
}


void
Daemons::DaemonsOn()
{
		// Maybe someday we'll add code here to edit the config file.
	StartDaemons = TRUE;
	StartAllDaemons();
}


void
Daemons::DaemonsOff()
{
		// Maybe someday we'll add code here to edit the config file.
	StartDaemons = FALSE;
	all_daemons_gone_action = MASTER_RESET;
	StopAllDaemons();
}


void
Daemons::StartAllDaemons()
{
	for( int i=0; i < no_daemons; i++ ) {
		if( daemon_ptr[i]->pid > 0 ) {
				// the daemon is already started
			continue;
		} 
		if( ! daemon_ptr[i]->runs_here ) continue;
		daemon_ptr[i]->Start();
	}
}


void
Daemons::StopAllDaemons() 
{
	daemons.SetAllReaper();
	int running = 0;
	for( int i=0; i < no_daemons; i++ ) {
		if( daemon_ptr[i]->pid && daemon_ptr[i]->runs_here ) {
			daemon_ptr[i]->Stop();
			running++;
		}
	}
	if( !running ) {
		AllDaemonsGone();
	}	   
}


void
Daemons::StopFastAllDaemons()
{
	daemons.SetAllReaper();
	int running = 0;
	for( int i=0; i < no_daemons; i++ ) {
		if( daemon_ptr[i]->pid && daemon_ptr[i]->runs_here ) {
			daemon_ptr[i]->StopFast();
			running++;
		}
	}
	if( !running ) {
		AllDaemonsGone();
	}	   
}


void
Daemons::ReconfigAllDaemons()
{
	dprintf( D_ALWAYS, "Reconfiguring all running daemons.\n" );
	for( int i=0; i < no_daemons; i++ ) {
		if( daemon_ptr[i]->pid && daemon_ptr[i]->runs_here ) {
			daemon_ptr[i]->Reconfig();
		}
	}
}


void
Daemons::InitMaster()
{
		// initialize master data structure
	int index = GetIndex("MASTER");
	if ( index == -1 ) {
		EXCEPT("InitMaster: MASTER not Specifed");
	}
	daemon_ptr[index]->timeStamp = 
		GetTimeStamp(daemon_ptr[index]->process_name);
	daemon_ptr[index]->pid = daemonCore->getpid();
}


void
Daemons::RestartMaster()
{
#ifdef WANT_DC_PM
	dprintf(D_ALWAYS, "Restarting master not yet supported with WANT_DC_PM\n");
#else
	immediate_restart_master = immediate_restart;
	if( NumberOfChildren() == 0 ) {
		FinishRestartMaster();
	}
	all_daemons_gone_action = MASTER_RESTART;
	StartDaemons = FALSE;
	StopAllDaemons();
#endif
}

// This function is called when all the children have finally exited
// and we're ready to actually do the restart.  It checks whether we
// want to immediately restart or not, and either sets a daemonCore
// timer to call FinalRestartMaster() after MASTER_NEW_BINARY_DELAY or
// just calls FinalRestartMaster directly.
void
Daemons::FinishRestartMaster()
{
	if( immediate_restart_master ) {
		dprintf( D_ALWAYS, "Restarting master right away.\n" );
		FinalRestartMaster();
	} else {
		dprintf( D_ALWAYS, "Restarting master in %d seconds.\n",
				 new_bin_delay );
		daemonCore->
			Register_Timer( new_bin_delay, 0, 
							(TimerHandlercpp)Daemons::FinalRestartMaster,
							"Daemons::FinalRestartMaster()", this );
	}
}

// Function that actually does the restart of the master.
void
Daemons::FinalRestartMaster()
{
#ifndef WANT_DC_PM
	int 		i, max_fds = getdtablesize();
	int			index = GetIndex("MASTER");

	if ( index == -1 ) {
		EXCEPT("Restart Master: MASTER not specifed");
	}

	if ( MasterLock->release() == FALSE ) {
		dprintf( D_ALWAYS,
				 "Can't remove lock on \"%s\"\n",daemon_ptr[index]->log_name);
		EXCEPT( "file_lock(%d)", MasterLockFD );
	}
	(void)close( MasterLockFD );

		// Now close all sockets and fds so our new invocation of
		// condor_master does not inherit them.
	for (i=0; i < max_fds; i++) {
		close(i);
	}

		// Make sure the exec() of the master works.  If it fails,
		// we'll fall past the execl() call, and hit the exponential
		// backoff code.
	while(1) {
		dprintf( D_ALWAYS, "Doing exec( \"%s\" )\n", 
				 daemon_ptr[index]->process_name);
		if( MasterName ) {
			(void)execl(daemon_ptr[index]->process_name, 
						"condor_master", "-f", "-n", MasterName, 0);
		} else {
			(void)execl(daemon_ptr[index]->process_name, 
						"condor_master", "-f", 0);
		}
		daemon_ptr[index]->restarts++;
		i = daemon_ptr[index]->NextStart();
		dprintf( D_ALWAYS, 
				 "Cannot execute condor_master, will try again in %d seconds\n", 
				 i );
		sleep(i);
	}
#endif
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


#if 0
void
Daemons::SignalAll( int signal )
{
	// Sends the given signal to all daemons except the master
	// itself.  (Master has runs_here set to false).
	for ( int i=0; i < no_daemons; i++) {
		if( daemon_ptr[i]->runs_here && (daemon_ptr[i]->pid > 0) ) {
			daemon_ptr[i]->Kill(signal);
		}
	}
}
#endif


// This function returns the number of active child processes currently being
// supervised by the master.
int Daemons::NumberOfChildren()
{
	int result = 0;
	for( int i=0; i < no_daemons; i++) {
		if( daemon_ptr[i]->runs_here && daemon_ptr[i]->pid ) {
			result++;
		}
	}
	dprintf(D_FULLDEBUG,"NumberOfChildren() returning %d\n",result);
	return result;
}


int
Daemons::AllReaper(Service *, int pid, int status)
{
		// find out which daemon died
	for( int i=0; i < no_daemons; i++) {
		if( pid == daemon_ptr[i]->pid ) {
			daemon_ptr[i]->Exited( status );
			if( NumberOfChildren() == 0 ) {
				AllDaemonsGone();
			}
			return TRUE;
		}
	}
	dprintf( D_ALWAYS, "Child %d died, but not a daemon -- Ignored\n", pid);
	return TRUE;
}


int
Daemons::DefaultReaper(Service *, int pid, int status)
{
	for( int i=0; i < no_daemons; i++) {
		if( pid == daemon_ptr[i]->pid ) {
			daemon_ptr[i]->Exited( status );
			if( PublishObituaries ) {
				daemon_ptr[i]->Obituary( status );
			}
			daemon_ptr[i]->Restart();
			return TRUE;
		}
	}
	dprintf( D_ALWAYS, "Child %d died, but not a daemon -- Ignored\n", pid);
	return TRUE;
}



// This function will set the reaper to one that calls
// AllDaemonsGone() when all the daemons have exited.
void
Daemons::SetAllReaper()
{
	if( reaper == ALL_R ) {
			// All reaper is already set.
		return;
	}
#ifdef WANT_DC_PM
	daemonCore->Reset_Reaper( 1, "All Daemon Reaper",
							  (ReaperHandlercpp)Daemons::AllReaper,
							  "Daemons::AllReaper()",this);
#else
	daemonCore->Cancel_Signal(DC_SIGCHLD);
	daemonCore->Register_Signal(DC_SIGCHLD,"DC_SIGCHLD",
		(SignalHandler)all_reaper_sigchld,"all_reaper_sigchld");
#endif
	reaper = ALL_R;
}


// This function will set the reaper to one that calls
// AllDaemonsGone() when all the daemons have exited.
void
Daemons::SetDefaultReaper()
{
	if( reaper == DEFAULT_R ) {
			// The default reaper is already set.
		return;
	}
#ifdef WANT_DC_PM
	daemonCore->Register_Reaper( "default daemon reaper",
								 (ReaperHandlercpp)Daemons::DefaultReaper,
								 "Daemons::DefaultReaper()", this );
#else
	daemonCore->Cancel_Signal( DC_SIGCHLD );
	daemonCore->Register_Signal( DC_SIGCHLD, "DC_SIGCHLD",
		(SignalHandler)standard_sigchld, "standard_sigchld" );
#endif
	reaper = DEFAULT_R;
}


void
Daemons::AllDaemonsGone()
{
	switch( all_daemons_gone_action ) {
	case MASTER_RESET:
		dprintf( D_ALWAYS, "All daemons are gone.\n" );
		SetDefaultReaper();
		break;
	case MASTER_RESTART:
		dprintf( D_ALWAYS, "All daemons are gone.  Restarting.\n" );
		FinishRestartMaster();
		break;
	case MASTER_EXIT:
		dprintf( D_ALWAYS, "All daemons are gone.  Exiting.\n" );
		master_exit(0);
		break;
	}
}


void
Daemons::StartTimers()
{
	static int old_update_int = -1;
	static int old_check_int = -1;
	static int old_preen_int = -1;

	if( old_update_int != update_interval ) {
		if( update_tid != -1 ) {
			daemonCore->Cancel_Timer( update_tid );
		}
			// Start updating the CM in 5 seconds, since if we're the
			// CM, we want to give ourself the chance to start the
			// collector before we try to send an update to it.
		update_tid = daemonCore->
			Register_Timer( 5, update_interval,
							(TimerHandler)update_collector,
							"update_collector" );
		old_update_int = update_interval;
	}

	int new_check = (int)( old_check_int != check_new_exec_interval );
	if( new_check || !StartDaemons ) {
		if( check_new_exec_tid != -1 ) {
			daemonCore->Cancel_Timer( check_new_exec_tid );
			check_new_exec_tid = -1;
		}
	}
	if( new_check && StartDaemons ) {
		check_new_exec_tid = daemonCore->
			Register_Timer( 5, check_new_exec_interval,
							(TimerHandlercpp)Daemons::CheckForNewExecutable,
							"Daemons::CheckForNewExecutable()", this );
	}
	old_check_int = check_new_exec_interval;

	int new_preen = (int)( old_preen_int != preen_interval );
	int first_preen = MIN( HOUR, preen_interval );
	if( !FS_Preen || new_preen ) {
		if( preen_tid != -1 ) {
			daemonCore->Cancel_Timer( preen_tid );
		}
	}
	if( new_preen && FS_Preen ) {
		preen_tid = daemonCore->
			Register_Timer( first_preen, preen_interval,
							(TimerHandler)run_preen,
							"run_preen()" );
	}
	old_preen_int = preen_interval;
}	
