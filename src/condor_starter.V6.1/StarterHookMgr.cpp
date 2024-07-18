/***************************************************************
 *
 * Copyright (C) 1990-2022, Condor Team, Computer Sciences Department,
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
#include "starter.h"
#include "StarterHookMgr.h"
#include "condor_attributes.h"
#include "hook_utils.h"
#include "status_string.h"
#include "classad_merge.h"
#include "jic_shadow.h"
#include "basename.h"

extern class Starter *Starter;

// // // // // // // // // // // //
// StarterHookMgr
// // // // // // // // // // // //

StarterHookMgr::StarterHookMgr()
	: JobHookClientMgr()
{
	m_hook_job_exit_timeout = 0;

	dprintf( D_FULLDEBUG, "Instantiating a StarterHookMgr\n" );
}


StarterHookMgr::~StarterHookMgr()
{
	dprintf( D_FULLDEBUG, "Deleting the StarterHookMgr\n" );

		// Delete our copies of the paths for each hook.
	clearHookPaths();
}


void
StarterHookMgr::clearHookPaths()
{
	m_hook_prepare_job.clear();
	m_hook_prepare_job_before_transfer.clear();
	m_hook_update_job_info.clear();
	m_hook_job_exit.clear();
}


bool
StarterHookMgr::reconfig()
{
	// Clear out our old copies of each hook's path.
	clearHookPaths();

	if (!getHookPath(HOOK_PREPARE_JOB, m_hook_prepare_job)) return false;
	if (!getHookPath(HOOK_PREPARE_JOB_BEFORE_TRANSFER, m_hook_prepare_job_before_transfer)) return false;
	if (!getHookPath(HOOK_UPDATE_JOB_INFO, m_hook_update_job_info)) return false;
	if (!getHookPath(HOOK_JOB_EXIT, m_hook_job_exit)) return false;

	CondorError err;
	if (!getHookArgs(HOOK_PREPARE_JOB, m_hook_prepare_jobs_args, err)) {
		dprintf(D_ALWAYS|D_FAILURE, "Failed to determine the PREPARE_JOB hooks: %s\n",
			err.getFullText().c_str());
		return false;
	}

	m_hook_job_exit_timeout = getHookTimeout(HOOK_JOB_EXIT, 30);

	return true;
}


int
StarterHookMgr::tryHookPrepareJob()
{
	if (m_hook_prepare_job.empty()) {
		return 0;
	}

	return tryHookPrepareJob_implementation(m_hook_prepare_job.c_str(), true);
}

int
StarterHookMgr::tryHookPrepareJobPreTransfer()
{
	if (m_hook_prepare_job_before_transfer.empty()) {
		return 0;
	}

	return tryHookPrepareJob_implementation(m_hook_prepare_job_before_transfer.c_str(), false);
}


int
StarterHookMgr::tryHookPrepareJob_implementation(const char *m_hook_prepare_job, bool after_filetransfer)
{
	ASSERT(m_hook_prepare_job);

	std::string hook_stdin;
	ClassAd* job_ad = Starter->jic->jobClassAd();
	sPrintAd(hook_stdin, *job_ad);

	HookClient* hook_client = new HookPrepareJobClient(m_hook_prepare_job, after_filetransfer);

	const char* hook_name = getHookTypeString(hook_client->type());

	Env env;
	Starter->PublishToEnv(&env);

	if (!spawn(hook_client, m_hook_prepare_jobs_args.Count() ? &m_hook_prepare_jobs_args : nullptr,
		hook_stdin, PRIV_USER_FINAL, &env))
	{
		std::string err_msg;
		formatstr(err_msg, "failed to execute %s (%s)",
						hook_name, m_hook_prepare_job);
		dprintf(D_ERROR,
				"ERROR in StarterHookMgr::tryHookPrepareJob_implementation: %s\n",
				err_msg.c_str());
		Starter->jic->notifyStarterError(err_msg.c_str(), true,
						 CONDOR_HOLD_CODE::HookPrepareJobFailure, 0);
		delete hook_client;
		return -1;
	}

	dprintf(D_ALWAYS, "%s (%s) invoked.\n",
			hook_name, m_hook_prepare_job);
	return 1;
}


bool
StarterHookMgr::hookUpdateJobInfo(ClassAd* job_info)
{
	if (m_hook_update_job_info.empty()) {
			// No need to dprintf() here, since this happens a lot.
		return false;
	}
	ASSERT(job_info);

	ClassAd update_ad(*(Starter->jic->jobClassAd()));
	MergeClassAds(&update_ad, job_info, true);

	std::string hook_stdin;
	sPrintAd(hook_stdin, update_ad);

		// Since we're not saving the output, this can just live on
        // the stack and be destroyed as soon as we return.
    HookClient client(HOOK_UPDATE_JOB_INFO, m_hook_update_job_info.c_str(), false);

	Env env;
	Starter->PublishToEnv(&env);

	if (!spawn(&client, NULL, hook_stdin, PRIV_USER_FINAL, &env)) {
		dprintf(D_ERROR,
				"ERROR in StarterHookMgr::hookUpdateJobInfo: "
				"failed to spawn HOOK_UPDATE_JOB_INFO (%s)\n",
				m_hook_update_job_info.c_str());
		return false;
	}

	dprintf(D_ALWAYS, "HOOK_UPDATE_JOB_INFO (%s) invoked.\n",
			m_hook_update_job_info.c_str());
	return true;
}


int
StarterHookMgr::tryHookJobExit(ClassAd* job_info, const char* exit_reason)
{
	if (m_hook_job_exit.empty()) {
		static bool logged_not_configured = false;
		if (!logged_not_configured) {
			dprintf(D_FULLDEBUG, "HOOK_JOB_EXIT not configured.\n");
			logged_not_configured = true;
		}
		return 0;
	}

		// Next, in case of retry, make sure we didn't already spawn
		// this hook and are just waiting for it to return.
	for (const HookClient *hook_client : m_client_list) {
		if (hook_client->type() == HOOK_JOB_EXIT) {
			dprintf(D_FULLDEBUG,
					"StarterHookMgr::tryHookJobExit() retried while still "
					"waiting for HOOK_JOB_EXIT to return - ignoring\n");
				// We want to return 1 here to indicate to the JIC
				// that the hook is currently running and it should
				// wait before moving on.
			return 1;
		}
	}

	ASSERT(job_info);
	ASSERT(exit_reason);

	std::string hook_stdin;
	sPrintAd(hook_stdin, *job_info);

	ArgList args;
	args.AppendArg(exit_reason);

	HookClient *hook_client = new HookJobExitClient(m_hook_job_exit.c_str());

	Env env;
	Starter->PublishToEnv(&env);

	if (!spawn(hook_client, &args, hook_stdin, PRIV_USER_FINAL, &env)) {
		dprintf(D_ERROR,
				"ERROR in StarterHookMgr::tryHookJobExit: "
				"failed to spawn HOOK_JOB_EXIT (%s)\n", m_hook_job_exit.c_str());
		delete hook_client;
		return -1;
	}

	dprintf(D_ALWAYS, "HOOK_JOB_EXIT (%s) invoked with reason: \"%s\"\n",
			m_hook_job_exit.c_str(), exit_reason);
	return 1;
}



// // // // // // // // // // // //
// HookPrepareJobClient class
// // // // // // // // // // // //

HookPrepareJobClient::HookPrepareJobClient(const char* hook_path, bool after_filetransfer)
	: HookClient(after_filetransfer ? HOOK_PREPARE_JOB : HOOK_PREPARE_JOB_BEFORE_TRANSFER, hook_path, true)
{
	// Nothing special needed in ctor
}


void
HookPrepareJobClient::hookExited(int exit_status) {
	std::string hook_name(getHookTypeString(type()));
	HookClient::hookExited(exit_status);

	// Make an update ad from the stdout of the hook
	ClassAd updateAd;
	std::string out(*getStdOut());
	if (!out.empty()) {
		initAdFromString(out.c_str(), updateAd);
		dprintf(D_FULLDEBUG, "%s output classad\n", hook_name.c_str());
		dPrintAd(D_FULLDEBUG, updateAd);
	}
	else {
		dprintf(D_FULLDEBUG, "%s output classad was empty (no job ad updates requested)\n", hook_name.c_str());
	}

	// If present, HookStatusCode attr in the stdout overrides exit status
	// unless killed by a signal.
	std::string exit_status_msg;
	statusString(exit_status, exit_status_msg);
	if (WIFSIGNALED(exit_status)) {
		exit_status = -1 * WTERMSIG(exit_status);
	}
	else {
		exit_status = WEXITSTATUS(exit_status); 
		int exit_from_stdout = -1;
		updateAd.LookupInteger("HookStatusCode", exit_from_stdout);
		if (exit_from_stdout >= 0) {
			exit_status = exit_from_stdout;
			formatstr(exit_status_msg, "reported status %03d", exit_status);
		}
	}

	// Create a log message
	std::string msg_from_stdout;
	updateAd.LookupString("HookStatusMessage", msg_from_stdout);
	std::string log_msg;
	formatstr(log_msg, "%s (%s) %s (%s): %s", hook_name.c_str(),
		condor_basename(m_hook_path),
		exit_status ? "failed" : "succeeded",
		exit_status_msg.c_str(),
		msg_from_stdout.empty() ? "<no message>" : msg_from_stdout.c_str());

	/* Notify the AP what happened if there is a message or a failure.
		From HTCONDOR-1411:
		HookStatusCode between 1 and 299 (inclusive) will result in the job
		going to Hold state, while a HookStatusCode > 299 will result in a shadow
		exception and the job going back to Idle state.  In either case, an event
		will be written into the job event log that includes the HookStatusMessage.
		If the HookStatusCode is 0 (success) and yet a HookStatusMessage is returned,
		the message will be written into the job event log as a warning, but the
		job will remain in Running state and job launch will continue.
	*/
	if (!msg_from_stdout.empty() || exit_status != 0) {
		Starter->jic->notifyStarterError(
			log_msg.c_str(),
			exit_status != 0 ? true : false, // shadow except or not (if hold code below is zero)
			exit_status > 0 && exit_status < 300 ? CONDOR_HOLD_CODE::HookPrepareJobFailure : 0, // hold job or not
			exit_status   // subcode 
		);
	}

	if (exit_status != 0) {
			// HOOK RETURNED FAILURE

		dprintf(D_ERROR,
				"ERROR in HookPrepareJobClient::hookExited: %s\n",
				log_msg.c_str());
		Starter->RemoteShutdownFast(0);
	}
	else {
			// HOOK RETURNED SUCCESS

			// Insert each expr from the update ad from hook into the job ad
		ClassAd* job_ad = Starter->jic->jobClassAd();
		job_ad->Update(updateAd);
		dprintf(D_FULLDEBUG, "After %s: merged job classad:\n", hook_name.c_str());
		dPrintAd(D_FULLDEBUG, *job_ad);

			// Now have the starter continue forward preparing for the job
		if (type() == HOOK_PREPARE_JOB) {
			Starter->jobEnvironmentReady();
		}
		if (type() == HOOK_PREPARE_JOB_BEFORE_TRANSFER) {
			JICShadow *p = dynamic_cast<JICShadow*>(Starter->jic);
			if (p) p->setupJobEnvironment_part2();
		}
	}
}


// // // // // // // // // // // //
// HookJobExitClient class
// // // // // // // // // // // //

HookJobExitClient::HookJobExitClient(const char* hook_path)
	: HookClient(HOOK_JOB_EXIT, hook_path, true)
{
		// Nothing special needed in the child class.
}


void
HookJobExitClient::hookExited(int exit_status) {
	HookClient::hookExited(exit_status);
		// Tell the JIC that it can mark allJobsDone() finished.
	Starter->jic->finishAllJobsDone();
	Starter->StarterExit(0);
}
