/* 
  This file implements the Starter class, used by the startd to keep
  track of a resource's starter process.  

  Written 10/6/97 by Derek Wright <wright@cs.wisc.edu>
*/

#include "startd.h"
static char *_FileName_ = __FILE__;


Starter::Starter()
{
	s_pid = -1;
	s_name = NULL;
}


Starter::~Starter()
{
	free( s_name );
}


void
Starter::setname( char* name )
{
	if( s_name ) {
		free( s_name );
	}
	s_name = strdup( name );
}


int
Starter::kill( int signo )
{
	return this->reallykill( signo, 0 );
}


int
Starter::killpg( int signo )
{
	return this->reallykill( signo, 1 );
}


int
Starter::reallykill( int signo, int pg )
{
	struct stat st;
	int 		ret = 0, sig = 0;
	priv_state	priv;
	char	signame[1024];
	signame[0]='\0';

	if ( s_pid <= 0 ) {
		dprintf( D_ALWAYS, "Invalid pid (%d) in Starter::kill(), returning.\n",  
				 s_pid );
		return -1;
	}

#if !defined(WIN32)
	switch( signo ) {
	case DC_SIGSUSPEND:
		sig = SIGUSR1;
		sprintf( signame, "SIGSUSPEND" );
		break;
	case DC_SIGHARDKILL:
		sig = SIGINT;
		sprintf( signame, "SIGHARDKILL" );
		break;
	case DC_SIGSOFTKILL:
		sig = SIGTSTP;
		sprintf( signame, "SIGSOFTKILL" );
		break;
	case DC_SIGPCKPT:
		sig = SIGUSR2;
		sprintf( signame, "SIGPCKPT" );
		break;
	case DC_SIGCONTINUE:
		sig = SIGCONT;
		sprintf( signame, "SIGCONTINUE" );
		break;
	case DC_SIGHUP:
		sig = SIGHUP;
		sprintf( signame, "SIGHUP" );
		break;
	case DC_SIGKILL:
		sig = SIGKILL;
		sprintf( signame, "SIGKILL" );
		break;
	default:
		EXCEPT( "Unknown signal (%d) in Starter::kill", signo );
	}
#endif

	for (errno = 0; (ret = stat(s_name, &st)) < 0; errno = 0) {
#if !defined(WIN32)
		if (errno == ETIMEDOUT)
			continue;
#endif
		EXCEPT( "Starter::kill(%d): cannot stat <%s> - errno = %d\n",
				signo, s_name, errno);
	}

	if( pg ) {
		dprintf( D_FULLDEBUG, 
				 "In Starter::killpg() with pid %d, sig %d (%s)\n", 
				 s_pid, signo, signame );
	} else {
		dprintf( D_FULLDEBUG, 
				 "In Starter::kill() with pid %d, sig %d (%s)\n", 
				 s_pid, signo, signame );
	}

	priv = set_root_priv();

#if !defined(WIN32) /* NEED TO PORT TO WIN32 */
	if (sig != SIGSTOP && sig != SIGCONT) {
		if( pg ) {
			ret = ::kill( -(s_pid), SIGCONT );
		} else {
			ret = ::kill( (s_pid), SIGCONT );
		}
	}
	if( pg ) {
		ret = ::kill( -(s_pid), sig );
	} else {
		ret = ::kill( (s_pid), sig );
	}
#endif

	set_priv(priv);

	if( ret < 0 ) {
		dprintf( D_ALWAYS, "Error sending signal to starter, errno = %d\n", 
				 errno );
		return -1;
	}
	return ret;
}


int 
Starter::spawn( start_info_t* info )
{
	s_pid = exec_starter( s_name, info->ji_hname, 
						  info->ji_sock1,
						  info->ji_sock2 );

	if( s_pid < 0 ) {
		dprintf( D_ALWAYS, "ERROR: exec_starter returned %d\n", s_pid );
		s_pid = -1;
	} else {

	}
	return s_pid;
}


void
Starter::exited()
{
	s_pid = -1;
	free( s_name );
	s_name = NULL;

	cleanup_execute_dir();
}


int
exec_starter(char* starter, char* hostname, int main_sock, int err_sock)
{
#if defined(WIN32) /* NEED TO PORT TO WIN32 */
	return 0;
#else
	int i;
	int pid;
	int n_fds = getdtablesize();

#if defined(Solaris)
	sigset_t set;
	if( sigprocmask(SIG_SETMASK,0,&set)  == -1 ) {
		EXCEPT("Error in reading procmask, errno = %d\n", errno);
	}
	for (i=0; i < MAXSIG; i++) {
		block_signal(i);
	}
#else
	int omask = sigblock(-1);
#endif

	if( (pid = fork()) < 0 ) {
		EXCEPT( "fork" );
	}

	if (pid) {	/* The parent */
		/*
		  (void)sigblock(omask);
		  */
#if defined(Solaris)
		if ( sigprocmask(SIG_SETMASK, &set, 0)  == -1 )
			{EXCEPT("Error in setting procmask, errno = %d\n", errno);}
#else
		(void)sigsetmask(omask);
#endif
		(void)close(main_sock);
		(void)close(err_sock);

		dprintf(D_ALWAYS,
				"exec_starter( %s, %d, %d ) : pid %d\n",
				hostname, main_sock, err_sock, pid);
		dprintf(D_ALWAYS, "execl(%s, \"condor_starter\", %s, 0)\n",
				starter, hostname);
	} else {	/* the child */
		/*
		 * N.B. The child is born with TSTP blocked, so he can be
		 * sure to set up his handler before accepting it.
		 */

		/*
		 * This should not change process groups because the
		 * condor_master daemon may want to do a killpg at some
		 * point...
		 *
		 *	if( setpgrp(0,getpid()) ) {
		 *		EXCEPT( "setpgrp(0, %d)", getpid() );
		 *	}
		 */
			/*
			 * We _DO_ want to create the starter with it's own
			 * process group, since when KILL evaluates to True, we
			 * don't want to kill the startd as well.  The master no
			 * longer kills via a process group to do a quick clean
			 * up, it just sends signals to the startd and schedd and
			 * they, in turn, do whatever quick cleaning needs to be 
			 * done. 
			 */
		if( setsid() < 0 ) {
			EXCEPT( "setsid()" );
		}

		if (dup2(main_sock,0) < 0) {
			EXCEPT("dup2(%d,%d)", main_sock, 0);
		}
		if (dup2(main_sock,1) < 0) {
			EXCEPT("dup2(%d,%d)", main_sock, 1);
		}
		if (dup2(err_sock,2) < 0) {
			EXCEPT("dup2(%d,%d)", err_sock, 2);
		}

		for(i = 3; i<n_fds; i++) {
			(void) close(i);
		}
		/*
		 * Starter must be exec'd as root so it can change to client's
		 * uid and gid.
		 */
		set_root_priv();
		(void)execl(starter, "condor_starter", hostname, 
					daemonCore->InfoCommandSinfulString(), 0);
		EXCEPT( "execl(%s, condor_starter, %s, %s, 0)", starter, 
				daemonCore->InfoCommandSinfulString(), hostname );
	}
	return pid;
#endif // !defined(WIN32)
}

		
bool
Starter::active()
{
	return( (s_pid != -1) );
}
	
