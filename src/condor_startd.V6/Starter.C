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
/* 
  This file implements the Starter class, used by the startd to keep
  track of a resource's starter process.  

  Written 10/6/97 by Derek Wright <wright@cs.wisc.edu>
*/

#include "startd.h"
static char *_FileName_ = __FILE__;


Starter::Starter( Resource* rip )
{
	this->rip = rip;
	s_pid = -1;
	s_name = NULL;
	s_procfam = NULL;
	s_pidfamily = NULL;
	s_family_size = 0;
}


Starter::~Starter()
{
	free( s_name );
	if( s_procfam ) {
		delete s_procfam;
	}
	if( s_pidfamily ) {
		delete [] s_pidfamily;
	}
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
	return reallykill( signo, 0 );
}


int
Starter::killpg( int signo )
{
	return reallykill( signo, 1 );
}


void
Starter::killkids( int signo ) 
{
	reallykill( signo, 2 );
}


int
Starter::reallykill( int signo, int type )
{
	struct stat st;
	int 		ret = 0, sig = 0;
	priv_state	priv;
	char		signame[32];
	signame[0]='\0';

	if ( s_pid <= 0 ) {
			// If there's no starter, just return.  We've done our task.
		return TRUE;
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
		EXCEPT( "Unknown signal (%d) in Starter::reallykill", signo );
	}
#endif

		/* 
		   On sites where the condor binaries are installed on NFS,
		   and the OS is foolish enough to try to page text segments
		   of programs to the binary even on NFS, we're might get a
		   segv in the starter if the NFS server is down.  So, we want
		   to try to stat the binary here to make sure the NFS server
		   is alive and well.  There are a few cases:
		   1) Hard NFS mount:  stat will block until things are well.
		   2) Soft NFS mount:  stat will return right away with some
 		      platform dependent errno.
		   3) starter binary has been moved so stat fails with ENOENT
		      or ENOTDIR (once the stat returns).
		   4) Permissions have been screwed so stat fails with EACCES.
		   So, if stat succeeds, all is well, and we do the kill.
		   If stat fails with ENOENT or ENOTDIR, we assume the server
		     is alive, but the binary has been moved, so we do the 
		     kill.  Similarly with EACCES.
		   If stat fails with "The file server is down" (ENOLINK), we
		     poll every 15 seconds until the stat succeeds or fails
			 with a known errno and dprintf to let the world know.
		   On dux 4.0, we might also get ESTALE, which means the
		     binary lives on a stale NFS mount.  If that's the case,
			 we're screwed (and the startd is probably screwed, too,
			 to EXCEPT to let the world know what went wrong.
		   Any other errno we get back from stat is something we don't
		     know how to handle, so we EXCEPT and print the errno.
			 
		   Added comments, polling, and check for known errnos
		     -Derek Wright, 1/2/98
		 */
	int needs_stat = TRUE, first_time = TRUE;
	while( needs_stat ) {
		errno = 0;
		ret = stat(s_name, &st);
		if( ret >=0 ) {
			needs_stat = FALSE;
			continue;
		}
		switch( errno ) {
		case EINTR:
		case ETIMEDOUT:
			break;
		case ENOENT:
		case ENOTDIR:
		case EACCES:
			needs_stat = FALSE;
			break;
#if defined(OSF1)
				// dux 4.0 doesn't have ENOLINK for stat().  It does
				// have ESTALE, which means our binaries live on a
				// stale NFS mount.  So, we can at least EXCEPT with a
				// more specific error message.
		case ESTALE:
			EXCEPT( "Condor binaries are on a stale NFS mount.  Aborting." );
			break;
#else
		case ENOLINK:
			dprintf( D_ALWAYS, 
					 "Can't stat binary (%s), file server probably down.\n",
					 s_name );
			if( first_time ) {
				dprintf( D_ALWAYS, 
						 "Will retry every 15 seconds until server is back up.\n" );
				first_time = FALSE;
			}
			sleep(15);
			break;
#endif
		default:
			EXCEPT( "stat(%s) failed with unexpected errno (%d)", 
					s_name, errno );
		}
	}


	switch( type ) {
	case 0:
		dprintf( D_FULLDEBUG, 
				 "In Starter::kill() with pid %d, sig %d (%s)\n", 
				 s_pid, signo, signame );
		break;
	case 1:
		dprintf( D_FULLDEBUG, 
				 "In Starter::killpg() with pid %d, sig %d (%s)\n", 
				 s_pid, signo, signame );
	case 2:
		dprintf( D_FULLDEBUG, 
				 "In Starter::kill_kids() with pid %d, sig %d (%s)\n", 
				 s_pid, signo, signame );
		break;
	default:
		EXCEPT( "Unknown type (%d) in Starter::reallykill\n" );
	}


#if !defined(WIN32) /* NEED TO PORT TO WIN32 */

	priv = set_root_priv();

		// If we're just doing a plain kill, and the signal we're
		// sending isn't SIGSTOP or SIGCONT, send a SIGCONT to the
		// starter just for good measure.
	if( sig != SIGSTOP && sig != SIGCONT && sig != SIGKILL ) {
		if( type == 1 ) { 
			ret = ::kill( -(s_pid), SIGCONT );
		} else if( type == 0 ) {
			ret = ::kill( (s_pid), SIGCONT );
		}
	}
		// Finally, do the deed.
	switch( type ) {
	case 0:
		ret = ::kill( (s_pid), sig );
		break;
	case 1:
		ret = ::kill( -(s_pid), sig );
		break;
	case 2:
		if( signo != DC_SIGKILL ) {
			dprintf( D_ALWAYS, 
					 "In Starter::killkids() with %s\n", signame );
			EXCEPT( "Starter::killkids() can only handle DC_SIGKILL!" );
		}
		s_procfam->hardkill();  // This really sends SIGKILL
		break;
	}

	set_priv(priv);

#endif /* WIN32 */

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
		s_procfam = new ProcFamily( s_pid, PRIV_ROOT, resmgr->m_proc );
	}
	return s_pid;
}


void
Starter::exited()
{
		// Just for good measure, try to kill what's left of our whole
		// pid family.  
	s_procfam->hardkill();

		// Now, delete any files lying around.
	cleanup_execute_dir( s_pid );

		// Finally, we can free up our memory and data structures.
	s_pid = -1;
	free( s_name );
	s_name = NULL;

	if( s_procfam ) {
		delete s_procfam;
		s_procfam = NULL;
	}
	if( s_pidfamily ) {
		delete [] s_pidfamily;
		s_pidfamily = NULL;
	}
	s_family_size = 0;
}


int
Starter::exec_starter( char* starter, char* hostname, 
					   int main_sock, int err_sock )
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
		if( resmgr->is_smp() ) {
			(void)execl(starter, "condor_starter", hostname, 
						daemonCore->InfoCommandSinfulString(), 
						"-a", rip->r_id, 0 );
		} else {			
			(void)execl(starter, "condor_starter", hostname, 
						daemonCore->InfoCommandSinfulString(), 0 );

		}
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
	

void
Starter::dprintf( int flags, char* fmt, ... )
{
	va_list args;
	va_start( args, fmt );
	rip->dprintf_va( flags, fmt, args );
	va_end( args );
}


void
Starter::recompute_pidfamily() 
{
	if( !s_procfam ) {
		return;
	}
	s_procfam->takesnapshot();
	if( s_pidfamily ) {
		delete [] s_pidfamily;
	}
	s_family_size = s_procfam->currentfamily( s_pidfamily );
}
