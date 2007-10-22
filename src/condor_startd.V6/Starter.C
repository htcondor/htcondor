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
	if( s.s_claim || s.s_pid || s.s_birthdate ||
	    s.s_port1 >= 0 || s.s_port2 >= 0 )
	{
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
	s_birthdate = 0;
	s_kill_tid = -1;
	s_port1 = -1;
	s_port2 = -1;
	s_reaper_id = -1;

#if HAVE_BOINC
	s_is_boinc = false;
#endif /* HAVE_BOINC */
}


Starter::~Starter()
{
	cancelKillTimer();

	if (s_path) {
		delete [] s_path;
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
Starter::setPath( const char* updated_path )
{
	if( s_path ) {
		delete [] s_path;
	}
	s_path = strnewp( updated_path );
}

void
Starter::setIsDC( bool updated_is_dc )
{
	s_is_dc = updated_is_dc;
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
			EXCEPT( "stat(%s) failed with unexpected errno %d (%s)", 
					s_path, errno, strerror( errno ) );
		}
	}
#endif	// if !defined(WIN32)

	if( is_dc() ) {
			// With DaemonCore, fow now convert a request to kill a
			// process group into a request to kill a process family via
			// DaemonCore
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
		EXCEPT( "Unknown type (%d) in Starter::reallykill\n", type );
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
		if (daemonCore->Kill_Family(s_pid) == FALSE) {
			dprintf(D_ALWAYS,
			        "error killing process family of starter with pid %u\n",
				s_pid);
		}
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
		s_birthdate = now;
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
	if (daemonCore->Kill_Family(s_pid) == FALSE) {
		dprintf(D_ALWAYS,
		        "error killing process family of starter with pid %u\n",
		        s_pid);
	}

		// Now, delete any files lying around.
	ASSERT( s_claim && s_claim->rip() );
	cleanup_execute_dir( s_pid, s_claim->rip()->executeDir() );

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

	FamilyInfo fi;
	fi.max_snapshot_interval = pid_snapshot_interval;
	fi.login = NULL;

	s_pid = daemonCore->
		Create_Process( final_path, *final_args, PRIV_ROOT, reaper_id,
		                TRUE, final_env, NULL, &fi, inherit_list, std_fds );
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
	// don't allow this if GLEXEC_STARTER is true
	//
	if( param_boolean( "GLEXEC_STARTER", false ) ) {
		// we don't support using glexec to spawn the old starter
		dprintf( D_ALWAYS,
			 "error: can't spawn starter %s via glexec\n",
			 s_path );
		return 0;
	}

	// set up the standard file descriptors
	//
	int std[3];
	std[0] = s_port1;
	std[1] = s_port1;
	std[2] = s_port2;

	// set up the starter's arguments
	//
	ArgList args;
	args.AppendArg("condor_starter");
	args.AppendArg(s_claim->client()->host());
	args.AppendArg(daemonCore->InfoCommandSinfulString());
	if (resmgr->is_smp()) {
		args.AppendArg("-a");
		args.AppendArg(s_claim->rip()->r_id_str);
	}

	// set up the signal mask (block everything, which is what the old
	// starter expects)
	//
	sigset_t full_mask;
	sigfillset(&full_mask);

	// set up a structure for telling DC to track the starter's process
	// family
	//
	FamilyInfo family_info;
	family_info.max_snapshot_interval = pid_snapshot_interval;
	family_info.login = NULL;

	// HISTORICAL COMMENTS:
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
			 * Don't create a new process group if the config file says
			 * not to.
			 */
	// END of HISTORICAL COMMENTS
	//
	// Create_Process now will param for USE_PROCESS_GROUPS internally and
	// call setsid if needed.

	// call Create_Process
	//
	int new_pid = daemonCore->Create_Process(s_path,       // path to binary
	                                         args,         // arguments
	                                         PRIV_ROOT,    // start as root
	                                         main_reaper,  // reaper
	                                         FALSE,        // no command port
	                                         NULL,         // inherit our env
	                                         NULL,         // inherit out cwd
	                                         &family_info, // new family
	                                         NULL,         // no inherited sockets
	                                         std,          // std FDs
	                                         NULL,         // no inherited FDs
	                                         0,            // zero nice inc
	                                         &full_mask,   // signal mask
	                                         0);           // DC job opts

	// clean up the "ports"
	//
	(void)close(s_port1);
	(void)close(s_port2);

	// check for error
	//
	if (new_pid == FALSE) {
		dprintf(D_ALWAYS, "execOldStarter: Create_Process error\n");
		return 0;
	}

	return new_pid;
#endif
}

#if !defined(WIN32)
bool
Starter::prepareForGlexec( const ArgList& orig_args, const Env* orig_env,
                           ArgList& glexec_args, Env& glexec_env )
{
	// if GLEXEC_STARTER is set, use glexec to invoke the
	// starter (or fail if we can't). this involves:
	//   - verifying that we have a delegated proxy from
	//     the user stored, since we need to hand it to
	//     glexec so it can look up the UID/GID
	//   - invoking 'glexec_starter_setup.sh' via glexec to
	//     setup the starter's "private directory" for a copy
	//     of the job's proxy to go into, as well as the StarterLog
	//     and execute dir
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
	FILE* fp = safe_fopen_wrapper( s_claim->client()->proxyFile(), "r" );
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

	// using the file name of the proxy that was stashed ealier, construct
	// the name of the starter's "private directory". the naming scheme is
	// (where XXXXXX is randomly generated via condor_mkstemp):
	//   - $(GLEXEC_USER_DIR)/startd-tmp-proxy-XXXXXX
	//       - startd's copy of the job's proxy
	//   - $(GLEXEC_USER_DIR)/starter-tmp-dir-XXXXXX
	//       - starter's private dir
	//
	MyString glexec_private_dir;
	char* dir_part = condor_dirname(s_claim->client()->proxyFile());
	ASSERT(dir_part != NULL);
	glexec_private_dir = dir_part;
	free(dir_part);
	glexec_private_dir += "/starter-tmp-dir-";
	char* random_part = s_claim->client()->proxyFile();
	random_part += strlen(random_part) - 6;
	glexec_private_dir += random_part;
	dprintf(D_ALWAYS,
	        "GLEXEC: starter private dir is '%s'\n",
	        glexec_private_dir.Value());

	// get the glexec command line prefix from config
	char* glexec_argstr = param( "GLEXEC" );
	if ( ! glexec_argstr ) {
		dprintf( D_ALWAYS,
		         "cannot use glexec to spawn starter: "
		         "GLEXEC not given in config\n" );
		return false;
	}

	// cons up a command line for my_system. we'll run the
	// script $(LIBEXEC)/glexec_starter_setup.sh, which
	// will create the starter's "private directory" (and
	// its log and execute subdirectories). the value of
	// glexec_private_dir will be passed as an argument to
	// the script

	// parse the glexec args for invoking glexec_starter_setup.sh.
	// do not free them yet, except on an error, as we use them
	// again below.
	MyString setup_err;
	ArgList  glexec_setup_args;
	glexec_setup_args.SetArgV1SyntaxToCurrentPlatform();
	if( ! glexec_setup_args.AppendArgsV1RawOrV2Quoted( glexec_argstr,
	                                                   &setup_err ) ) {
		dprintf( D_ALWAYS,
		         "GLEXEC: failed to parse GLEXEC from config: %s\n",
		         setup_err.Value() );
		free( glexec_argstr );
		return 0;
	}

	// set up the rest of the arguments for the glexec setup script
	char* libexec = param("LIBEXEC");
	if (libexec == NULL) {
		dprintf( D_ALWAYS,
		         "GLEXEC: LIBEXEC not defined; can't find setup script\n" );
		free( glexec_argstr );
	}
	MyString setup_script = libexec;
	free(libexec);
	setup_script += "/glexec_starter_setup.sh";
	glexec_setup_args.AppendArg(setup_script.Value());
	glexec_setup_args.AppendArg(glexec_private_dir.Value());

	// debug info.  this display format totally screws up the quoting, but
	// my_system gets it right.
	MyString disp_args;
	glexec_setup_args.GetArgsStringForDisplay(&disp_args, 0);
	dprintf (D_ALWAYS, "GLEXEC: about to glexec: ** %s **\n",
			disp_args.Value());

	// the only thing actually needed by glexec at this point is the cert, so
	// that it knows who to map to.  the pipe outputs the username that glexec
	// ended up using, on a single text line by itself.
	SetEnv( "SSL_CLIENT_CERT", pem_str.Value() );

	// create the starter's private dir
	int ret = my_system(glexec_setup_args);

	// clean up, since there's private info in there.  i wish we didn't have to
	// put this in the environment to begin with, but alas, that's just how
	// glexec works.
	UnsetEnv( "SSL_CLIENT_CERT");

	if ( ret != 0 ) {
		dprintf(D_ALWAYS,
		        "GLEXEC: error creating private dir: my_system returned %d\n",
		        ret);
		free( glexec_argstr );
		return 0;
	}

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

#if defined(HAVE_EXT_GLOBUS) && !defined(SKIP_AUTHENTICATION)
	// GLEXEC_SOURCE_PROXY -  proxy to provide to the child
	//                        (file is owned by us)
	glexec_env.SetEnv( "GLEXEC_SOURCE_PROXY", s_claim->client()->proxyFile() );
	dprintf (D_ALWAYS, "GLEXEC: setting GLEXEC_SOURCE_PROXY to %s\n",
		s_claim->client()->proxyFile());

	// GLEXEC_TARGET_PROXY - child-owned file to copy its proxy to.
	// this needs to be in a directory owned by that user, and not world
	// writable.  glexec enforces this.  hence, all the whoami/mkdir mojo
	// above.
	MyString child_proxy_file = glexec_private_dir;
	child_proxy_file += "/glexec_starter_proxy";
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
	MyString execute_dir = glexec_private_dir;
	execute_dir += "/execute";
	glexec_env.SetEnv ( "_CONDOR_EXECUTE", execute_dir.Value());

	// the LOG dir should be owned by the mapped user.  we created this
	// earlier, and now we override it in the condor_config via the
	// environment.
	MyString log_dir = glexec_private_dir;
	log_dir += "/log";
	glexec_env.SetEnv ( "_CONDOR_LOG", log_dir.Value());

	// PROCD_ADDRESS: the Starter that we are about to create will
	// not have access to our ProcD. we'll explicitly set PROCD_ADDRESS
	// to be in its LOG directory. the Starter will see that its
	// PROCD_ADDRESS knob is different from what it inherits in
	// CONDOR_PROCD_ADDRESS, and know it needs to create its own ProcD
	//
	MyString procd_address = log_dir;
	procd_address += "/procd_pipe";
	glexec_env.SetEnv( "_CONDOR_PROCD_ADDRESS", procd_address.Value() );

	return true;
}

void
Starter::cleanupAfterGlexec()
{
	// remove the copy of the user proxy that we own
	// (the starter should remove the one glexec created for it)
	if( s_claim->client()->proxyFile() != NULL ) {
		if ( unlink( s_claim->client()->proxyFile() ) == -1) {
			dprintf( D_ALWAYS,
			         "error removing temporary proxy %s: %s (%d)\n",
			         s_claim->client()->proxyFile(),
			         strerror(errno),
			         errno );
		}
		s_claim->client()->setProxyFile(NULL);
	}
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
	if (daemonCore->Get_Family_Usage(s_pid, s_usage, true) == FALSE) {
		EXCEPT( "Starter::percentCpuUsage(): Fatal error getting process "
		        "info for the starter and decendents" ); 
	}

	// In vm universe, there may be a process dealing with a VM.
	// Unfortunately, such a process is created by a specific 
	// running daemon for virtual machine program(such as vmware-serverd).
	// So our Get_Family_Usage is unable to get usage for the process.
	// Here, we call a function in vm universe manager.
	// If this starter is for vm universe, s_usage will be updated.
	// Otherwise, s_usage will not change.
	if ( resmgr ) {
		resmgr->m_vmuniverse_mgr.getUsageForVM(s_pid, s_usage);
	}

	if( (DebugFlags & D_FULLDEBUG) && (DebugFlags & D_LOAD) ) {
		dprintf( D_FULLDEBUG,
		        "Starter::percentCpuUsage(): Percent CPU usage "
		        "for the family of starter with pid %u is: %f\n",
		        s_pid,
		        s_usage.percent_cpu );
	}

	return s_usage.percent_cpu;
}

unsigned long
Starter::imageSize( void )
{
		// we assume we're only asked for this after we've already
		// computed % cpu usage and we've already got this info
		// sitting here...
	return s_usage.total_image_size;
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

	int tmp_killing_timeout = killing_timeout;
	if( s_claim && (s_claim->universe() == CONDOR_UNIVERSE_VM) ) {
		// For vm universe, we need longer killing_timeout
		int vm_killing_timeout = param_integer( "VM_KILLING_TIMEOUT", 60);
		if( killing_timeout < vm_killing_timeout ) {
			tmp_killing_timeout = vm_killing_timeout;
		}
	}

	s_kill_tid = 
		daemonCore->Register_Timer( tmp_killing_timeout,
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

