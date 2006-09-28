/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
/* 
  This file implements the Starter class, used by the startd to keep
  track of a resource's starter process.  

  Written 10/6/97 by Derek Wright <wright@cs.wisc.edu>
  Adapted 11/98 for new starter/shadow and WinNT happiness by Todd Tannenbaum
*/

#include "condor_common.h"
#include "startd.h"
#include "classad_merge.h"
#include "dynuser.h"
#include "condor_auth_x509.h"
#include "setenv.h"
#include "my_popen.h"
#include "basename.h"

#ifdef WIN32
extern dynuser *myDynuser;
#endif

Starter::Starter()
{
	s_ad = NULL;
	s_path = NULL;
	s_is_dc = false;

	initRunData();
}


Starter::Starter( const Starter& s )
{
	if( s.s_claim || s.s_pid || s.s_procfam || s.s_family_size
		|| s.s_pidfamily || s.s_birthdate 
		|| s.s_port1 >= 0 || s.s_port2 >= 0 ) {
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
	s_claim = NULL;
	s_pid = 0;		// pid_t can be unsigned, so use 0, not -1
	s_procfam = NULL;
	s_family_size = 0;
	s_pidfamily = NULL;
	s_birthdate = 0;
	s_last_snapshot = 0;
	s_kill_tid = -1;
	s_port1 = -1;
	s_port2 = -1;
	s_reaper_id = -1;

#if HAVE_BOINC
	s_is_boinc = false;
#endif /* HAVE_BOINC */

		// Initialize our procInfo structure so we don't use any
		// values until we've actually computed them.
	memset( (void*)&s_pinfo, 0, (size_t)sizeof(s_pinfo) );
}


Starter::~Starter()
{
	cancelKillTimer();

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
	ClassAd* merged_ad;
	if( mach_ad ) {
		merged_ad = new ClassAd( *mach_ad );
		MergeClassAds( merged_ad, s_ad, true );
	} else {
		merged_ad = new ClassAd( *s_ad );
	}
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
Starter::setClaim( Claim* c )
{
	s_claim = c;
}


void
Starter::setPorts( int port1, int port2 )
{
	s_port1 = port1;
	s_port2 = port2;
}


void
Starter::publish( ClassAd* ad, amask_t mask, StringList* list )
{
	if( !(IS_STATIC(mask) && IS_PUBLIC(mask)) ) {
		return;
	}

		// Check for ATTR_STARTER_IGNORED_ATTRS. If defined,
		// we insert all attributes from the starter ad except those
		// in the list. If not, we fall back on our old behavior
		// of only inserting attributes prefixed with "Has" or
		// "Java". Either way, we only add the "Has" attributes
		// into the StringList (the StarterAbilityList)
	char* ignored_attrs = NULL;
	StringList* ignored_attr_list = NULL;
	if (s_ad->LookupString(ATTR_STARTER_IGNORED_ATTRS, &ignored_attrs)) {
		ignored_attr_list = new StringList(ignored_attrs);
		free(ignored_attrs);

		// of course, we don't want ATTR_STARTER_IGNORED_ATTRS
		// in either!
		ignored_attr_list->append(ATTR_STARTER_IGNORED_ATTRS);
	}

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

		if (ignored_attr_list) {
				// insert every attr that's not in the ignored_attr_list
			if (!ignored_attr_list->contains(lhstr)) {
				ad->Insert(expr_str);
				if (strincmp(lhstr, "Has", 3) == MATCH) {
					list->append(lhstr);
				}
			}
		}
		else {
				// no list of attrs to ignore - fallback on old behavior
			if( strincmp(lhstr, "Has", 3) == MATCH ) {
				ad->Insert( expr_str );
				if( list ) {
					list->append( lhstr );
				}
			} else if( strincmp(lhstr, "Java", 4) == MATCH ) {
				ad->Insert( expr_str );
			}
		}

		free( expr_str );
		expr_str = NULL;
		free( lhstr );
		lhstr = NULL;
	}

	if (ignored_attr_list) {
		delete ignored_attr_list;
	}
}


bool
Starter::kill( int signo )
{
	return reallykill( signo, 0 );
}


bool
Starter::killpg( int signo )
{
	return reallykill( signo, 1 );
}


void
Starter::killkids( int signo ) 
{
	reallykill( signo, 2 );
}


bool
Starter::reallykill( int signo, int type )
{
#ifndef WIN32
	struct stat st;
#endif
	int 		ret = 0, sig = 0;
	priv_state	priv;
	char		signame[32];
	signame[0]='\0';

	if ( s_pid == 0 ) {
			// If there's no starter, just return.  We've done our task.
		return true;
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
#if defined(OSF1) || defined(Darwin) || defined(CONDOR_FREEBSD)
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
			if( ret == FALSE ) {
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

		// Finally, figure out what bool to return...
	if( ret < 0 ) {
		if(errno==ESRCH) {
			/* Aha, the starter is already dead */
			return true;
		} else {
			dprintf( D_ALWAYS, 
					 "Error sending signal to starter, errno = %d (%s)\n", 
					 errno, strerror(errno) );
			return false;
		}
	} else {
		return true;
	}
}



int 
Starter::spawn( time_t now, Stream* s )
{
	if( isCOD() ) {
		s_pid = execCODStarter();
#if HAVE_BOINC
	} else if( isBOINC() ) {
		s_pid = execBOINCStarter(); 
#endif /* HAVE_BOINC */
	} else if( is_dc() ) {
		s_pid = execDCStarter( s ); 
	} else {
			// Use old icky non-daemoncore starter.
		s_pid = execOldStarter();
	}

	if( s_pid == 0 ) {
		dprintf( D_ALWAYS, "ERROR: exec_starter returned %d\n", s_pid );
	} else {

		PidEnvID penvid;

		// if there isn't any ancestor information for this pid, that is ok.
		daemonCore->InfoEnvironmentID(&penvid, s_pid);
		
		s_birthdate = now;
		s_procfam = new ProcFamily( s_pid, &penvid, PRIV_ROOT );

		dprintf( D_PROCFAMILY, 
				 "Created new ProcFamily w/ pid %d as the parent.\n",
				 s_pid ); 
		recomputePidFamily( now );
	}
	return s_pid;
}


void
Starter::exited()
{
		// Make sure our time isn't going to go off.
	cancelKillTimer();

		// Just for good measure, try to kill what's left of our whole
		// pid family.  
	s_procfam->hardkill();

		// Now, delete any files lying around.
	cleanup_execute_dir( s_pid );

#if !defined(WIN32)
	if( param_boolean( "GLEXEC_STARTER", false ) ) {
		cleanupAfterGlexec();
	}
#endif
}


int
Starter::execCODStarter( void )
{
	int rval;
	MyString lock_env;
	ArgList args;
	Env env;
	char* tmp;

	tmp = param( "LOCK" );
	if( ! tmp ) { 
		tmp = param( "LOG" );
	}
	if( ! tmp ) { 
		EXCEPT( "LOG not defined!" );
	}
	lock_env = "_condor_STARTER_LOCK=";
	lock_env += tmp;
	free( tmp );
	lock_env += DIR_DELIM_CHAR;
	lock_env += "StarterLock.cod";

	env.SetEnv(lock_env.Value());

	if(!s_claim->makeCODStarterArgs(args)) {
		dprintf( D_ALWAYS, "ERROR: failed to create COD starter args.\n");
		return 0;
	}

	int* std_fds_p = NULL;
	int std_fds[3];
	int pipe_fds[2];
	if( s_claim->hasJobAd() ) {
		if( ! daemonCore->Create_Pipe(pipe_fds) ) {
			dprintf( D_ALWAYS, "ERROR: Can't create pipe to pass job ClassAd "
					 "to starter, aborting\n" );
			return 0;
		}
			// pipe_fds[0] is the read-end of the pipe.  we want that
			// setup as STDIN for the starter.  we'll hold onto the
			// write end of it so 
		std_fds[0] = pipe_fds[0];
		std_fds[1] = -1;
		std_fds[2] = -1;
		std_fds_p = std_fds;
	}

	rval = execDCStarter( args, &env, std_fds_p, NULL );

	if( s_claim->hasJobAd() ) {
			// now that the starter has been spawned, we need to do
			// some things with the pipe:

			// 1) close our copy of the read end of the pipe, so we
			// don't leak it.  we have to use DC::Close_Pipe() for
			// this, not just close(), so things work on windoze.
		daemonCore->Close_Pipe( pipe_fds[0] );

			// 2) dump out the job ad to the write end, since the
			// starter is now alive and can read from the pipe.

		s_claim->writeJobAd( pipe_fds[1] );

			// Now that all the data is written to the pipe, we can
			// safely close the other end, too.  
		daemonCore->Close_Pipe( pipe_fds[1] );
	}

	return rval;
}


#if HAVE_BOINC
int
Starter::execBOINCStarter( void )
{
	ArgList args;
	args.AppendArg("condor_starter");
	args.AppendArg("-f");
	args.AppendArg("-append");
	args.AppendArg("boinc");
	args.AppendArg("-job-keyword");
	args.AppendArg("boinc");
	return execDCStarter( args, NULL, NULL, NULL );
}
#endif /* HAVE_BOINC */


int
Starter::execDCStarter( Stream* s )
{
	ArgList args;

	char* hostname = s_claim->client()->host();
	if ( resmgr->is_smp() ) {
		// Note: the "-a" option is a daemon core option, so it
		// must come first on the command line.
		args.AppendArg("condor_starter");
		args.AppendArg("-f");
		args.AppendArg("-a");
		args.AppendArg(s_claim->rip()->r_id_str);
		args.AppendArg(hostname);
	} else {
		args.AppendArg("condor_starter");
		args.AppendArg("-f");
		args.AppendArg(hostname);
	}
	execDCStarter( args, NULL, NULL, s );

	return s_pid;
}


int
Starter::execDCStarter( ArgList const &args, Env const *env, 
						int* std_fds, Stream* s )
{
	Stream *sock_inherit_list[] = { s, 0 };
	Stream** inherit_list = NULL;
	if( s ) {
		inherit_list = sock_inherit_list;
	}

	const ArgList* final_args = &args;
	const Env* final_env = env;
	const char* final_path = s_path;

#if !defined(WIN32)
	// see if we should be using glexec to spawn the starter.
	// if we are, the cmd, args, and env to use will be modified
	ArgList glexec_args;
	Env glexec_env;
	if( param_boolean( "GLEXEC_STARTER", false ) ) {
		if( ! prepareForGlexec( args, env, glexec_args, glexec_env ) ) {
			// something went wrong; prepareForGlexec will
			// have already logged it
			cleanupAfterGlexec();
			return 0;
		}
		final_args = &glexec_args;
		final_env = &glexec_env;
		final_path = glexec_args.GetArg(0);
	}
#endif
								   
	int reaper_id;
	if( s_reaper_id > 0 ) {
		reaper_id = s_reaper_id;
	} else {
		reaper_id = main_reaper;
	}

	if(DebugFlags & D_FULLDEBUG) {
		MyString args_string;
		final_args->GetArgsStringForDisplay(&args_string);
		dprintf( D_FULLDEBUG, "About to Create_Process \"%s\"\n",
				 args_string.Value() );
	}

	s_pid = daemonCore->
		Create_Process( final_path, *final_args, PRIV_ROOT, reaper_id,
						TRUE, final_env, NULL, TRUE, inherit_list, std_fds );
	if( s_pid == FALSE ) {
		dprintf( D_ALWAYS, "ERROR: exec_starter failed!\n");
		s_pid = 0;
	}
	return s_pid;
}


int
Starter::execOldStarter( void )
{
#if defined(WIN32) /* THIS IS UNIX SPECIFIC */
	return 0;
#else
	if( param_boolean( "GLEXEC_STARTER", false ) ) {
		// we don't support using glexec to spawn the old starter
		dprintf( D_ALWAYS,
			 "error: can't spawn starter %s via glexec\n",
			 s_path );
		return 0;
	}

	char* hostname = s_claim->client()->host();
	int i;
	int pid;
	int n_fds = getdtablesize();
	int main_sock = s_port1;
	int err_sock = s_port2;

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
				s_path, hostname);
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
			(void)execl( s_path, "condor_starter", hostname, 
						 daemonCore->InfoCommandSinfulString(), 
						 "-a", s_claim->rip()->r_id_str, 0 );
		} else {			
			(void)execl( s_path, "condor_starter", hostname, 
						 daemonCore->InfoCommandSinfulString(), 0 );

		}
			// If we got this far, there was an error in execl().
		dprintf( D_ALWAYS, 
				 "ERROR: execl(%s, condor_starter, %s, %s, 0) errno: %d\n", 
				 s_path, daemonCore->InfoCommandSinfulString(), hostname,
				 errno );
		exit( 4 );
	}
	if ( pid < 0 ) {
		pid = 0;
	}
	return pid;
#endif // !defined(WIN32)
}

#if !defined(WIN32)
bool
Starter::prepareForGlexec( const ArgList& orig_args, const Env* orig_env,
                           ArgList& glexec_args, Env& glexec_env )
{
	// if GLEXEC_STARTER is set, use glexec to invoke the
	// starter (and fail if we can't). this involves:
	//   - verifying that we have a delegated proxy from
	//     the user stored, since we need to hand it to
	//     glexec so it can look up the UID/GID
	//   - invoking 'glexec whoami' to find out the target username
	//   - creating a directory /tmp/condor.<startd_pid>.<username> for 
	//     the copied proxy to go into, as well as the StarterLog and
	//     execute dir
	//   - adding the contents of the GLEXEC config param
	//     to the front of the command line
	//   - setting up glexec's environment (setting the
	//     mode, handing off the proxy, etc.)

	// verify that we have a stored proxy
	if( s_claim->client()->proxyFile() == NULL ) {
		dprintf( D_ALWAYS,
		         "cannot use glexec to spawn starter: no proxy "
		         "(is GLEXEC_STARTER set in the shadow?)\n" );
		return false;
	}

	// read the cert into a string
	FILE* fp = fopen( s_claim->client()->proxyFile(), "r" );
	if( fp == NULL ) {
		dprintf( D_ALWAYS,
		         "cannot use glexec to spawn starter: "
		         "couldn't open proxy: %s (%d)\n",
		         strerror(errno), errno );
		return false;
	}
	MyString pem_str;
	while( pem_str.readLine( fp, true ) );
	fclose( fp );

	// param for the user dir for glexec to use, put it into a MyString, and
	// free it.  default to /tmp if not defined.
	char *glexec_user_dir_tmp = param("GLEXEC_USER_DIR");
	MyString glexec_user_dir;
	if (glexec_user_dir_tmp) {
		glexec_user_dir = glexec_user_dir_tmp;
		free(glexec_user_dir_tmp);
		glexec_user_dir_tmp = NULL;
	} else {
		glexec_user_dir = "/tmp";
	}

	// get the glexec command line prefix from config
	char* glexec_argstr = param( "GLEXEC" );
	if ( ! glexec_argstr ) {
		dprintf( D_ALWAYS,
		         "cannot use glexec to spawn starter: "
		         "GLEXEC not given in config\n" );
		return false;
	}

	// cons up a command line for popen.  we want to run this:
	// /bin/sh -c 'WHOAMI=`whoami`;cd <user_dir>;umask 077;
	//    mkdir -p condor.<startd_pid>.$WHOAMI;
	//    cd condor.<startd_pid>.$WHOAMI;
	//    mkdir -p execute;mkdir -p log;
	//    echo $WHOAMI
	//
	// (where <user_dir> is the glexec_user_dir param'ed above,
	//  and <startd_pid> is our pid)

	// parse the glexec args for invoking whoami.  do not free them yet,
	// except on an error, as we use them again below.
	MyString whoami_err;
	ArgList  glexec_whoami_args;
	if( ! glexec_whoami_args.AppendArgsV1RawOrV2Quoted( glexec_argstr,
						     &whoami_err ) ) {
		dprintf( D_ALWAYS,
		         "GLEXEC: failed to parse GLEXEC from config: %s\n",
		         whoami_err.Value() );
		free( glexec_argstr );
		return 0;
	}

	// add the rest of the args for the whoami / mkdir mojo
	MyString glexec_whoami_arg;
	char* shell_path = param("SH");
	if (shell_path) {
			// glexec refuses to follow symlinks, so we offer
			// the "SH" knob as a workaround
		glexec_whoami_args.AppendArg(shell_path);
		free(shell_path);
	}
	else {
		glexec_whoami_args.AppendArg("/bin/sh");
	}
	glexec_whoami_args.AppendArg("-c");
	int my_pid = getpid();
	glexec_whoami_arg = "WHOAMI=`whoami`;cd ";
	glexec_whoami_arg += glexec_user_dir.Value();
	glexec_whoami_arg += ";umask 077;";
	glexec_whoami_arg.sprintf_cat("mkdir -p condor.%d.$WHOAMI;", my_pid);
	glexec_whoami_arg.sprintf_cat("cd condor.%d.$WHOAMI;", my_pid);
	glexec_whoami_arg += "mkdir -p execute;mkdir -p log;echo $WHOAMI";
	glexec_whoami_args.AppendArg( glexec_whoami_arg.Value() );

	// debug info.  this display format totally screws up the quoting, but
	// popen gets it right.
	MyString disp_args;
	glexec_whoami_args.GetArgsStringForDisplay(&disp_args, 0);
	dprintf (D_ALWAYS, "GLEXEC: about to glexec: ** %s **\n",
			disp_args.Value());

	// the only thing actually needed by glexec at this point is the cert, so
	// that it knows who to map to.  the pipe outputs the username that glexec
	// ended up using, on a single text line by itself.
	SetEnv( "SSL_CLIENT_CERT", pem_str.Value() );
	FILE * glexec_pipe;
	glexec_pipe = my_popen(glexec_whoami_args, "r", FALSE);
	MyString glexec_username;
	glexec_username.readLine(glexec_pipe);
	my_pclose(glexec_pipe);

	// clean up, since there's private info in there.  i wish we didn't have to
	// put this in the environment to begin with, but alas, that's just how
	// glexec works.
	UnsetEnv( "SSL_CLIENT_CERT");


	// first strip off carriage return from the username
	glexec_username.setChar(glexec_username.Length()-1, '\0');
	dprintf (D_ALWAYS, "GLEXEC: got '%s' from my_popen.\n",
			glexec_username.Value());

	// update glexec_user_dir to include the condor.<startd_pid>.<username> dir
	glexec_user_dir.sprintf_cat("/condor.%d.%s", my_pid, glexec_username.Value());
	dprintf (D_ALWAYS, "GLEXEC: userdir is '%s'\n",
			glexec_user_dir.Value());

	// now prepare the starter command line, starting with glexec and its
	// options (if any).
	MyString err;
	if( ! glexec_args.AppendArgsV1RawOrV2Quoted( glexec_argstr,
						     &err ) ) {
		dprintf( D_ALWAYS,
		         "failed to parse GLEXEC from config: %s\n",
		         err.Value() );
		free( glexec_argstr );
		return 0;
	}
	free( glexec_argstr );

	// complete the command line by adding in the original
	// arguments. we also make sure that the full path to the
	// starter is given
	int starter_path_pos = glexec_args.Count();
	glexec_args.AppendArgsFromArgList( orig_args );
	glexec_args.RemoveArg( starter_path_pos );
	glexec_args.InsertArg( s_path, starter_path_pos );

	// set up the environment stuff
	if( orig_env ) {
		// first merge in the original
		glexec_env.MergeFrom( *orig_env );
	}

	// GLEXEC_MODE - get account from lcmaps
	glexec_env.SetEnv( "GLEXEC_MODE", "lcmaps_get_account" );

	// SSL_CLIENT_CERT - cert to use for the mapping
	glexec_env.SetEnv( "SSL_CLIENT_CERT", pem_str.Value() );

#if defined(GSI_AUTHENTICATION) && !defined(SKIP_AUTHENTICATION)
	// GLEXEC_SOURCE_PROXY -  proxy to provide to the child
	//                        (file is owned by us)
	glexec_env.SetEnv( "GLEXEC_SOURCE_PROXY", s_claim->client()->proxyFile() );
	dprintf (D_ALWAYS, "GLEXEC: setting GLEXEC_SOURCE_PROXY to %s\n",
		s_claim->client()->proxyFile());

	// GLEXEC_TARGET_PROXY - child-owned file to copy its proxy to.
	// this needs to be in a directory owned by that user, and not world
	// writable.  glexec enforces this.  hence, all the whoami/mkdir mojo
	// above.
	MyString child_proxy_file = glexec_user_dir;
	child_proxy_file += "/";
	child_proxy_file += condor_basename(s_claim->client()->proxyFile());
	child_proxy_file += ".starter";
	dprintf (D_ALWAYS, "GLEXEC: setting GLEXEC_TARGET_PROXY to %s\n",
		child_proxy_file.Value());
	glexec_env.SetEnv( "GLEXEC_TARGET_PROXY", child_proxy_file.Value() );

	// _CONDOR_GSI_DAEMON_PROXY - starter's proxy
	MyString var_name = "_CONDOR_";
	var_name += STR_GSI_DAEMON_PROXY;
	glexec_env.SetEnv( var_name.Value(), child_proxy_file.Value() );
#endif

	// the EXECUTE dir should be owned by the mapped user.  we created this
	// earlier, and now we override it in the condor_config via the
	// environment.
	MyString execute_dir = glexec_user_dir;
	execute_dir += "/execute";
	glexec_env.SetEnv ( "_CONDOR_EXECUTE", execute_dir.Value());

	// the LOG dir should be owned by the mapped user.  we created this
	// earlier, and now we override it in the condor_config via the
	// environment.
	MyString log_dir = glexec_user_dir;
	log_dir += "/log";
	glexec_env.SetEnv ( "_CONDOR_LOG", log_dir.Value());

	return true;
}

void
Starter::cleanupAfterGlexec()
{
	// remove the copy of the user proxy that we own
	// (the starter should remove the one glexec created for it)
	if( s_claim->client()->proxyFile() != NULL ) {
		remove( s_claim->client()->proxyFile() );
	}

	// should the starter clean up the StarterLog?  how about removing the
	// directories if the other VMs are not running any jobs by this user (a
	// nice potential race condition?)  right now, we litter.  however, the
	// same dirs get reused if/when the user runs another job.
	//
	// and, at this point, there's nothing the startd can do unless we invoke
	// glexec to clean up.
}
#endif // !WIN32

bool
Starter::isCOD()
{
	if( ! s_claim ) {
		return false;
	}
	return s_claim->isCOD();
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
	if( s_claim && s_claim->rip() ) {
		s_claim->rip()->dprintf_va( flags, fmt, args );
	} else {
		::_condor_dprintf_va( flags, fmt, args );
	}
	va_end( args );
}


float
Starter::percentCpuUsage( void )
{
	int status;

	time_t now = time(NULL);
	if( now - s_last_snapshot >= pid_snapshot_interval ) { 
		recomputePidFamily( now );
	}

	if( (DebugFlags & D_FULLDEBUG) && (DebugFlags & D_LOAD) ) {
		printPidFamily( D_FULLDEBUG, 
						"Computing percent CPU usage with pids: " );
	}

		// ProcAPI wants a non-const pointer reference, so we need
		// a temporary.
	procInfo *pinfoPTR = &s_pinfo;
	if( (ProcAPI::getProcSetInfo(s_pidfamily, s_family_size,
								 pinfoPTR, status) == PROCAPI_FAILURE) ) {

			// If we failed, it might be b/c our pid family has stale
			// info, so before we give up for real, recompute and try
			// once more.
		printPidFamily(D_FULLDEBUG, 
			"Starter::percentCpuUsage(): Failed trying to get cpu usage "
			"with this family: ");  

		recomputePidFamily();

		printPidFamily(D_FULLDEBUG, 
			"Starter::percentCpuUsage(): Maybe that family was stale, "
			"attempting recomputed family: ");  

		if( (ProcAPI::getProcSetInfo( s_pidfamily, s_family_size, 
									  pinfoPTR, status) == PROCAPI_FAILURE) ) {
			EXCEPT( "Starter::percentCpuUsage(): Fatal error getting process "
					"info for the starter and decendents" ); 
		}
	}
	if( (DebugFlags & D_FULLDEBUG) && (DebugFlags & D_LOAD) ) {
		dprintf( D_FULLDEBUG, "Starter::percentCpuUsage(): Percent CPU usage "
								"for those pids is: %f\n", s_pinfo.cpuusage );
	}
	return s_pinfo.cpuusage;
}


void
Starter::recomputePidFamily( time_t now ) 
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
Starter::printPidFamily( int dprintf_level, char* header ) 
{
	MyString msg;
	char numbuf[32];

	if( header ) {
		msg += header;
	}
	int i;
	for( i=0; i<s_family_size; i++ ) {
		snprintf( numbuf, 32, "%d ", s_pidfamily[i] );
		msg += numbuf;
	}
	dprintf( dprintf_level, "%s\n", msg.Value() );
}


unsigned long
Starter::imageSize( void )
{
		// we assume we're only asked for this after we've already
		// computed % cpu usage and we've already got this info
		// sitting here...
	return s_pinfo.imgsize;
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


char*
Starter::getIpAddr( void )
{
	if( ! s_pid ) {
		return NULL;
	}
	return daemonCore->InfoCommandSinfulString( s_pid );
}


bool
Starter::killHard( void )
{
	if( ! active() ) {
		return true;
	}
	if( ! kill(DC_SIGHARDKILL) ) {
		killpg( SIGKILL );
		return false;
	}
	startKillTimer();
	return true;
}


bool
Starter::killSoft( void )
{
	if( ! active() ) {
		return true;
	}
	if( ! kill(DC_SIGSOFTKILL) ) {
		killpg( SIGKILL );
		return false;
	}
	return true;
}


bool
Starter::suspend( void )
{
	if( ! active() ) {
		return true;
	}
	if( ! kill(DC_SIGSUSPEND) ) {
		killpg( SIGKILL );
		return false;
	}
	return true;
}


bool
Starter::resume( void )
{
	if( ! active() ) {
		return true;
	}
	if( ! kill(DC_SIGCONTINUE) ) {
		killpg( SIGKILL );
		return false;
	}
	return true;
}


int
Starter::startKillTimer( void )
{
	if( s_kill_tid >= 0 ) {
			// Timer already started.
		return TRUE;
	}
	s_kill_tid = 
		daemonCore->Register_Timer( killing_timeout,
						0, 
						(TimerHandlercpp)&Starter::sigkillStarter,
						"sigkillStarter", this );
	if( s_kill_tid < 0 ) {
		EXCEPT( "Can't register DaemonCore timer" );
	}
	return TRUE;
}


void
Starter::cancelKillTimer( void )
{
	int rval;
	if( s_kill_tid != -1 ) {
		rval = daemonCore->Cancel_Timer( s_kill_tid );
		if( rval < 0 ) {
			dprintf( D_ALWAYS, 
					 "Failed to cancel hardkill-starter timer (%d): "
					 "daemonCore error\n", s_kill_tid );
		} else {
			dprintf( D_FULLDEBUG, "Canceled hardkill-starter timer (%d)\n",
					 s_kill_tid );
		}
		s_kill_tid = -1;
	}
}


bool
Starter::sigkillStarter( void )
{
		// Now that the timer has gone off, clear out the tid.
	s_kill_tid = -1;

	if( active() ) {
		dprintf( D_ALWAYS, "starter (pid %d) is not responding to the "
				 "request to hardkill its job.  The startd will now "
				 "directly hard kill the starter and all its "
				 "decendents.\n", s_pid );

			// Kill all of the starter's children.
		killkids( SIGKILL );

			// Kill the starter's entire process group.
		return killpg( SIGKILL );
	}
	return true;
}

