/***************************************************************
 *
 * Copyright (C) 1990-2008, Condor Team, Computer Sciences Department,
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

extern Starter *Starter;


// // // // // // // // // // // //
// StarterHookMgr
// // // // // // // // // // // //

StarterHookMgr::StarterHookMgr()
	: HookClientMgr()
{
	m_hook_keyword = NULL;
	m_hook_prepare_job = NULL;
	m_hook_update_job_info = NULL;
	m_hook_job_exit = NULL;

	m_hook_job_exit_timeout = 0;

	dprintf( D_FULLDEBUG, "Instantiating a StarterHookMgr\n" );
}


StarterHookMgr::~StarterHookMgr()
{
	dprintf( D_FULLDEBUG, "Deleting the StarterHookMgr\n" );

		// Delete our copies of the paths for each hook.
	clearHookPaths();

	if (m_hook_keyword) {
		free(m_hook_keyword);
	}
}


void
StarterHookMgr::clearHookPaths()
{
	if (m_hook_prepare_job) {
		free(m_hook_prepare_job);
        m_hook_prepare_job = NULL;
	}
	if (m_hook_update_job_info) {
		free(m_hook_update_job_info);
        m_hook_update_job_info = NULL;
	}
	if (m_hook_job_exit) {
		free(m_hook_job_exit);
        m_hook_job_exit = NULL;
	}
}


bool
StarterHookMgr::initialize(ClassAd* job_ad)
{
	char* tmp = param("STARTER_JOB_HOOK_KEYWORD");
	if (tmp) {
		m_hook_keyword = tmp;
		dprintf(D_FULLDEBUG, "Using STARTER_JOB_HOOK_KEYWORD value from config file: \"%s\"\n", m_hook_keyword);
	}
	else if (!job_ad->LookupString(ATTR_HOOK_KEYWORD, &m_hook_keyword)) {
		dprintf(D_FULLDEBUG,
				"Job does not define %s, not invoking any job hooks.\n",
				ATTR_HOOK_KEYWORD);
		return true;
	}
	else {
		dprintf(D_FULLDEBUG,
				"Using %s value from job ClassAd: \"%s\"\n",
				ATTR_HOOK_KEYWORD, m_hook_keyword);
	}
	if (!reconfig()) return false;
	return HookClientMgr::initialize();
}


bool
StarterHookMgr::reconfig()
{
	// Clear out our old copies of each hook's path.
	clearHookPaths();

    if (!getHookPath(HOOK_PREPARE_JOB, m_hook_prepare_job)) return false;
    if (!getHookPath(HOOK_UPDATE_JOB_INFO, m_hook_update_job_info)) return false;
    if (!getHookPath(HOOK_JOB_EXIT, m_hook_job_exit)) return false;

	m_hook_job_exit_timeout = getHookTimeout(HOOK_JOB_EXIT, 30);

	return true;
}


bool StarterHookMgr::getHookPath(HookType hook_type, char*& hpath)
{
    hpath = NULL;
	if (!m_hook_keyword) return true;
	std::string _param;
	formatstr(_param, "%s_HOOK_%s", m_hook_keyword, getHookTypeString(hook_type));
	return validateHookPath(_param.c_str(), hpath);
}


int StarterHookMgr::getHookTimeout(HookType hook_type, int def_value)
{
	if (!m_hook_keyword) return 0;
	std::string _param;
	formatstr(_param, "%s_HOOK_%s_TIMEOUT", m_hook_keyword, getHookTypeString(hook_type));
	return param_integer(_param.c_str(), def_value);
}


int
StarterHookMgr::tryHookPrepareJob()
{
	if (!m_hook_prepare_job) {
		dprintf(D_FULLDEBUG, "HOOK_PREPARE_JOB not configured.\n");
		return 0;
	}

	std::string hook_stdin;
	ClassAd* job_ad = Starter->jic->jobClassAd();
	sPrintAd(hook_stdin, *job_ad);

	HookClient* hook_client = new HookPrepareJobClient(m_hook_prepare_job);

	Env env;
	Starter->PublishToEnv(&env);

	if (!spawn(hook_client, NULL, hook_stdin, PRIV_USER_FINAL, &env)) {
		std::string err_msg;
		formatstr(err_msg, "failed to execute HOOK_PREPARE_JOB (%s)",
						m_hook_prepare_job);
		dprintf(D_ALWAYS|D_FAILURE,
				"ERROR in StarterHookMgr::tryHookPrepareJob: %s\n",
				err_msg.c_str());
		Starter->jic->notifyStarterError(err_msg.c_str(), true,
						 CONDOR_HOLD_CODE_HookPrepareJobFailure, 0);
		delete hook_client;
		return -1;
	}

	dprintf(D_FULLDEBUG, "HOOK_PREPARE_JOB (%s) invoked.\n",
			m_hook_prepare_job);
	return 1;
}


bool
StarterHookMgr::hookUpdateJobInfo(ClassAd* job_info)
{
	if (!m_hook_update_job_info) {
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
    HookClient client(HOOK_UPDATE_JOB_INFO, m_hook_update_job_info, false);

	Env env;
	Starter->PublishToEnv(&env);

	if (!spawn(&client, NULL, hook_stdin, PRIV_USER_FINAL, &env)) {
		dprintf(D_ALWAYS|D_FAILURE,
				"ERROR in StarterHookMgr::hookUpdateJobInfo: "
				"failed to spawn HOOK_UPDATE_JOB_INFO (%s)\n",
				m_hook_update_job_info);
		return false;
	}

	dprintf(D_FULLDEBUG, "HOOK_UPDATE_JOB_INFO (%s) invoked.\n",
			m_hook_update_job_info);
	return true;
}


int
StarterHookMgr::tryHookJobExit(ClassAd* job_info, const char* exit_reason)
{
	if (!m_hook_job_exit) {
		static bool logged_not_configured = false;
		if (!logged_not_configured) {
			dprintf(D_FULLDEBUG, "HOOK_JOB_EXIT not configured.\n");
			logged_not_configured = true;
		}
		return 0;
	}

	HookClient *hook_client;

		// Next, in case of retry, make sure we didn't already spawn
		// this hook and are just waiting for it to return.
    m_client_list.Rewind();
    while (m_client_list.Next(hook_client)) {
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

	hook_client = new HookJobExitClient(m_hook_job_exit);

	Env env;
	Starter->PublishToEnv(&env);

	if (!spawn(hook_client, &args, hook_stdin, PRIV_USER_FINAL, &env)) {
		dprintf(D_ALWAYS|D_FAILURE,
				"ERROR in StarterHookMgr::tryHookJobExit: "
				"failed to spawn HOOK_JOB_EXIT (%s)\n", m_hook_job_exit);
		delete hook_client;
		return -1;
	}

	dprintf(D_FULLDEBUG, "HOOK_JOB_EXIT (%s) invoked with reason: \"%s\"\n",
			m_hook_job_exit, exit_reason);
	return 1;
}



// // // // // // // // // // // //
// HookPrepareJobClient class
// // // // // // // // // // // //

HookPrepareJobClient::HookPrepareJobClient(const char* hook_path)
	: HookClient(HOOK_PREPARE_JOB, hook_path, true)
{
		// Nothing special needed in the child class.
}


void
HookPrepareJobClient::hookExited(int exit_status) {
	HookClient::hookExited(exit_status);
	if (WIFSIGNALED(exit_status) || WEXITSTATUS(exit_status) != 0) {
		std::string status_msg;
		statusString(exit_status, status_msg);
		int subcode;
		if (WIFSIGNALED(exit_status)) {
			subcode = -1 * WTERMSIG(exit_status);
		}
		else {
			subcode = WEXITSTATUS(exit_status);
		}
		std::string err_msg;
		formatstr(err_msg, "HOOK_PREPARE_JOB (%s) failed (%s)", m_hook_path,
						status_msg.c_str());
		dprintf(D_ALWAYS|D_FAILURE,
				"ERROR in StarterHookMgr::tryHookPrepareJob: %s\n",
				err_msg.c_str());
		Starter->jic->notifyStarterError(err_msg.c_str(), true,
							 CONDOR_HOLD_CODE_HookPrepareJobFailure, subcode);
		Starter->RemoteShutdownFast(0);
	}
	else {
			// Make an update ad from the stdout of the hook
		std::string out(*getStdOut());
		ClassAd updateAd;
		initAdFromString(out.c_str(), updateAd);
		dprintf(D_FULLDEBUG, "Prepare hook output classad\n");
		dPrintAd(D_FULLDEBUG, updateAd);

			// Insert each expr from the update ad into the job ad
		ClassAd* job_ad = Starter->jic->jobClassAd();
		job_ad->Update(updateAd);
		dprintf(D_FULLDEBUG, "After Prepare hook: merged job classad:\n");
		dPrintAd(D_FULLDEBUG, *job_ad);
		Starter->jobEnvironmentReady();
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
