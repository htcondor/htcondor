/***************************************************************
 *
 * Copyright (C) 1990-2018, Condor Team, Computer Sciences Department,
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
#include "condor_config.h"
#include "remote_proc.h"
#include "starter.h"
#include "my_popen.h"

extern Starter *Starter;


RemoteProc::RemoteProc( ClassAd * job_ad )
{
	dprintf ( D_FULLDEBUG, "In RemoteProc::RemoteProc()\n" );
	JobAd = job_ad;

	formatstr( m_remoteJobId, "%s-%ld", Starter->getMySlotName().c_str(), (long)daemonCore->getpid() );
}

RemoteProc::~RemoteProc() {
}

int RemoteProc::StartJob()
{
	// Invoke launcher with these arguments:
	//   'submit_wait_stageout'
	//   m_remoteJobId
	//   path to job ad
	//   path to status ad
	//   path to sandbox directory

	ArgList args;
	Env env;
	InitWorkerArgs(args, env, "submit_wait_stageout");

	std::string sandbox_dir = Starter->jic->jobRemoteIWD();
	std::string scratch_dir = Starter->GetWorkingDir(0);

	std::string job_ad_file = scratch_dir + ".job.ad";
	args.AppendArg(job_ad_file.c_str());

	std::string status_ad_file = scratch_dir + ".status.ad";
	args.AppendArg(status_ad_file.c_str());

	args.AppendArg(sandbox_dir.c_str());

//	ClassAd recoveryAd;
//	recoveryAd.Assign("DockerContainerName", containerName.c_str());
//	Starter->WriteRecoveryFile(&recoveryAd);

	int childFDs[3] = { -1, -1, -1 };
	{
		TemporaryPrivSentry sentry(PRIV_USER);
		std::string WorkerOutputFile = scratch_dir + ".worker_stdout";

		childFDs[1] = safe_open_wrapper(WorkerOutputFile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
		childFDs[2] = childFDs[1];
	}

	  // Ulog the execute event
	Starter->jic->notifyJobPreSpawn();

	CondorError err;

	// The following line is for condor_who to parse
	dprintf( D_ALWAYS, "About to exec remote worker:%s %s\n", args.GetArg(0), args.GetArg(1));
	FamilyInfo fi;
	fi.max_snapshot_interval = param_integer( "PID_SNAPSHOT_INTERVAL", 15 );
	int pid = daemonCore->Create_Process( args.GetArg(0), args,
		PRIV_USER_FINAL, 1, FALSE, FALSE, &env, scratch_dir.c_str(),
		& fi, NULL, childFDs );

	if( pid == 0 ) {
		dprintf( D_ERROR, "Failed to launch remote worker\n" );
		return FALSE;
	}
	JobPid = pid;
	dprintf( D_FULLDEBUG, "Remote worker submit pid %d\n", JobPid );
	// The following line is for condor_who to parse
	dprintf( D_ALWAYS, "Create_Process succeeded, pid=%d\n", JobPid);

	// Start a timer to poll for job usage updates.
//	updateTid = daemonCore->Register_Timer(2,
//			20, (TimerHandlercpp)&RemoteProc::getStats,
//			"RemoteProc::getStats",this);

	return TRUE;
}

bool RemoteProc::JobReaper( int pid, int status )
{
	if( pid == JobPid ) {

		if (status != 0) {
			// Something went wrong getting the job to or from the
			// remote location.
			// TODO For now, we're assuming tool's entire stdout/err
			//   makes a suitable hold reason message.
			std::string scratch_dir = Starter->GetWorkingDir(0);
			std::string message;

			char buf[512];
			buf[0] = '\0';

			{
				TemporaryPrivSentry sentry(PRIV_USER);
				std::string fileName = scratch_dir;
				fileName += "/.worker_stdout";
				int fd = safe_open_wrapper(fileName.c_str(), O_RDONLY, 0644);
				if (fd >= 0) {
					int r = read(fd, buf, 511);
					if (r < 0) {
						dprintf(D_ALWAYS, "Cannot read worker output file on job submission. Errno %d\n", errno);
						snprintf(buf, sizeof(buf), "Cannot read worker output file on job submission. Errno %d\n", errno);
					} else {
						buf[r] = '\0';
						int buflen = strlen(buf);
						for (int i = 0; i < buflen; i++) {
							if (buf[i] == '\n') buf[i] = ' ';
						}
					}
					close(fd);
				} else {
					dprintf(D_ALWAYS, "Cannot open .worker_stdout\n");
				}
				unlink(".worker_stdout");
			}
			message = buf;
			Starter->jic->holdJob(message.c_str(), CONDOR_HOLD_CODE::FailedToCreateProcess, 0);
			return UserProc::JobReaper( pid, status );
		}

//		daemonCore->Cancel_Timer(updateTid);

		// TODO extract status info
//		status = ...;
		dprintf( D_FULLDEBUG, "Setting status of remote job to %d.\n", status );

		// TODO: Record final job usage.

		// TODO Do cleanup call? (or should that be implicit with stageout?

		return UserProc::JobReaper( pid, status );
	}

	return 0;
}

//
// JobExit() is called after file transfer.
//
bool RemoteProc::JobExit() {
	int reason;

	dprintf( D_FULLDEBUG, "Inside RemoteProc::JobExit()\n" );

	if( requested_exit == true ) {
		if( Starter->jic->hadHold() || Starter->jic->hadRemove() ) {
			reason = JOB_KILLED;
		} else {
			reason = JOB_SHOULD_REQUEUE;
		}
//	} else if( job_not_started ) {
//		reason = JOB_NOT_STARTED;
	} else {
		reason = JOB_EXITED;
	}

	// TODO do we need to do anything here?
	RemoveRemoteJob();

	return Starter->jic->notifyJobExit( exit_status, reason, this );
}

void RemoteProc::Suspend() {
	dprintf( D_ALWAYS, "RemoteProc::Suspend()\n" );

		// TODO what can we do here?
}


void RemoteProc::Continue() {
	dprintf( D_ALWAYS, "RemoteProc::Continue()\n" );

	// TODO What can we do here?
}

//
// Setting requested_exit allows OsProc::JobExit() to handle telling the
// user why the job exited.
//

bool RemoteProc::Remove() {
	dprintf( D_ALWAYS, "RemoteProc::Remove()\n" );

	requested_exit = true;

	// TODO Call cleanup script
	//   Wait for remote side to acknowledge removal?
	RemoveRemoteJob();

	// Do NOT send any signals to the waiting process.  It should only
	// react when the container does.

	// If rm_kill_sig is not SIGKILL, the process may linger.  Returning
	// false indicates that shutdown is pending.
	return false;
}


bool RemoteProc::Hold() {
	dprintf( D_ALWAYS, "RemoteProc::Hold()\n" );

	requested_exit = true;

	// TODO Call cleanup script
	//   Wait for remote side to acknowledge removal?
	RemoveRemoteJob();

	// Do NOT send any signals to the waiting process.  It should only
	// react when the container does.

	// If rm_kill_sig is not SIGKILL, the process may linger.  Returning
	// false indicates that shutdown is pending.
	return false;
}


bool RemoteProc::ShutdownGraceful() {
	dprintf( D_ALWAYS, "RemoteProc::ShutdownGraceful()\n" );

	if( JobPid < 0 ) {
		// We haven't forwarded the job yet, probably because we're still
		// doing file transfer.  Since we're all done, just return true;
		// the FileTransfer object will clean itself up.
		return true;
	}

	requested_exit = true;

	// TODO Call cleanup script
	//   Wait for remote side to acknowledge removal?
	RemoveRemoteJob();

	// If rm_kill_sig is not SIGKILL, the process may linger.  Returning
	// false indicates that shutdown is pending.
	return false;
}


bool RemoteProc::ShutdownFast() {
	dprintf( D_ALWAYS, "RemoteProc::ShutdownFast()\n" );

	if( JobPid < 0 ) {
		// We haven't forwarded the job yet, probably because we're still
		// doing file transfer.  Since we're all done, just return true;
		// the FileTransfer object will clean itself up.
		return true;
	}

	// There's no point unpausing the container (and possibly swapping
	// it all back in again) if we're just going to be sending it a SIGKILL,
	// so don't bother to Continue() the process if it's been suspended.
	requested_exit = true;

	// TODO Call cleanup script
	//   Wait for remote side to acknowledge removal?
	RemoveRemoteJob();

	// Based on the other comments, you'd expect this to return true.
	// It could, but it's simpler to just to let the usual routines
	// handle the job clean-up than to duplicate them all here.
	return false;
}


void
RemoteProc::getStats( int /* timerID */ ) {
	// TODO read .status.ad file and update in-memory data
}

bool RemoteProc::PublishUpdateAd( ClassAd * ad ) {
	// TODO Use data from .status.ad file

	std::string status_file = Starter->GetWorkingDir(0);
	status_file += "/.job.ad.out";
	FILE *status_fp = safe_fopen_wrapper_follow(status_file.c_str(), "r");
	if ( status_fp != NULL ) {
		bool is_eof = false;
		int error = 0;
		std::ignore = InsertFromFile(status_fp, *ad, is_eof, error);
	} else {
		dprintf( D_FULLDEBUG, "RemoteProc: failed to open status file\n" );
	}
	bool exit_by_signal = false;
	ad->LookupBool(ATTR_ON_EXIT_BY_SIGNAL, exit_by_signal);
	if (exit_by_signal) {
		int code;
		if (ad->LookupInteger(ATTR_ON_EXIT_SIGNAL, code)) {
			exit_status = code;
		}
	} else {
		int code;
		if (ad->LookupInteger(ATTR_ON_EXIT_CODE, code)) {
			exit_status = code << 8;
		}
	}
	//
	// If we want to use the existing reporting code (probably a good
	// idea), we'll need to make sure that m_proc_exited, is_checkpointed,
	// is_suspended, num_pids, and dumped_core are set for OsProc; and that
	// job_start_time, job_exit_time, and exit_status are set.
	//
	// We set is_suspended and num_pids already, except for the TODO above.
	// DockerProc::JobReaper() already sets m_proc_exited, exit_code, and
	// dumped_core (indirectly, via OsProc::JobReaper()).
	//
	// We will need to set is_checkpointed appropriately when we support it.
	//
	// TODO: We could approximate job_start_time and job_exit_time internally,
	// or set them during our status polling.
	//

//	if (memUsage > 0) {
		// Set RSS, Memory and ImageSize to same values, best we have
//		ad->Assign(ATTR_RESIDENT_SET_SIZE, int(memUsage / 1024));
//		ad->Assign(ATTR_MEMORY_USAGE, int(memUsage / (1024 * 1024)));
//		ad->Assign(ATTR_IMAGE_SIZE, int(memUsage / (1024 * 1024)));
//		ad->Assign(ATTR_NETWORK_IN, double(netIn) / (1000 * 1000));
//		ad->Assign(ATTR_NETWORK_OUT, double(netOut) / (1000 * 1000));
//		ad->Assign(ATTR_JOB_REMOTE_USER_CPU, (int) (userCpu / (1000l * 1000l * 1000l)));
//		ad->Assign(ATTR_JOB_REMOTE_SYS_CPU, (int) (sysCpu / (1000l * 1000l * 1000l)));
//	}
	return UserProc::PublishUpdateAd( ad );
}


// TODO: Implement.
void RemoteProc::PublishToEnv( Env * /* env */ ) {
	dprintf( D_FULLDEBUG, "RemoteProc::PublishToEnv()\n" );
	return;
}

void RemoteProc::RemoveRemoteJob()
{
	ArgList args;
	Env env;
	InitWorkerArgs(args, env, "remove");

	TemporaryPrivSentry sentry(PRIV_USER);

	int rc = my_system(args, &env);

	dprintf(D_ALWAYS, "RemoteProc: remove command returned %d\n", rc);
}

void RemoteProc::InitWorkerArgs(ArgList &args, Env &env, const char *cmd)
{
	std::string worker_cmd;
	std::string remote_dir;
	param(worker_cmd, "STARTER_REMOTE_CMD");
	param(remote_dir, "STARTER_REMOTE_DIR");

	args.AppendArg(worker_cmd.c_str());
	args.AppendArg(cmd);
	args.AppendArg(remote_dir.c_str());
	args.AppendArg(m_remoteJobId.c_str());

	env.Import();
	env.SetEnv("_CONDOR_STARTER_REMOTE_DIR", remote_dir.c_str());
}
