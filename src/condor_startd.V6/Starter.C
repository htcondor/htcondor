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
  Adapted 11/98 for new starter/shadow and WinNT happiness by Todd Tannenbaum
*/

#include "condor_common.h"
#include "startd.h"
#include "classad_merge.h"


Starter::Starter()
{
	s_ad = NULL;
	s_path = NULL;
	s_is_dc = false;

	initRunData();
}


Starter::Starter( const Starter& s )
{
	if( s.rip || s.s_pid || s.s_procfam || s.s_family_size
		|| s.s_pidfamily || s.s_birthdate ) {
		EXCEPT( "Trying to copy a Starter object that's already running!" );
	}

	if( s.s_ad ) {
		s_ad = new ClassAd( *(s.s_ad) );
	} else {
		s_ad = NULL;
	}

	if( s.s_path ) {
		s_path = strnewp( s.s_path );
	} else {
		s_path = NULL;
	}

	s_is_dc = s.s_is_dc;

	initRunData();
}


void
Starter::initRunData( void ) 
{
	rip = NULL;
	s_pid = 0;		// pid_t can be unsigned, so use 0, not -1
	s_procfam = NULL;
	s_family_size = 0;
	s_pidfamily = NULL;
	s_birthdate = 0;
	s_last_snapshot = 0;
}


Starter::~Starter()
{
	if (s_path) {
		delete [] s_path;
	}
	if( s_procfam ) {
		delete s_procfam;
	}
	if( s_pidfamily ) {
		delete [] s_pidfamily;
		s_pidfamily = NULL;
	}
	if( s_ad ) {
		delete( s_ad );
	}
}


bool
Starter::satisfies( ClassAd* job_ad, ClassAd* mach_ad )
{
	int requirements = 0;
	ClassAd* merged_ad = new ClassAd( *mach_ad );
	MergeClassAds( merged_ad, s_ad, true );
	if( ! job_ad->EvalBool(ATTR_REQUIREMENTS, merged_ad, requirements) ) { 
		requirements = 0;
	}
	delete( merged_ad );
	return (bool)requirements;
}


bool
Starter::provides( const char* ability )
{
	int has_it = 0;
	if( ! s_ad ) {
		return false;
	}
	if( ! s_ad->EvalBool(ability, NULL, has_it) ) { 
		has_it = 0;
	}
	return (bool)has_it;
}


void
Starter::setAd( ClassAd* ad )
{
	if( s_ad ) {
		delete( s_ad );
	}
	s_ad = ad;
}


void
Starter::setPath( const char* path )
{
	if( s_path ) {
		delete [] s_path;
	}
	s_path = strnewp( path );
}


void
Starter::setIsDC( bool is_dc )
{
	s_is_dc = is_dc;
}


void
Starter::setResource( Resource* rip )
{
	this->rip = rip;
}


void
Starter::publish( ClassAd* ad, amask_t mask, StringList* list )
{
	if( !(IS_STATIC(mask) && IS_PUBLIC(mask)) ) {
		return;
	}

		// only publish attributes that start with "Has"
	ExprTree *tree, *lhs;
	char *expr_str = NULL, *lhstr = NULL;
	s_ad->ResetExpr();
	while( (tree = s_ad->NextExpr()) ) {
		if( (lhs = tree->LArg()) ) {
			lhs->PrintToNewStr( &lhstr );
		} else {
			dprintf( D_ALWAYS, 
					 "ERROR parsing Starter classad attribute!\n" );
			continue;
		}
		tree->PrintToNewStr( &expr_str );
		if( strincmp(lhstr, "Has", 3) == MATCH ) {
			ad->Insert( expr_str );
			if( list ) {
				list->append( lhstr );
			}
		}
		free( expr_str );
		expr_str = NULL;
		free( lhstr );
		lhstr = NULL;
	}
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

	if ( s_pid == 0 ) {
			// If there's no starter, just return.  We've done our task.
		return TRUE;
	}

	switch( signo ) {
	case DC_SIGSUSPEND:
		sprintf( signame, "DC_SIGSUSPEND" );
		break;
	case DC_SIGHARDKILL:
		signo = SIGQUIT;
		sprintf( signame, "SIGQUIT" );
		break;
	case DC_SIGSOFTKILL:
		signo = SIGTERM;
		sprintf( signame, "SIGTERM" );
		break;
	case DC_SIGPCKPT:
		sprintf( signame, "DC_SIGPCKPT" );
		break;
	case DC_SIGCONTINUE:
		sprintf( signame, "DC_SIGCONTINUE" );
		break;
	case SIGHUP:
		sprintf( signame, "SIGHUP" );
		break;
	case SIGKILL:
		sprintf( signame, "SIGKILL" );
		break;
	default:
		EXCEPT( "Unknown signal (%d) in Starter::reallykill", signo );
	}

#if !defined(WIN32)

	if( !is_dc() ) {
		switch( signo ) {
		case DC_SIGSUSPEND:
			sig = SIGUSR1;
			break;
		case SIGQUIT:
		case DC_SIGHARDKILL:
			sig = SIGINT;
			break;
		case SIGTERM:
		case DC_SIGSOFTKILL:
			sig = SIGTSTP;
			break;
		case DC_SIGPCKPT:
			sig = SIGUSR2;
			break;
		case DC_SIGCONTINUE:
			sig = SIGCONT;
			break;
		case SIGHUP:
			sig = SIGHUP;
			break;
		case SIGKILL:
			sig = SIGKILL;
			break;
		default:
			EXCEPT( "Unknown signal (%d) in Starter::reallykill", signo );
		}
	}


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
		ret = stat(s_path, &st);
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
					 s_path );
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
					s_path, errno );
		}
	}
#endif	// if !defined(WIN32)

	if( is_dc() ) {
			// With DaemonCore, fow now convert a request to kill a
			// process group into a request to kill a pid family via
			// ProcFamily
		if ( type == 1 ) {
			type = 2;
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

	priv = set_root_priv();

#ifndef WIN32
	if( ! is_dc() ) {
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
	}
#endif /* ! WIN32 */

		// Finally, do the deed.
	switch( type ) {
	case 0:
		if( is_dc() ) {		
			ret = daemonCore->Send_Signal( (s_pid), signo );
				// Translate Send_Signal's return code to Unix's kill()
			if ( ret == FALSE ) {
				ret = -1;
			} else {
				ret = 0;
			}
			break;
		} 
#ifndef WIN32
		else {
			ret = ::kill( (s_pid), sig );
			break;
		}
#endif /* ! WIN32 */

	case 1:
#ifndef WIN32
			// We already know we're not DaemonCore.
		ret = ::kill( -(s_pid), sig );
		break;
#endif /* ! WIN32 */

	case 2:
		if( signo != SIGKILL ) {
			dprintf( D_ALWAYS, 
					 "In Starter::killkids() with %s\n", signame );
			EXCEPT( "Starter::killkids() can only handle SIGKILL!" );
		}
		s_procfam->hardkill();  // This really sends SIGKILL
		break;
	}

	set_priv(priv);

	if( ret < 0 ) {
		if(errno==ESRCH) {
			/* Aha, the starter is already dead */
			return 0;
		} else {
			dprintf( D_ALWAYS, "Error sending signal to starter, errno = %d\n", 
					 errno );
			return -1;
		}
	} else {
		return ret;
	}
}


int 
Starter::spawn( start_info_t* info, time_t now )
{

	if( is_dc() ) {
			// Use spiffy new starter.
		s_pid = exec_starter( s_path, info->ji_hname, 
							  info->shadowCommandSock);
	} else {
			// Use old icky non-daemoncore starter.
		s_pid = exec_starter( s_path, info->ji_hname, 
							  info->ji_sock1,
							  info->ji_sock2 );
	}

	if( s_pid == 0 ) {
		dprintf( D_ALWAYS, "ERROR: exec_starter returned %d\n", s_pid );
	} else {
		s_birthdate = now;
		s_procfam = new ProcFamily( s_pid, PRIV_ROOT );
#if WIN32
		// we only support running jobs as user nobody for the first pass
		char nobody_login[60];
		//sprintf(nobody_login,"condor-run-dir_%d",s_pid);
		sprintf(nobody_login,"condor-run-%d",s_pid);
		// set ProcFamily to find decendants via a common login name
		s_procfam->setFamilyLogin(nobody_login);
#endif
		dprintf( D_PROCFAMILY, 
				 "Created new ProcFamily w/ pid %d as the parent.\n", s_pid );
		recompute_pidfamily( now );
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
}


int
Starter::exec_starter( char* starter, char* hostname, 
					   Stream *sock)
{
	char args[_POSIX_ARG_MAX];
	Stream *sock_inherit_list[] = { sock, 0 };

	if ( resmgr->is_smp() ) {
		// Note: the "-a" option is a daemon core option, so it
		// must come first on the command line.
		sprintf( args, "condor_starter -f -a %s %s", rip->r_id_str, hostname );
	} else {
		sprintf(args, "condor_starter -f %s", hostname);
	}

	dprintf ( D_FULLDEBUG, "About to Create_Process \"%s\".\n", args );

	s_pid = daemonCore->Create_Process( s_path, args, PRIV_ROOT, main_reaper, TRUE, 
										NULL, NULL, TRUE, sock_inherit_list );
	if( s_pid == FALSE ) {
		dprintf( D_ALWAYS, "ERROR: exec_starter failed!\n");
		s_pid = 0;
	}
	return s_pid;
}

int
Starter::exec_starter( char* starter, char* hostname, 
					   int main_sock, int err_sock )
{
#if defined(WIN32) /* THIS IS UNIX SPECIFIC */
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
			   NOTE: Since we're in the child, we don't want to call
			   EXCEPT() or weird things will try to happen, as the
			   forked child tries to vacate starter, update the CM,
			   etc.  So, just dprintf() and exit(), instead.
			*/
		
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
			dprintf( D_ALWAYS, 
					 "setsid() failed in child, errno: %d\n", errno );
			exit( 4 );
		}

			// Now, dup the special socks to their well-known fds.
		if( dup2(main_sock,0) < 0 ) {
			dprintf( D_ALWAYS, "dup2(%d,0) failed in child, errno: %d\n",
					 main_sock, errno ); 
			exit( 4 );
		}
		if( dup2(main_sock,1) < 0 ) {
			dprintf( D_ALWAYS, "dup2(%d,1) failed in child, errno: %d\n",
					 main_sock, errno );
			exit( 4 );
		}
		if( dup2(err_sock,2) < 0 ) {
			dprintf( D_ALWAYS, "dup2(%d,2) failed in child, errno: %d\n",
					 err_sock, errno );
			exit( 4 );
		}

			// Close everything else to prevent fd leaks and other
			// problems. 
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
						"-a", rip->r_id_str, 0 );
		} else {			
			(void)execl(starter, "condor_starter", hostname, 
						daemonCore->InfoCommandSinfulString(), 0 );

		}
			// If we got this far, there was an error in execl().
		dprintf( D_ALWAYS, 
				 "ERROR: execl(%s, condor_starter, %s, %s, 0) errno: %d\n", 
				 starter, daemonCore->InfoCommandSinfulString(), hostname,
				 errno );
		exit( 4 );
	}
	if ( pid < 0 ) {
		pid = 0;
	}
	return pid;
#endif // !defined(WIN32)
}

		
bool
Starter::active()
{
	return( (s_pid != 0) );
}
	

void
Starter::dprintf( int flags, char* fmt, ... )
{
	va_list args;
	va_start( args, fmt );
	if( rip ) {
		rip->dprintf_va( flags, fmt, args );
	} else {
		::_condor_dprintf_va( flags, fmt, args );
	}
	va_end( args );
}


void
Starter::recompute_pidfamily( time_t now ) 
{
	if( !s_procfam ) {
		dprintf( D_PROCFAMILY, 
		 "Starter::recompute_pidfamily: ERROR: No ProcFamily object.\n" );
		return;
	}
	dprintf( D_PROCFAMILY, "Recomputing pid family\n" );
	s_procfam->takesnapshot();
	if( s_pidfamily ) {
		delete [] s_pidfamily;
		s_pidfamily = NULL;
	}
	s_family_size = s_procfam->currentfamily( s_pidfamily );
	if( ! s_family_size ) {
		dprintf( D_ALWAYS, 
			"WARNING: No processes found in starter's family\n" );
	}
	if( now ) {
		s_last_snapshot = now;
	} else {
		s_last_snapshot = time( NULL );
	}
}


void
Starter::printInfo( int debug_level )
{
	dprintf( debug_level, "Info for \"%s\":\n", s_path );
	dprintf( debug_level | D_NOHEADER, "IsDaemonCore: %s\n", 
			 s_is_dc ? "True" : "False" );
	if( ! s_ad ) {
		dprintf( debug_level | D_NOHEADER, 
				 "No ClassAd available!\n" ); 
	} else {
		s_ad->dPrint( debug_level );
	}
	dprintf( debug_level | D_NOHEADER, "*** End of starter info ***\n" ); 
}
