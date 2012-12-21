/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

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
#include "dc_starter.h"
#include "classadHistory.h"
#include "classad_helpers.h"
#include "ipv6_hostname.h"

#if defined(LINUX)
#include "glexec_starter.linux.h"
#endif

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
	: Service( s )
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
	s_softkill_tid = -1;
	s_port1 = -1;
	s_port2 = -1;
	s_reaper_id = -1;

#if HAVE_BOINC
	s_is_boinc = false;
#endif /* HAVE_BOINC */
	s_job_update_sock = NULL;

	m_hold_job_cb = NULL;

		// XXX: ProcFamilyUsage needs a constructor
	s_usage.max_image_size = 0;
	s_usage.num_procs = 0;
	s_usage.percent_cpu = 0.0;
	s_usage.sys_cpu_time = 0;
	s_usage.total_image_size = 0;
	s_usage.user_cpu_time = 0;
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
	if( s_job_update_sock ) {
		daemonCore->Cancel_Socket( s_job_update_sock );
		delete s_job_update_sock;
	}

	if( m_hold_job_cb ) {
		m_hold_job_cb->cancelCallback();
		m_hold_job_cb = NULL;
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
		dprintf( D_ALWAYS, "Failed to find requirements in merged ad?\n" );
		classad::PrettyPrint pp;
		std::string szbuff;
		pp.Unparse(szbuff,job_ad);
		dprintf( D_ALWAYS, "job_ad\n%s\n",szbuff.c_str());
		pp.Unparse(szbuff,merged_ad);
		dprintf( D_ALWAYS, "merged_ad\n%s\n",szbuff.c_str());
		
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

	ExprTree *tree, *pCopy;
	const char *lhstr = NULL;
	s_ad->ResetExpr();
	while( s_ad->NextExpr(lhstr, tree) ) {
		pCopy=0;
	
		if (ignored_attr_list) {
				// insert every attr that's not in the ignored_attr_list
			if (!ignored_attr_list->contains(lhstr)) {
				pCopy = tree->Copy();
				ad->Insert(lhstr, pCopy, false);
				if (strncasecmp(lhstr, "Has", 3) == MATCH) {
					list->append(lhstr);
				}
			}
		}
		else {
				// no list of attrs to ignore - fallback on old behavior
			if( strncasecmp(lhstr, "Has", 3) == MATCH ) {
				pCopy = tree->Copy();
				ad->Insert( lhstr, pCopy, false );
				if( list ) {
					list->append( lhstr );
				}
			} else if( strncasecmp(lhstr, "Java", 4) == MATCH ) {
				pCopy = tree->Copy();
				ad->Insert( lhstr, pCopy, false);
			}
		}
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
	const char *signame = "";

	if ( s_pid == 0 ) {
			// If there's no starter, just return.  We've done our task.
		return true;
	}

	switch( signo ) {
	case DC_SIGSUSPEND:
		signame = "DC_SIGSUSPEND";
		break;
	case DC_SIGHARDKILL:
		signo = SIGQUIT;
		signame = "SIGQUIT";
		break;
	case DC_SIGSOFTKILL:
		signo = SIGTERM;
		signame = "SIGTERM";
		break;
	case DC_SIGPCKPT:
		signame = "DC_SIGPCKPT";
		break;
	case DC_SIGCONTINUE:
		signame = "DC_SIGCONTINUE";
		break;
	case SIGHUP:
		signame = "SIGHUP";
		break;
	case SIGKILL:
		signame = "SIGKILL";
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
#if defined(Darwin) || defined(CONDOR_FREEBSD)
				// dux 4.0 doesn't have ENOLINK for stat().  It does
				// have ESTALE, which means our binaries live on a
				// stale NFS mount.  So, we can at least EXCEPT with a
				// more specific error message.
		case ESTALE:
			(void)first_time; // Shut the compiler up
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
		break;
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
			} else if( type == 0 && 
						!daemonCore->ProcessExitedButNotReaped(s_pid)) 
			{
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
			if (!daemonCore->ProcessExitedButNotReaped(s_pid)) {
				ret = ::kill( (s_pid), sig );
			}
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
			ret = -1;
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

void
Starter::finalizeExecuteDir()
{
		// If setExecuteDir() has already been called (e.g. BOINC), then
		// do nothing.  Otherwise, choose the execute dir now.
	if( !executeDir() ) {
		ASSERT( s_claim && s_claim->rip() );
		setExecuteDir( s_claim->rip()->executeDir() );
	}
}

char const *
Starter::executeDir()
{
	return s_execute_dir.Length() ? s_execute_dir.Value() : NULL;
}

int 
Starter::spawn( time_t now, Stream* s )
{
		// if execute dir has not been set, choose one now
	finalizeExecuteDir();

	if (claimType() == CLAIM_COD) {
		s_pid = execJobPipeStarter();
	}
#if HAVE_JOB_HOOKS
	else if (claimType() == CLAIM_FETCH) {
		s_pid = execJobPipeStarter();
	}
#endif /* HAVE_JOB_HOOKS */
#if HAVE_BOINC
	else if( isBOINC() ) {
		s_pid = execBOINCStarter(); 
	}
#endif /* HAVE_BOINC */
	else if( is_dc() ) {
		s_pid = execDCStarter( s ); 
	}
	else {
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
Starter::exited(int status)
{
	ClassAd *jobAd = NULL;
	bool jobAdNeedsFree = true;

	if (s_claim && s_claim->ad()) {
		// real jobs in the startd have claims and job ads, boinc and perhaps others won't
		jobAd = s_claim->ad();
		jobAdNeedsFree = false;
	} else {
		// Dummy up an ad
		int now = (int) time(0);
		jobAd = new ClassAd();
		jobAd->SetMyTypeName("Job");
		jobAd->SetTargetTypeName("Machine");
		jobAd->Assign(ATTR_CLUSTER_ID, now);
		jobAd->Assign(ATTR_PROC_ID, 1);
		jobAd->Assign(ATTR_OWNER, "boinc");
		jobAd->Assign(ATTR_Q_DATE, (int)s_birthdate);
		jobAd->Assign(ATTR_JOB_PRIO, 0);
		jobAd->Assign(ATTR_IMAGE_SIZE, 0);
		jobAd->Assign(ATTR_JOB_CMD, "boinc");
		MyString gjid;
		gjid.formatstr("%s#%d#%d#%d", get_local_hostname().Value(), 
					 now, 1, now);
		jobAd->Assign(ATTR_GLOBAL_JOB_ID, gjid);
	}

	// First, patch up the ad a little bit 
	jobAd->Assign(ATTR_COMPLETION_DATE, (int)time(0));
	int runtime = time(0) - s_birthdate;
	
	jobAd->Assign(ATTR_JOB_REMOTE_WALL_CLOCK, runtime);
	int jobStatus = COMPLETED;
	if (WIFSIGNALED(status)) {
		jobStatus = REMOVED;
	}
	jobAd->Assign(ATTR_JOB_STATUS, jobStatus);
	AppendHistory(jobAd);
	WritePerJobHistoryFile(jobAd, true /* use gjid for filename*/);

	if (jobAdNeedsFree) {
		delete jobAd;
	}

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
	ASSERT( executeDir() );
	cleanup_execute_dir( s_pid, executeDir() );

#if defined(LINUX)
	if( param_boolean( "GLEXEC_STARTER", false ) ) {
		cleanupAfterGlexec();
	}
#endif
}


int
Starter::execJobPipeStarter( void )
{
	int rval;
	MyString lock_env;
	ArgList args;
	Env env;
	char* tmp;

	if (s_claim->type() == CLAIM_COD) {
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
	}

		// Create an argument list for this starter, based on the claim.
	s_claim->makeStarterArgs(args);

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
Starter::receiveJobClassAdUpdate( Stream *stream )
{
	ClassAd update_ad;
	int final_update = 0;

		// It is expected that we will get here when the stream is closed.
		// Unfortunately, log noise will be generated when we try to read
		// from it.

	stream->decode();
	if( !stream->get( final_update) ||
		!update_ad.initFromStream( *stream ) ||
		!stream->end_of_message() )
	{
		final_update = 1;
	}
	else {
		dprintf(D_FULLDEBUG, "Received job ClassAd update from starter.\n");
		update_ad.dPrint( D_JOB );

		// In addition to new info about the job, the starter also
		// inserts contact info for itself (important for CCB and
		// shadow-starter reconnect, because startd needs to relay
		// starter's full contact info to the shadow when queried).
		// It's a bit of a hack to do it through this channel, but
		// better than nothing.
		update_ad.LookupString(ATTR_STARTER_IP_ADDR,m_starter_addr);

		if( s_claim ) {
			s_claim->receiveJobClassAdUpdate( update_ad );
		}
	}

	if( final_update ) {
		dprintf(D_FULLDEBUG, "Closing job ClassAd update socket from starter.\n");
		daemonCore->Cancel_Socket(s_job_update_sock);
		delete s_job_update_sock;
		s_job_update_sock = NULL;
	}
	return KEEP_STREAM;
}

int
Starter::execDCStarter( ArgList const &args, Env const *env, 
						int* std_fds, Stream* s )
{
	Stream *inherit_list[] =
		{ 0 /*ClassAd update stream (assigned below)*/,
		  s /*shadow syscall sock*/,
		  0 /*terminal NULL*/ };

	const ArgList* final_args = &args;
	const char* final_path = s_path;
	Env new_env;

	if( env ) {
		new_env.MergeFrom( *env );
	}

		// The starter figures out its execute directory by paraming
		// for EXECUTE, which we override in the environment here.
		// This way, all the logic about choosing a directory to use
		// is in only one place.
	ASSERT( executeDir() );
	new_env.SetEnv( "_CONDOR_EXECUTE", executeDir() );

	env = &new_env;


		// Build the affinity string to pass to the starter via env

	std::string affinityString;
	if (s_claim && s_claim->rip() && s_claim->rip()->get_affinity_set()) {
		std::list<int> *l = s_claim->rip()->get_affinity_set();
		bool needComma = false;
		for (std::list<int>::iterator it = l->begin(); it != l->end(); it++) {
			if (needComma) {
				formatstr_cat(affinityString, ", %d", *it);
			} else {
				formatstr_cat(affinityString, "%d ", *it);
				needComma = true;
			}
		}
	}

	int slotId = s_claim->rip()->r_sub_id;
	if (slotId == 0) {
		// Isn't a paritionable slot, use the main id
		slotId = s_claim->rip()->r_id;
	}
	std::string affinityKnob;
	formatstr(affinityKnob, "_CONDOR_SLOT%d_CPU_AFFINITY",  slotId);

	if (param_boolean("ASSIGN_CPU_AFFINITY", false)) {
		new_env.SetEnv(affinityKnob.c_str(), affinityString.c_str());
		new_env.SetEnv("_CONDOR_ENFORCE_CPU_AFFINITY", "true");
		dprintf(D_FULLDEBUG, "Setting affinity env to %s = %s\n", affinityKnob.c_str(), affinityString.c_str());
	}


	ReliSock child_job_update_sock;   // child inherits this socket
	ASSERT( !s_job_update_sock );
	s_job_update_sock = new ReliSock; // parent (yours truly) keeps this socket
	ASSERT( s_job_update_sock );

		// Connect parent and child sockets together so child can send us
		// udpates to the job ClassAd.
	if( !s_job_update_sock->connect_socketpair( child_job_update_sock ) ) {
		dprintf( D_ALWAYS, "ERROR: Failed to create job ClassAd update socket!\n");
		s_pid = 0;
		return s_pid;
	}
	inherit_list[0] = &child_job_update_sock;

	// Pass the machine ad to the starter
	if (s_claim) 
		s_claim->writeMachAd( s_job_update_sock );

	if( daemonCore->Register_Socket(
			s_job_update_sock,
			"starter ClassAd update socket",
			(SocketHandlercpp)&Starter::receiveJobClassAdUpdate,
			"receiveJobClassAdUpdate",
			this) < 0 )
	{
		EXCEPT("Failed to register ClassAd update socket.");
	}

#if defined(LINUX)
	// see if we should be using glexec to spawn the starter.
	// if we are, the cmd, args, env, and stdin to use will be
	// modified
	ArgList glexec_args;
	Env glexec_env;
	int glexec_std_fds[3];
	if( param_boolean( "GLEXEC_STARTER", false ) ) {
		if( ! glexec_starter_prepare( s_path,
		                              s_claim->client()->proxyFile(),
		                              args,
		                              env,
		                              std_fds,
		                              glexec_args,
		                              glexec_env,
		                              glexec_std_fds ) )
		{
			// something went wrong; prepareForGlexec will
			// have already logged it
			cleanupAfterGlexec();
			return 0;
		}
		final_path = glexec_args.GetArg(0);
		final_args = &glexec_args;
		env = &glexec_env;
		std_fds = glexec_std_fds;
	}
#endif
								   
	int reaper_id;
	if( s_reaper_id > 0 ) {
		reaper_id = s_reaper_id;
	} else {
		reaper_id = main_reaper;
	}

	if(IsFulldebug(D_FULLDEBUG)) {
		MyString args_string;
		final_args->GetArgsStringForDisplay(&args_string);
		dprintf( D_FULLDEBUG, "About to Create_Process \"%s\"\n",
				 args_string.Value() );
	}

	FamilyInfo fi;
	fi.max_snapshot_interval = pid_snapshot_interval;

	s_pid = daemonCore->
		Create_Process( final_path, *final_args, PRIV_ROOT, reaper_id,
		                TRUE, env, NULL, &fi, inherit_list, std_fds );
	if( s_pid == FALSE ) {
		dprintf( D_ALWAYS, "ERROR: exec_starter failed!\n");
		s_pid = 0;
	}

#if defined(LINUX)
	if( param_boolean( "GLEXEC_STARTER", false ) ) {
		// if we used glexec to spawn the Starter, we now need to send
		// the Starter's environment to our glexec wrapper script so it
		// can exec the Starter with all the environment variablew we rely
		// on it inheriting
		//
		if ( !glexec_starter_handle_env(s_pid) ) {
			// something went wrong; handleGlexecEnvironment will
			// have already logged it
			cleanupAfterGlexec();
			return 0;
		}
	}
#endif

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

	Env env;

		// The starter figures out its execute directory by paraming
		// for EXECUTE, which we override in the environment here.
		// This way, all the logic about choosing a directory to use
		// is in only one place.
	ASSERT( executeDir() );
	env.SetEnv( "_CONDOR_EXECUTE", executeDir() );

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
	                                         &env,
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

#if defined(LINUX)
void
Starter::cleanupAfterGlexec()
{
	// remove the copy of the user proxy that we own
	// (the starter should remove the one glexec created for it)
	if( s_claim && s_claim->client()->proxyFile() != NULL ) {
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
#endif

ClaimType
Starter::claimType()
{
	if( ! s_claim ) {
		return CLAIM_NONE;
	}
	return s_claim->type();
}


bool
Starter::active()
{
	return( (s_pid != 0) );
}
	

void
Starter::dprintf( int flags, const char* fmt, ... )
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
		
		// Now try to tack on details posted in the job ad because 
		// hypervisors such as libvirt will spawn processes outside 
		// of condor's perview. 
		float fPercentCPU=0.0;
		int iNumCPUs=0;
		ClassAd * jobAd = s_claim->ad();
		
		jobAd->LookupFloat(ATTR_JOB_VM_CPU_UTILIZATION, fPercentCPU);
		jobAd->LookupInteger(ATTR_JOB_VM_VCPUS, iNumCPUs);
		
		// computations outside take cores into account.
		fPercentCPU = fPercentCPU * iNumCPUs;
		
		dprintf( D_LOAD, "Starter::percentCpuUsage() adding VM Utilization %f\n",fPercentCPU);
		
		s_usage.percent_cpu += fPercentCPU;
		
	}

	if( IsDebugVerbose(D_LOAD) ) {
		dprintf(D_LOAD,
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


char const*
Starter::getIpAddr( void )
{
	if( ! s_pid ) {
		return NULL;
	}
	if( !m_starter_addr.IsEmpty() ) {
		return m_starter_addr.Value();
	}

	// Fall back on the raw contact string known to daemonCore.
	// Unfortunately, that doesn't include any of the fancy
	// stuff like private network info and CCB.
	dprintf(D_ALWAYS,
			"Warning: giving raw address in response to starter address query,"
			"because update from starter not received yet.\n");

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
	dprintf(D_FULLDEBUG, "in starter:killHard starting kill timer\n");
	startKillTimer();	

	return true;
}


bool
Starter::killSoft( bool /*state_change*/ )
{
	if( ! active() ) {
		return true;
	}
	if( ! kill(DC_SIGSOFTKILL) ) {
		killpg( SIGKILL );
		return false;
	}

		// If this soft-kill was triggered by entering the preempting
		// state, state_change will be true.  We _could_ take the
		// stance that a different timeout should apply in that case
		// vs. other cases, such as condor_vacate_job.  However, for
		// simplicity, we currently treat all cases with the same max
		// vacate timeout.  Not having a timeout at all for some cases
		// would require some care.  For example, it was observed in
		// the past when there was no timeout that a job which ignored
		// the soft-kill signal from condor_vacate_job was then
		// immune to preemption.

	startSoftkillTimeout();

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

		// Create a periodic timer so that if the kill attempt fails,
		// we keep trying.
	s_kill_tid = 
		daemonCore->Register_Timer( tmp_killing_timeout,
									std::max(1,tmp_killing_timeout),
						(TimerHandlercpp)&Starter::sigkillStarter,
						"sigkillStarter", this );
	if( s_kill_tid < 0 ) {
		EXCEPT( "Can't register DaemonCore timer" );
	}
	return TRUE;
}


int
Starter::startSoftkillTimeout( void )
{
	if( s_softkill_tid >= 0 ) {
			// Timer already started.
		return TRUE;
	}

	int softkill_timeout = s_claim ? s_claim->rip()->evalMaxVacateTime() : 0;
	if( softkill_timeout < 0 ) {
		softkill_timeout = 0;
	}

	s_softkill_tid = 
		daemonCore->Register_Timer( softkill_timeout,
						(TimerHandlercpp)&Starter::softkillTimeout,
						"softkillTimeout", this );
	if( s_softkill_tid < 0 ) {
		EXCEPT( "Can't register softkillTimeout timer" );
	}
	dprintf(D_FULLDEBUG,"Using max vacate time of %ds for this job.\n",softkill_timeout);
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
	if( s_softkill_tid != -1 ) {
		rval = daemonCore->Cancel_Timer( s_softkill_tid );
		if( rval < 0 ) {
			dprintf( D_ALWAYS, 
					 "Failed to cancel softkill-starter timer (%d): "
					 "daemonCore error\n", s_softkill_tid );
		} else {
			dprintf( D_FULLDEBUG, "Canceled softkill-starter timer (%d)\n",
					 s_softkill_tid );
		}
		s_softkill_tid = -1;
	}
}


void
Starter::sigkillStarter( void )
{
		// In case the kill fails for some reason, we are on a periodic
		// timer that will keep trying.

	if( active() ) {
		dprintf( D_ALWAYS, "starter (pid %d) is not responding to the "
				 "request to hardkill its job.  The startd will now "
				 "directly hard kill the starter and all its "
				 "decendents.\n", s_pid );

			// Kill all of the starter's children.
		killkids( SIGKILL );

			// Kill the starter's entire process group.
		killpg( SIGKILL );
	}
}

void
Starter::softkillTimeout( void )
{
	s_softkill_tid = -1;
	if( active() ) {
		dprintf( D_ALWAYS, "max vacate time expired.  Escalating to a fast shutdown of the job.\n" );
		killHard();
	}
}

bool
Starter::holdJob(char const *hold_reason,int hold_code,int hold_subcode,bool soft)
{
	if( !s_is_dc ) {
		return false;  // this starter does not support putting jobs on hold
	}
	if( m_hold_job_cb ) {
		dprintf(D_ALWAYS,"holdJob() called when operation already in progress (starter pid %d).\n", s_pid);
		return true;
	}

	classy_counted_ptr<DCStarter> starter = new DCStarter(getIpAddr());
	classy_counted_ptr<StarterHoldJobMsg> msg = new StarterHoldJobMsg(hold_reason,hold_code,hold_subcode,soft);

	m_hold_job_cb = new DCMsgCallback( (DCMsgCallback::CppFunction)&Starter::holdJobCallback, this );

	msg->setCallback( m_hold_job_cb );

	starter->sendMsg(msg.get());

	if( soft ) {
		startSoftkillTimeout();
	}
	else {
		startKillTimer();
	}

	return true;
}

void
Starter::holdJobCallback(DCMsgCallback *cb)
{
	ASSERT( m_hold_job_cb == cb );
	m_hold_job_cb = NULL;

	ASSERT( cb->getMessage() );
	if( cb->getMessage()->deliveryStatus() != DCMsg::DELIVERY_SUCCEEDED ) {
		dprintf(D_ALWAYS,"Failed to hold job (starter pid %d), so killing it.\n", s_pid);
		killSoft();
	}
}
