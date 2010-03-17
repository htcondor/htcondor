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


#include "condor_common.h"
#include "condor_classad.h"
#include "condor_debug.h"
#include "condor_daemon_core.h"
#include "condor_attributes.h"
#include "condor_syscall_mode.h"
#include "exit.h"
#include "vanilla_proc.h"
#include "starter.h"
#include "syscall_numbers.h"
#include "dynuser.h"
#include "condor_config.h"
#include "domain_tools.h"
#include "classad_helpers.h"

#ifdef WIN32
#include "executable_scripts.WINDOWS.h"
extern dynuser* myDynuser;
#endif

extern CStarter *Starter;

VanillaProc::VanillaProc(ClassAd* jobAd) : OsProc(jobAd)
{
#if !defined(WIN32)
	m_escalation_tid = -1;
#endif
}

int
VanillaProc::StartJob()
{
	dprintf(D_FULLDEBUG,"in VanillaProc::StartJob()\n");

	// vanilla jobs, unlike standard jobs, are allowed to run 
	// shell scripts (or as is the case on NT, batch files).  so
	// edit the ad so we start up a shell, pass the executable as
	// an argument to the shell, if we are asked to run a .bat file.
#ifdef WIN32

	CHAR		interpreter[MAX_PATH+1],
				systemshell[MAX_PATH+1];    
	const char* jobtmp				= Starter->jic->origJobName();
	int			joblen				= strlen(jobtmp);
	const char	*extension			= joblen > 0 ? &(jobtmp[joblen-4]) : NULL;
	bool		binary_executable	= ( extension && 
										( MATCH == strcasecmp ( ".exe", extension ) || 
										  MATCH == strcasecmp ( ".com", extension ) ) ),
				java_universe		= ( CONDOR_UNIVERSE_JAVA == job_universe );
	ArgList		arguments;
	MyString	filename,
				jobname, 
				error;
	
	if ( extension && !java_universe && !binary_executable ) {

		/** since we do not actually know how long the extension of
			the file is, we'll need to hunt down the '.' in the path,
			if it exists */
		extension = strrchr ( jobtmp, '.' );

		if ( !extension ) {

			dprintf ( 
				D_ALWAYS, 
				"VanillaProc::StartJob(): Failed to extract "
				"the file's extension.\n" );

			/** don't fail here, since we want executables to run
				as usual.  That is, some condor jobs submit 
				executables that do not have the '.exe' extension,
				but are, nonetheless, executable binaries.  For
				instance, a submit script may contain:

				executable = executable$(OPSYS) */

		} else {

			/** pull out the path to the executable */
			if ( !JobAd->LookupString ( 
				ATTR_JOB_CMD, 
				jobname ) ) {
				
				/** fall back on Starter->jic->origJobName() */
				jobname = jobtmp;

			}

			/** If we transferred the job, it may have been
				renamed to condor_exec.exe even though it is
				not an executable. Here we rename it back to
				a the correct extension before it will run. */
			if ( MATCH == strcasecmp ( 
					CONDOR_EXEC, 
					condor_basename ( jobname.Value () ) ) ) {
				filename.sprintf ( "condor_exec%s", extension );
				rename ( CONDOR_EXEC, filename.Value () );					
			} else {
				filename = jobname;
			}
			
			/** Since we've renamed our executable, we need to
				update the job ad to reflect this change. */
			if ( !JobAd->Assign ( 
				ATTR_JOB_CMD, 
				filename ) ) {

				dprintf (
					D_ALWAYS,
					"VanillaProc::StartJob(): ERROR: failed to "
					"set new executable name.\n" );

				return FALSE;

			}

			/** We've moved the script to argv[1], so we need to 
				add	the remaining arguments to positions argv[2]..
				argv[/n/]. */
			if ( !arguments.AppendArgsFromClassAd ( JobAd, &error ) ||
				 !arguments.InsertArgsIntoClassAd ( JobAd, NULL, 
				&error ) ) {

				dprintf (
					D_ALWAYS,
					"VanillaProc::StartJob(): ERROR: failed to "
					"get arguments from job ad: %s\n",
					error.Value () );

				return FALSE;

			}

			/** Since we know already we don't want this file returned
				to us, we explicitly add it to an exception list which
				will stop the file transfer mechanism from considering
				it for transfer back to its submitter */
			Starter->jic->removeFromOutputFiles (
				filename.Value () );

		}
			
	}
#endif

	// set up a FamilyInfo structure to tell OsProc to register a family
	// with the ProcD in its call to DaemonCore::Create_Process
	//
	FamilyInfo fi;

	// take snapshots at no more than 15 seconds in between, by default
	//
	fi.max_snapshot_interval = param_integer("PID_SNAPSHOT_INTERVAL", 15);

	char const *dedicated_account = Starter->jic->getExecuteAccountIsDedicated();
	if( ThisProcRunsAlongsideMainProc() ) {
			// If we track a secondary proc's family tree (such as
			// sshd) using the same dedicated account as the job's
			// family tree, we could end up killing the job when we
			// clean up the secondary family.
		dedicated_account = NULL;
	}
	if (dedicated_account) {
			// using login-based family tracking
		fi.login = dedicated_account;
			// The following message is documented in the manual as the
			// way to tell whether the dedicated execution account
			// configuration is being used.
		dprintf(D_ALWAYS,
		        "Tracking process family by login \"%s\"\n",
		        fi.login);
	}

#if defined(LINUX)
	// on Linux, we also have the ability to track processes via
	// a phony supplementary group ID
	//
	gid_t tracking_gid;
	if (param_boolean("USE_GID_PROCESS_TRACKING", false)) {
		if (!can_switch_ids() &&
		    (Starter->condorPrivSepHelper() == NULL))
		{
			EXCEPT("USE_GID_PROCESS_TRACKING enabled, but can't modify "
			           "the group list of our children unless running as "
			           "root or using PrivSep");
		}
		fi.group_ptr = &tracking_gid;
	}
#endif

	// have OsProc start the job
	//
	return OsProc::StartJob(&fi);
}


bool
VanillaProc::PublishUpdateAd( ClassAd* ad )
{
	dprintf( D_FULLDEBUG, "In VanillaProc::PublishUpdateAd()\n" );

	ProcFamilyUsage* usage;
	ProcFamilyUsage cur_usage;
	if (m_proc_exited) {
		usage = &m_final_usage;
	}
	else {
		if (daemonCore->Get_Family_Usage(JobPid, cur_usage) == FALSE) {
			dprintf(D_ALWAYS, "error getting family usage in "
					"VanillaProc::PublishUpdateAd() for pid %d\n", JobPid);
			return false;
		}
		usage = &cur_usage;
	}

		// Publish the info we care about into the ad.
	char buf[200];
	sprintf( buf, "%s=%lu", ATTR_JOB_REMOTE_SYS_CPU, usage->sys_cpu_time );
	ad->InsertOrUpdate( buf );
	sprintf( buf, "%s=%lu", ATTR_JOB_REMOTE_USER_CPU, usage->user_cpu_time );
	ad->InsertOrUpdate( buf );
	sprintf( buf, "%s=%lu", ATTR_IMAGE_SIZE, usage->max_image_size );
	ad->InsertOrUpdate( buf );

		// Update our knowledge of how many processes the job has
	num_pids = usage->num_procs;

		// Now, call our parent class's version
	return OsProc::PublishUpdateAd( ad );
}


bool
VanillaProc::JobReaper(int pid, int status)
{
	dprintf(D_FULLDEBUG,"in VanillaProc::JobReaper()\n");

#if !defined(WIN32)
	cancelEscalationTimer();
#endif

	if (pid == JobPid) {
			// Make sure that nothing was left behind.
		daemonCore->Kill_Family(JobPid);

			// Record final usage stats for this process family, since
			// once the reaper returns, the family is no longer
			// registered with DaemonCore and we'll never be able to
			// get this information again.
		if (daemonCore->Get_Family_Usage(JobPid, m_final_usage) == FALSE) {
			dprintf(D_ALWAYS, "error getting family usage for pid %d in "
					"VanillaProc::JobReaper()\n", JobPid);
		}
	}

		// This will reset num_pids for us, too.
	return OsProc::JobReaper( pid, status );
}


void
VanillaProc::Suspend()
{
	dprintf(D_FULLDEBUG,"in VanillaProc::Suspend()\n");
	
	// suspend the user job
	if (JobPid != -1) {
		if (daemonCore->Suspend_Family(JobPid) == FALSE) {
			dprintf(D_ALWAYS,
			        "error suspending family in VanillaProc::Suspend()\n");
		}
	}
	
	// set our flag
	is_suspended = true;
}

void
VanillaProc::Continue()
{
	dprintf(D_FULLDEBUG,"in VanillaProc::Continue()\n");
	
	// resume user job
	if (JobPid != -1) {
		if (daemonCore->Continue_Family(JobPid) == FALSE) {
			dprintf(D_ALWAYS,
			        "error continuing family in VanillaProc::Continue()\n");
		}
	}

	// set our flag
	is_suspended = false;
}

bool
VanillaProc::ShutdownGraceful()
{
	dprintf(D_FULLDEBUG,"in VanillaProc::ShutdownGraceful()\n");
	
	if ( JobPid == -1 ) {
		// there is no process family yet, probably because we are still
		// transferring files.  just return true to say we're all done,
		// and that way the starter class will simply delete us and the
		// FileTransfer destructor will clean up.
		return true;
	}

	// WE USED TO.....
	//
	// take a snapshot before we softkill the parent job process.
	// this helps ensure that if the parent exits without killing
	// the kids, our JobExit() handler will get em all.
	//
	// TODO: should we make an explicit call to the procd here to tell
	// it to take a snapshot???

	// now softkill the parent job process.  this is exactly what
	// OsProc::ShutdownGraceful does, so call it.
	//
	return OsProc::ShutdownGraceful();	
}

bool
VanillaProc::ShutdownFast()
{
	dprintf(D_FULLDEBUG,"in VanillaProc::ShutdownFast()\n");
	
	if ( JobPid == -1 ) {
		// there is no process family yet, probably because we are still
		// transferring files.  just return true to say we're all done,
		// and that way the starter class will simply delete us and the
		// FileTransfer destructor will clean up.
		return true;
	}

	// We purposely do not do a SIGCONT here, since there is no sense
	// in potentially swapping the job back into memory if our next
	// step is to hard kill it.
	requested_exit = true;

#if !defined(WIN32)
	int kill_sig = -1;

	// Determine if a custom kill signal is provided in the job
	kill_sig = findRmKillSig( JobAd );
	if ( kill_sig == -1 )
	{
		kill_sig = findSoftKillSig( JobAd );
	}

	// If a custom kill signal was given, send that signal to the
	// job
	if ( kill_sig != -1 ) {
		if ( daemonCore->Signal_Process( JobPid, kill_sig ) == FALSE ) {
                        dprintf(D_ALWAYS,
                                "Error: Failed to send signal %d to job with "
                                " pid %u\n", kill_sig, JobPid);
		}
		else {
			startEscalationTimer();
			return false;
		}
	}
#endif
	return finishShutdownFast();
}

bool
VanillaProc::finishShutdownFast()
{
	// this used to be the only place where we would clean up the process
	// family. this, however, wouldn't properly clean up local universe jobs
	// so a call to Kill_Family has been added to JobReaper(). i'm not sure
	// that this call is still needed, but am unwilling to remove it on the
	// eve of Condor 7
	//   -gquinn, 2007-11-14
	daemonCore->Kill_Family(JobPid);

	return false;	// shutdown is pending, so return false
}

#if !defined(WIN32)
bool
VanillaProc::startEscalationTimer()
{
	int job_kill_time = 0;
	int killing_timeout = param_integer( "KILLING_TIMEOUT", 30 );
	int escalation_delay;

	if ( m_escalation_tid >= 0 ) {
		return true;
	}

	if ( !JobAd->LookupInteger(ATTR_KILL_SIG_TIMEOUT, job_kill_time) ) {
		job_kill_time = killing_timeout;
	}

	escalation_delay = std::max(std::min(killing_timeout-1, job_kill_time), 0);

	dprintf(D_FULLDEBUG, "Using escalation delay %d for Escalation Timer\n", escalation_delay);
	m_escalation_tid = daemonCore->Register_Timer(escalation_delay,
						escalation_delay,
						(TimerHandlercpp)&VanillaProc::EscalateSignal,
						"EscalateSignal", this);

	if ( m_escalation_tid < 0 ) {
		dprintf(D_ALWAYS, "Error: Unable to register signal esclation timer.  timeout attmepted: %d\n", escalation_delay);
	}
	return true;
}

void
VanillaProc::cancelEscalationTimer()
{
	int rval;
	if ( m_escalation_tid != -1 ) {
		rval = daemonCore->Cancel_Timer( m_escalation_tid );
		if ( rval < 0 ) {
			dprintf(D_ALWAYS, "Failed to cancel signal escalation "
					"timer (%d): daemonCore error\n", m_escalation_tid);
		}
		else {
			dprintf(D_FULLDEBUG, "Cancel signal escalation timer (%d)\n", m_escalation_tid);
		}
		m_escalation_tid = -1;
	}
}

bool
VanillaProc::EscalateSignal()
{
	cancelEscalationTimer();

	dprintf(D_FULLDEBUG, "Esclation Timer fired.  Killing job\n");
	finishShutdownFast();

	return true;
}
#endif
