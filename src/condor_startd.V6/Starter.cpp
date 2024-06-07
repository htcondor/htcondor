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
#include "setenv.h"
#include "my_popen.h"
#include "basename.h"
#include "dc_starter.h"
#include "classadHistory.h"
#include "classad_helpers.h"
#include "ipv6_hostname.h"
#include "shared_port_endpoint.h"

#define SANDBOX_STARTER_LOG_FILENAME ".starter.log"

ClassAd* Starter::s_ad = nullptr; // starter capabilities ad, (not the job ad!)
char* Starter::s_path = nullptr;

// Keep track of living Starters
std::map<pid_t, Starter*> living_starters;

Starter *findStarterByPid(pid_t pid)
{
	if ( ! pid) return NULL;
	auto found = living_starters.find(pid);
	if (found != living_starters.end()) {
		return found->second;
	}
	return NULL;
}




Starter::Starter()
	: s_orphaned_jobad(NULL)
{
	initRunData();
}


void
Starter::initRunData( void ) 
{
	s_pid = 0;		// pid_t can be unsigned, so use 0, not -1
	s_birthdate = 0;
	s_last_update_time = 0;
	s_got_final_update = false;
	s_kill_tid = -1;
	s_softkill_tid = -1;
	s_hold_timeout = -1;
	s_reaper_id = -1;
	s_exit_status = 0;
	setOrphanedJob(NULL);
	s_was_reaped = false;
	s_created_execute_dir = false;
	s_is_vm_universe = false;
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
	s_vm_cpu_usage = 0; // extra cpu percentage from VM processes
	s_num_vm_cpus = 1;
}

// when a claim associated with this starter is destroyed before the starter is reaped
// it will hand off the job ad so that we can write correct things into the history file
// when the starter is finally reaped.
void Starter::setOrphanedJob(ClassAd * job)
{
	if (s_orphaned_jobad && (job != s_orphaned_jobad)) {
		delete s_orphaned_jobad;
	}
	s_orphaned_jobad = job;
}


Starter::~Starter()
{
	cancelKillTimer();
	setOrphanedJob(NULL);
	if (s_pid) {
		auto found = living_starters.find(s_pid);
		if (found != living_starters.end()) {
			living_starters.erase(found);
		}
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

void
Starter::config()
{
	free(s_path);
	s_path = nullptr;
	delete s_ad;
	s_ad = nullptr;

	s_path = param("STARTER");
	if (s_path == nullptr) {
		::dprintf(D_ALWAYS, "STARTER not defined in config file!\n");
		return;
	}

	FILE* fp;
	const char *args[] = { s_path,
					 "-classad",
					 NULL };
	char buf[1024];

		// first, try to execute the given path with a "-classad"
		// option, and grab the output as a ClassAd
		// note we run the starter here as root if possible,
		// since that is how the starter will be invoked for real,
		// and the real uid of the starter may influence the
		// list of capabilities the "-classad" option returns.
	{
		TemporaryPrivSentry sentry(PRIV_ROOT);
		fp = my_popenv( args, "r", FALSE );
	}

	if( ! fp ) {
		::dprintf( D_ALWAYS, "Failed to execute %s!\n", s_path );
		return;
	}
	ClassAd* ad = new ClassAd;
	bool read_something = false;
	while( fgets(buf, 1024, fp) ) {
		read_something = true;
		if( ! ad->Insert(buf) ) {
			::dprintf( D_ALWAYS, "Failed to insert \"%s\" into starter ClassAd!\n", buf );
			delete( ad );
			pclose( fp );
			return;
		}
	}
	my_pclose( fp );
	if( ! read_something ) {
		::dprintf( D_ALWAYS,
		         "\"%s -classad\" did not produce any output, ignoring\n",
		         s_path );
		delete( ad );
		return;
	}
	s_ad = ad;
}

void
Starter::publish( ClassAd* ad )
{
	std::string ability_list;
	std::vector<std::string> ignored_attr_list = {ATTR_VERSION, ATTR_IS_DAEMON_CORE};

	if (!s_ad) {
		return;
	}

	ExprTree *tree, *pCopy;
	const char *lhstr = NULL;
	for (auto itr = s_ad->begin(); itr != s_ad->end(); itr++) {
		lhstr = itr->first.c_str();
		tree = itr->second;
		pCopy=0;
	
			// insert every attr that's not in the ignored_attr_list
		if (!contains(ignored_attr_list, lhstr)) {
			pCopy = tree->Copy();
			ad->Insert(lhstr, pCopy);
			if (strncasecmp(lhstr, "Has", 3) == MATCH) {
				if (!ability_list.empty()) ability_list += ',';
				ability_list += lhstr;
			}
		}
	}

	ad->Assign(ATTR_STARTER_ABILITY_LIST, ability_list);

}


bool
Starter::signal( int signo )
{
	return reallykill( signo, false );
}


bool
Starter::killfamily()
{
	return reallykill( SIGKILL, true );
}


bool
Starter::reallykill( int signo, bool kill_family )
{
#ifndef WIN32
	struct stat st;
#endif
	int 		ret = 0;
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
	case SIGQUIT:
		signame = "SIGQUIT";
		break;
	case SIGTERM:
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

	if (kill_family) {
		dprintf( D_FULLDEBUG, 
				 "In Starter::killfamily() with pid %d, sig %d (%s)\n",
				 s_pid, signo, signame );
	} else {
		dprintf( D_FULLDEBUG, 
				 "In Starter::signal() with pid %d, sig %d (%s)\n",
				 s_pid, signo, signame );
	}

	priv = set_root_priv();

		// Finally, do the deed.
	if (kill_family) {
		if( signo != SIGKILL ) {
			dprintf( D_ALWAYS, 
					 "In Starter::killfamily() with %s\n", signame );
			EXCEPT( "Starter::killfamily() can only handle SIGKILL!" );
		}
		if (daemonCore->Kill_Family(s_pid) == FALSE) {
			dprintf(D_ALWAYS,
			        "error killing process family of starter with pid %u\n",
				s_pid);
			ret = -1;
		}
	} else {
		if( daemonCore->ProcessExitedButNotReaped(s_pid) ) {
			ret = TRUE;
		} else {
			ret = daemonCore->Send_Signal( (s_pid), signo );
		}
			// Translate Send_Signal's return code to Unix's kill()
		if( ret == FALSE ) {
			ret = -1;
		} else {
			ret = 0;
		}
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
Starter::finalizeExecuteDir(Claim * claim)
{
		// If setExecuteDir() has already been called (e.g. BOINC), then
		// do nothing.  Otherwise, choose the execute dir now.
	if( s_execute_dir.empty() ) {
		ASSERT( claim && claim->rip() );
		setExecuteDir( claim->rip()->executeDir() );
	}
}

// Spawn the starter process that this starter object is managing.
// the claim is optional and will be NULL for boinc jobs and possibly others.
//
// returns: the pid of the created starter process, returns 0 on failure.
//
// on success this object is owned by the global living_starters data structure and may be located by calling findStarterByPid()
// on failure the caller is responsible for deleting this object.
//
pid_t Starter::spawn(Claim * claim, time_t now, Stream* s)
{
		// if execute dir has not been set, choose one now
	finalizeExecuteDir(claim);

	if (claim) {
		s_is_vm_universe = claim->universe() == CONDOR_UNIVERSE_VM;

		// we will need to know the number of cpus allocated to the hypervisor in order to interpret
		// the cpu usage numbers, so capture that now.
		ClassAd * jobAd = claim->ad();
		if (jobAd) {
			if ( ! jobAd->LookupInteger(ATTR_JOB_VM_VCPUS, s_num_vm_cpus) &&
				 ! jobAd->LookupInteger(ATTR_REQUEST_CPUS, s_num_vm_cpus)) {
				s_num_vm_cpus = 1;
			}
		}
	}

	ClaimType ct = CLAIM_NONE;
	if (claim) { ct = claim->type(); }
	if (ct == CLAIM_COD) {
		s_pid = execJobPipeStarter(claim);
	}
#if HAVE_JOB_HOOKS
	else if (ct == CLAIM_FETCH) {
		s_pid = execJobPipeStarter(claim);
	}
#endif /* HAVE_JOB_HOOKS */
#if HAVE_BOINC
	else if( isBOINC() ) {
		s_pid = execBOINCStarter(claim);
	}
#endif /* HAVE_BOINC */
	else {
		s_pid = execDCStarter(claim, s);
	}

	if( s_pid == 0 ) {
		dprintf( D_ALWAYS, "ERROR: exec_starter returned %d\n", s_pid );
	} else {
		s_birthdate = now;
		living_starters[s_pid] = this;
	}

	return s_pid;
}

// called when the starter process is reaped
// the claim is optional, if it is NULL then EITHER this starter was never
// associated with a claim (i.e. boinc) or the claim was already deleted
void
Starter::exited(Claim * claim, int status) // Claim may be NULL.
{
	// remember that we have reaped this starter.
	s_was_reaped = true;
	s_exit_status = status;
	bool abnormal_exit = WIFSIGNALED(status) || status != 0;

	ClassAd *jobAd = NULL;
	ClassAd dummyAd; // used then claim is NULL

	if (claim && claim->ad()) {
		// real jobs in the startd have claims and job ads, boinc and perhaps others won't
		jobAd = claim->ad();
	} else if (s_orphaned_jobad) {
		jobAd = s_orphaned_jobad;
	} else {
		// Dummy up an ad, assume a boinc type job.
		int now = (int) time(0);
		SetMyTypeName(dummyAd, JOB_ADTYPE);
		dummyAd.Assign(ATTR_TARGET_TYPE, STARTD_OLD_ADTYPE); // TODO: remove this once no-one needs TargetType anymore
		dummyAd.Assign(ATTR_CLUSTER_ID, now);
		dummyAd.Assign(ATTR_PROC_ID, 1);
		dummyAd.Assign(ATTR_OWNER, "boinc");
		dummyAd.Assign(ATTR_Q_DATE, s_birthdate);
		dummyAd.Assign(ATTR_JOB_PRIO, 0);
		dummyAd.Assign(ATTR_IMAGE_SIZE, 0);
		dummyAd.Assign(ATTR_JOB_CMD, "boinc");
		std::string gjid;
		formatstr(gjid,"%s#%d#%d#%d", get_local_hostname().c_str(), now, 1, now);
		dummyAd.Assign(ATTR_GLOBAL_JOB_ID, gjid);
		jobAd = &dummyAd;
	}

	// First, patch up the ad a little bit 
	jobAd->Assign(ATTR_COMPLETION_DATE, (int)time(0));
	int runtime = time(0) - s_birthdate;
	
	jobAd->Assign(ATTR_JOB_REMOTE_WALL_CLOCK, runtime);
	int jobStatus = COMPLETED;
	if (WIFSIGNALED(status)) {
		jobStatus = REMOVED;
	}

	jobAd->Assign(ATTR_STARTER_EXIT_STATUS, status);
	jobAd->Assign(ATTR_JOB_STATUS, jobStatus);

	if (claim) {
		bool badputFromDraining = claim->getBadputCausedByDraining();
		jobAd->Assign(ATTR_BADPUT_CAUSED_BY_DRAINING, badputFromDraining);
		bool badputFromPreemption = claim->getBadputCausedByPreemption();
		jobAd->Assign(ATTR_BADPUT_CAUSED_BY_PREEMPTION, badputFromPreemption);
	}
	AppendHistory(jobAd);
	WritePerJobHistoryFile(jobAd, true /* use gjid for filename*/);

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
		// If we created the parent execute directory (say for filesystem
		// encryption), then clean that up, too.
	ASSERT( executeDir() );

	cleanup_execute_dir( s_pid, executeDir(), logicalVolumeName(), s_created_execute_dir, abnormal_exit );

}


int
Starter::execJobPipeStarter( Claim* claim )
{
	int rval;
	std::string lock_env;
	ArgList args;
	Env env;
	char* tmp;

	if (claim->type() == CLAIM_COD) {
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

		env.SetEnv(lock_env.c_str());
	}

		// Create an argument list for this starter, based on the claim.
	claim->makeStarterArgs(args);

	int* std_fds_p = NULL;
	int std_fds[3];
	int pipe_fds[2] = {-1,-1};
	if( claim->hasJobAd() ) {
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

	rval = execDCStarter( claim, args, &env, std_fds_p, NULL );

	if( claim->hasJobAd() ) {
			// now that the starter has been spawned, we need to do
			// some things with the pipe:

			// 1) close our copy of the read end of the pipe, so we
			// don't leak it.  we have to use DC::Close_Pipe() for
			// this, not just close(), so things work on windoze.
		daemonCore->Close_Pipe( pipe_fds[0] );

			// 2) dump out the job ad to the write end, since the
			// starter is now alive and can read from the pipe.

		claim->writeJobAd( pipe_fds[1] );

			// Now that all the data is written to the pipe, we can
			// safely close the other end, too.  
		daemonCore->Close_Pipe( pipe_fds[1] );
	}

	return rval;
}


#if HAVE_BOINC
int
Starter::execBOINCStarter( Claim * claim )
{
	ArgList args;

	args.AppendArg("condor_starter");
	args.AppendArg("-f");
	args.AppendArg("-append");
	args.AppendArg("boinc");
	args.AppendArg("-job-keyword");
	args.AppendArg("boinc");
	return execDCStarter( claim, args, NULL, NULL, NULL );
}
#endif /* HAVE_BOINC */


int
Starter::execDCStarter( Claim * claim, Stream* s )
{
	ArgList args;

	// decide whether we are going to pass a -slot-name argument to the starter
	// by default we do things the pre 8.1.4 way
	// we default to -slot-name, but that can be disabled by setting STARTER_LOG_NAME_APPEND = true
	// otherwise the value of this param determines what we append.
	bool slot_arg = false;
	enum { APPEND_NOTHING, APPEND_SLOT, APPEND_CLUSTER, APPEND_JOBID } append = APPEND_SLOT;

	std::string ext;
	if (param(ext, "STARTER_LOG_NAME_APPEND")) {
		slot_arg = true;
		if (MATCH == strcasecmp(ext.c_str(), "Slot")) {
			append = APPEND_SLOT;
		} else if (MATCH == strcasecmp(ext.c_str(), "ClusterId") || MATCH == strcasecmp(ext.c_str(), "Cluster")) {
			append = APPEND_CLUSTER;
		} else if (MATCH == strcasecmp(ext.c_str(), "JobId")) {
			append = APPEND_JOBID;
		} else if (MATCH == strcasecmp(ext.c_str(), "false") || MATCH == strcasecmp(ext.c_str(), "0")) {
			append = APPEND_NOTHING;
		} else if (MATCH == strcasecmp(ext.c_str(), "true") || MATCH == strcasecmp(ext.c_str(), "1")) {
			append = APPEND_SLOT;
			slot_arg = false;
		}
	}


	args.AppendArg("condor_starter");
	args.AppendArg("-f");

	// If a slot-type is defined, pass it as the local name
	// so starter params can switch on slot-type
	if (claim->rip()->type_id() != 0) {
		args.AppendArg("-local-name");

		std::string slot_type_name("slot_type_");
		formatstr_cat(slot_type_name, "%d", claim->rip()->type_id());
		args.AppendArg(slot_type_name);
	}

	ClassAd * jobAd = claim->ad();
	if (jobAd) {
		bool add_a2_arg = false;
		std::string a2arg;
		if (jobAd->LookupBool(ATTR_JOB_STARTER_DEBUG, add_a2_arg) ||
			jobAd->LookupString(ATTR_JOB_STARTER_DEBUG, a2arg)) {
			if (string_is_boolean_param(a2arg.c_str(), add_a2_arg)) {
				a2arg.clear();
			} else if ( ! a2arg.empty()) {
				// validate the a2arg characters.  should have only space,tab, and +-|,~:[A-Za-Z0-9]
				size_t off = strspn(a2arg.c_str(), "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_:+-~, \t");
				add_a2_arg = a2arg.size() == off;
				if ( ! add_a2_arg) {
					dprintf(D_ERROR, "Ignoring invalid JobStarterDebug value (off=%d): %s\n", off, a2arg.c_str());
				} else {
					replace_str(a2arg,"default", "$(ALL_DEBUG),$(STARTER_DEBUG)");
				}
				trim(a2arg);
			}
		}
		if (add_a2_arg) {
			if ( ! a2arg.empty()) a2arg += ",";
			formatstr_cat(a2arg, "$(EXECUTE)/dir_$(PID)/" SANDBOX_STARTER_LOG_FILENAME);
			args.AppendArg("-a2");
			args.AppendArg(a2arg);
		}
	}


	// Note: the "-a" option is a daemon core option, so it
	// must come first on the command line.
	if (append != APPEND_NOTHING) {
		args.AppendArg("-a");
		switch (append) {
		case APPEND_CLUSTER: args.AppendArg(std::to_string(claim->cluster())); break;

		case APPEND_JOBID: {
			std::string jobid;
			formatstr(jobid, "%d.%d", claim->cluster(), claim->proc());
			args.AppendArg(jobid.c_str());
		} break;

		case APPEND_SLOT: args.AppendArg(claim->rip()->r_id_str); break;
		default:
			EXCEPT("Programmer Error: unexpected append argument %d", append);
		}
	}

	if (slot_arg) {
		args.AppendArg("-slot-name");
		args.AppendArg(claim->rip()->r_id_str);
	}

	args.AppendArg(claim->client()->c_host);
	execDCStarter( claim, args, NULL, NULL, s );

	return s_pid;
}

int
Starter::receiveJobClassAdUpdate( Stream *stream )
{
	ClassAd update_ad;
	int final_update = 0;

	s_last_update_time = time(NULL);

		// It is expected that we will get here when the stream is closed.
		// Unfortunately, log noise will be generated when we try to read
		// from it.

	stream->decode();
	stream->timeout(10);
	if( !stream->get( final_update) ||
		!getClassAd( stream, update_ad ) ||
		!stream->end_of_message() )
	{
		dprintf(D_JOB, "Could not read update job ClassAd update from starter, assuming final_update\n");
		final_update = 1;
	}
	else {
		if (IsDebugLevel(D_JOB)) {
			std::string adbuf;
			dprintf(D_JOB, "Received %sjob ClassAd update from starter :\n%s", final_update?"final ":"", formatAd(adbuf, update_ad, "\t"));
		} else {
			dprintf(D_FULLDEBUG, "Received %sjob ClassAd update from starter.\n", final_update?"final ":"");
		}

		// In addition to new info about the job, the starter also
		// inserts contact info for itself (important for CCB and
		// shadow-starter reconnect, because startd needs to relay
		// starter's full contact info to the shadow when queried).
		// It's a bit of a hack to do it through this channel, but
		// better than nothing.
		update_ad.LookupString(ATTR_STARTER_IP_ADDR,m_starter_addr);

		// The update may contain information of VM cpu utitilization because
		// hypervisors such as libvirt will spawn processes outside
		// of condor's perview. we want to capture the value here so we can include it in our usage
		double fPercentCPU=0.0;
		bool has_vm_cpu = update_ad.LookupFloat(ATTR_JOB_VM_CPU_UTILIZATION, fPercentCPU);

		Claim* claim = resmgr->getClaimByPid(s_pid);
		if( claim ) {
			claim->receiveJobClassAdUpdate(update_ad, final_update);

			ClassAd * jobAd = claim->ad();
			if (jobAd) {
				// in case the cpu utilization was modified while being updated.
				has_vm_cpu = jobAd->LookupFloat(ATTR_JOB_VM_CPU_UTILIZATION, fPercentCPU);
			}
		}

		// we only want to overwrite the member for additional cpu if we got an update for that attribute
		if (has_vm_cpu) {
			s_vm_cpu_usage = fPercentCPU;
		}
	}

	if( final_update ) {
		dprintf(D_FULLDEBUG, "Closing job ClassAd update socket from starter.\n");
		daemonCore->Cancel_Socket(s_job_update_sock);
		delete s_job_update_sock;
		s_job_update_sock = NULL;
		s_got_final_update = true;
	}
	return KEEP_STREAM;
}

// most methods of spawing the starter end up here. 
int Starter::execDCStarter(
	Claim * claim, // claim is optional and will be NULL for backfill jobs.
	ArgList const &args,
	Env const *env,
	int* std_fds,
	Stream* s )
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

		// Handle encrypted execute directory
	FilesystemRemap  fs_remap_obj;	// put on stack so destroyed when leave this method
	FilesystemRemap* fs_remap = NULL;
	// If admin desires encrypted exec dir in config, do it
	bool encrypt_execdir = param_boolean_crufty("ENCRYPT_EXECUTE_DIRECTORY",false);
	// Or if user wants encrypted exec in job ad, do it
	if (!encrypt_execdir && claim && claim->ad()) {
		claim->ad()->LookupBool(ATTR_ENCRYPT_EXECUTE_DIRECTORY,encrypt_execdir);
	}
	if ( encrypt_execdir ) {
#ifdef LINUX
		// On linux, setup a directory $EXECUTE/encryptedX subdirectory
		// to serve as an ecryptfs mount point; pass this directory
		// down to the condor_starter as if it were $EXECUTE so
		// that the starter creates its dir_<pid> directory on the
		// ecryptfs filesystem setup by doing an AddEncryptedMapping.
		static int unsigned long privdirnum = 0;
		TemporaryPrivSentry sentry(PRIV_CONDOR);
		formatstr_cat(s_execute_dir,"%cencrypted%lu",
				DIR_DELIM_CHAR,privdirnum++);
		if( mkdir(s_execute_dir.c_str(), 0755) < 0 ) {
			dprintf( D_ERROR,
			         "Failed to create encrypted dir %s: %s\n",
			         s_execute_dir.c_str(),
			         strerror(errno) );
			return 0;
		}
		s_created_execute_dir = true;
		dprintf( D_ALWAYS,
		         "Created encrypted dir %s\n", s_execute_dir.c_str() );
		fs_remap = &fs_remap_obj;
		if ( fs_remap->AddEncryptedMapping(s_execute_dir.c_str()) ) {
			// FilesystemRemap object dprintfs out an error message for us
			return 0;
		}
#endif
	}

		// The starter figures out its execute directory by paraming
		// for EXECUTE, which we override in the environment here.
		// This way, all the logic about choosing a directory to use
		// is in only one place.
	ASSERT( executeDir() );
	new_env.SetEnv( "_CONDOR_EXECUTE", executeDir() );

	auto * volman = resmgr->getVolumeManager();

	if (claim && volman && volman->is_enabled() && claim->rip()) {
		// TODO: generate an LV name that is more unique than the slot id
		// perhaps  r_id_str + startd_pid + uniqueness_value
		s_lv_name = claim->rip()->r_id_str;

			// Cleanup from any previously-crashed starters.
			// TODO: do we really want to do this here?
		CondorError err;
		if (volman->CleanupLV(s_lv_name, err) < 0) {
			std::string msg = err.getFullText();
			dprintf(D_ERROR, "Last chance cleanup of LV %s failed : %s\n", s_lv_name.c_str(), msg.c_str());
			return 0;
		}

		long long disk_kb = -1;
		if (claim->rip()->r_attr) { disk_kb = claim->rip()->r_attr->get_disk(); }
		volman->UpdateStarterEnv(new_env, s_lv_name, disk_kb);
	}

	env = &new_env;


		// Build the affinity string to pass to the starter via env

	std::string affinityString;
	if (claim && claim->rip() && claim->rip()->get_affinity_set()) {
		std::list<int> *l = claim->rip()->get_affinity_set();
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

	bool affinityBool = false;
	if ( ! claim || ! claim->ad()) {
		affinityBool = param_boolean("ASSIGN_CPU_AFFINITY", false);
	} else {
		auto_free_ptr assign_cpu_affinity(param("ASSIGN_CPU_AFFINITY"));
		if ( ! assign_cpu_affinity.empty()) {
			classad::Value value;
			if (claim->ad()->EvaluateExpr(assign_cpu_affinity.ptr(), value)) {
				if ( ! value.IsBooleanValueEquiv(affinityBool)) {
					// was an expression, but not a bool, so report it and continue
					EXCEPT("ASSIGN_CPU_AFFINITY does not evaluate to a boolean, it is : %s", ClassAdValueToString(value));
				}
			}
		}
	}

	if (affinityBool) {
		new_env.SetEnv("_CONDOR_STARTD_ASSIGNED_AFFINITY", affinityString.c_str());
		new_env.SetEnv("_CONDOR_ENFORCE_CPU_AFFINITY", "true");
		dprintf(D_ALWAYS, "Setting affinity env to %s\n", affinityString.c_str());
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
	if (claim) {
		claim->writeMachAdAndOverlay(s_job_update_sock);
	}

	if( daemonCore->Register_Socket(
			s_job_update_sock,
			"starter ClassAd update socket",
			(SocketHandlercpp)&Starter::receiveJobClassAdUpdate,
			"receiveJobClassAdUpdate",
			this) < 0 )
	{
		EXCEPT("Failed to register ClassAd update socket.");
	}

	int reaper_id;
	if( s_reaper_id > 0 ) {
		reaper_id = s_reaper_id;
	} else {
		reaper_id = main_reaper;
	}

	if(IsFulldebug(D_FULLDEBUG)) {
		std::string args_string;
		final_args->GetArgsStringForDisplay(args_string);
		dprintf( D_FULLDEBUG, "About to Create_Process \"%s\"\n",
				 args_string.c_str() );
	}

	FamilyInfo fi;
	fi.max_snapshot_interval = pid_snapshot_interval;

	std::string sockBaseName( "starter" );
	if( claim ) { sockBaseName = claim->rip()->r_id_str; }
	std::string daemon_sock = SharedPortEndpoint::GenerateEndpointName( sockBaseName.c_str() );
	s_pid = daemonCore->
		Create_Process( final_path, *final_args, PRIV_ROOT, reaper_id,
		                TRUE, TRUE, env, NULL, &fi, inherit_list, std_fds,
						NULL, 0, NULL, 0, NULL, NULL, daemon_sock.c_str(), NULL, fs_remap);
	if( s_pid == FALSE ) {
		dprintf( D_ALWAYS, "ERROR: exec_starter failed!\n");
		s_pid = 0;
	}

	return s_pid;
}

bool
Starter::active() const
{
	return( (s_pid != 0) );
}
	

void
Starter::dprintf( int flags, const char* fmt, ... )
{
	const DPF_IDENT ident = 0; // REMIND: maybe something useful here??
	va_list args;
	va_start( args, fmt );
	if ( ! s_dpf.empty()) {
		std::string fmt_str( s_dpf );
		fmt_str += ": ";
		fmt_str += fmt;
		::_condor_dprintf_va( flags, ident, fmt_str.c_str(), args );
	} else {
		::_condor_dprintf_va( flags, ident, fmt, args );
	}
	va_end( args );
}

const ProcFamilyUsage & Starter::updateUsage()
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
		
		// computations outside take cores into account, so we need to correct for that
		// using the latest cached values for vm cpu usage and vm num cpus.
		double fPercentCPU = s_vm_cpu_usage * s_num_vm_cpus;
		dprintf( D_LOAD, "Starter::percentCpuUsage() adding VM Utilization %f\n",fPercentCPU);
		
		s_usage.percent_cpu += fPercentCPU;
		
	}

	if( IsDebugVerbose(D_LOAD) ) {
		dprintf(D_LOAD | D_VERBOSE,
		        "Starter::percentCpuUsage(): Percent CPU usage "
		        "for the family of starter with pid %u is: %f\n",
		        s_pid,
		        s_usage.percent_cpu );
	}

	return s_usage;
}

void
Starter::printInfo( int debug_level )
{
	dprintf( debug_level, "Info for \"%s\":\n", s_path );
	dprintf( debug_level | D_NOHEADER, "IsDaemonCore: True\n" );
	if( ! s_ad ) {
		dprintf( debug_level | D_NOHEADER, 
				 "No ClassAd available!\n" ); 
	} else {
		dPrintAd( debug_level, *s_ad );
	}
	dprintf( debug_level | D_NOHEADER, "*** End of starter info ***\n" ); 
}


char const*
Starter::getIpAddr( void )
{
	if( ! s_pid ) {
		return nullptr;
	}
	if( !m_starter_addr.empty() ) {
		return m_starter_addr.c_str();
	}

	// At this point, we haven't gotten an update from the starter (yet)
	// so the starter hasn't told us the real, external address with CCB
	// extensions and other goodness.  We used to return a guess for what
	// we thought the address to be, but that broke condor_ssh_to_job
	// so, if we don't know for sure, return that we don't know.
	return nullptr;
}


bool
Starter::killHard( int timeout )
{
	if( ! active() ) {
		return true;
	}
	
	if( ! signal(SIGQUIT) ) {
		killfamily();
		return false;
	}
	dprintf(D_FULLDEBUG, "in starter:killHard starting kill timer\n");
	startKillTimer(timeout);

	return true;
}


bool
Starter::killSoft( int timeout, bool /*state_change*/ )
{
	if( ! active() ) {
		return true;
	}
	if( ! signal(SIGTERM) ) {
		killfamily();
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

	startSoftkillTimeout(timeout);

	return true;
}


bool
Starter::suspend( void )
{
	if( ! active() ) {
		return true;
	}
	if( ! signal(DC_SIGSUSPEND) ) {
		killfamily();
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
	if( ! signal(DC_SIGCONTINUE) ) {
		killfamily();
		return false;
	}
	return true;
}


int
Starter::startKillTimer( int timeout )
{
	if( s_kill_tid >= 0 ) {
			// Timer already started.
		return TRUE;
	}

		// Create a periodic timer so that if the kill attempt fails,
		// we keep trying.
	s_kill_tid = 
		daemonCore->Register_Timer( timeout,
									std::max(1,timeout),
						(TimerHandlercpp)&Starter::sigkillStarter,
						"sigkillStarter", this );
	if( s_kill_tid < 0 ) {
		EXCEPT( "Can't register DaemonCore timer" );
	}
	return TRUE;
}


int
Starter::startSoftkillTimeout( int timeout )
{
	if( s_softkill_tid >= 0 ) {
			// Timer already started.
		return TRUE;
	}

	int softkill_timeout = timeout;

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
Starter::sigkillStarter( int /* timerID */ )
{
		// In case the kill fails for some reason, we are on a periodic
		// timer that will keep trying.

	if( active() ) {
		dprintf( D_ALWAYS, "starter (pid %d) is not responding to the "
				 "request to hardkill its job.  The startd will now "
				 "directly hard kill the starter and all its "
				 "decendents.\n", s_pid );

			// Kill the starter's entire family.
		killfamily();
	}
}

void
Starter::softkillTimeout( int /* timerID */ )
{
	s_softkill_tid = -1;
	if( active() ) {
		dprintf( D_ALWAYS, "max vacate time expired.  Escalating to a fast shutdown of the job.\n" );
		killHard(s_is_vm_universe ? vm_killing_timeout : killing_timeout);
	}
}

bool
Starter::holdJob(char const *hold_reason,int hold_code,int hold_subcode,bool soft,int timeout)
{
	if( m_hold_job_cb ) {
		dprintf(D_ALWAYS,"holdJob() called when operation already in progress (starter pid %d).\n", s_pid);
		return true;
	}

	// If we can't find the complete-with-CCB address, use the socket we
	// handed to the starter when we created it.  Since the startd is always
	// local to the starter (until further notice), this should always work.
	//
	// Arguably, we shouldn't be using TCP/IP to communicated with our
	// starters anyway, and even if we must, it should almost certainly
	// be done over the loopback interface where we hopefully don't have to
	// fight for port numbers or deal with nearly as many attackers.
	const char * sinful = getIpAddr();
	if( sinful == NULL ) {
		sinful = daemonCore->InfoCommandSinfulString( s_pid );
	}

	classy_counted_ptr<DCStarter> starter = new DCStarter(sinful);
	classy_counted_ptr<StarterHoldJobMsg> msg = new StarterHoldJobMsg(hold_reason,hold_code,hold_subcode,soft);

	m_hold_job_cb = new DCMsgCallback( (DCMsgCallback::CppFunction)&Starter::holdJobCallback, this );

	msg->setCallback( m_hold_job_cb );

	// store the timeout so that the holdJobCallback has access to it.
	s_hold_timeout = timeout;
	starter->sendMsg(msg.get());

	if( soft ) {
		startSoftkillTimeout(timeout);
	}
	else {
		startKillTimer(timeout);
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
		killSoft(s_hold_timeout);
	}
}
