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

#include "basename.h"
#include "cred_dir.h"
#include "condor_attributes.h"
#include "condor_holdcodes.h"
#include "directory.h"
#include "hook_utils.h"
#include "shadow.h"
#include "secure_file.h"
#include "ShadowHookMgr.h"
#include "status_string.h"
#include "store_cred.h"

extern BaseShadow *Shadow;

std::string getCredDir()
{
	auto job_ad = Shadow->getJobAd();
	if (!job_ad) {
		dprintf(D_ERROR, "Shadow does not have a copy of the job ad.\n");
		return "";
	}
	std::string spool;
	if (!param(spool, "SPOOL")) {
		dprintf(D_ERROR, "Cannot create necessary spool directory because SPOOL is unset.\n");
		return "";
	}
	int cluster, proc;
	if (!job_ad->EvaluateAttrInt(ATTR_CLUSTER_ID, cluster) || !job_ad->EvaluateAttrInt(ATTR_PROC_ID, proc)) {
		dprintf(D_ERROR, "Cannot create necessary spool directory because job cluster / proc is unset\n");
		return "";
	}
	std::string cred_dir;
	formatstr(cred_dir, "%s%c%d%c_condor_creds.cluster%d.proc%d", spool.c_str(), DIR_DELIM_CHAR, cluster % 10000,
		DIR_DELIM_CHAR, cluster, proc);
	return cred_dir;
}

ShadowHookMgr::ShadowHookMgr()
	: JobHookClientMgr()
{}


ShadowHookMgr::~ShadowHookMgr()
{
		// Always try to delete the credential directory.
	std::string cred_dir;
	if ((cred_dir = getCredDir()) == "") {
		dprintf(D_ERROR, "Failed to generate directory to potentially cleanup\n");
	}
	{
		TemporaryPrivSentry sentry(PRIV_ROOT);
		struct stat si{};
		if (stat(cred_dir.c_str(), &si) == 0) {
			Directory cred_dirp(cred_dir.c_str());
			cred_dirp.Remove_Entire_Directory();
		}
		rmdir(cred_dir.c_str());
	}
}


bool
ShadowHookMgr::reconfig()
{
	m_hook_prepare_job = "";

	if (!getHookPath(HOOK_SHADOW_PREPARE_JOB, m_hook_prepare_job)) return false;

	CondorError err;
	if (!getHookArgs(HOOK_SHADOW_PREPARE_JOB, m_args, err)) {
		dprintf(D_ALWAYS|D_FAILURE, "Failed to determine the SHADOW_PREPARE_JOB hooks: %s\n",
			err.getFullText().c_str());
		return false;
	}

	return true;
}


int
ShadowHookMgr::tryHookPrepareJob()
{
	if (m_hook_prepare_job.empty()) {
		return 0;
	}

	std::string hook_stdin;
	auto job_ad = Shadow->getJobAd();
	if (!job_ad) {
		dprintf(D_ERROR, "Shadow does not have a copy of the job ad.\n");
		return -1;
	}
	sPrintAd(hook_stdin, *job_ad);

	auto hook_client = new HookShadowPrepareJobClient(m_hook_prepare_job);
	auto hook_name = getHookTypeString(hook_client->type());

	Env env;
	// Note that condor_preen will clean up the directory in case if we crash
	// and don't do it inside the starter.
	auto cred_dir = getCredDir();
	if (cred_dir.empty()) {
		delete hook_client;
		return -1;
	}

	htcondor::ShadowHookCredDirCreator creds(*job_ad, cred_dir);
	CondorError err;
	if (!creds.PrepareCredDir(err)) {
		delete hook_client;
		return -1;
	}

	if (creds.MadeCredDir()) {
		env.SetEnv("_CONDOR_CREDS", cred_dir.c_str());
	}

	if (!spawn(hook_client, m_args.Count() ? &m_args : nullptr, hook_stdin, PRIV_USER_FINAL, &env)) {
		delete hook_client;
		std::string err_msg;
		formatstr(err_msg, "failed to execute %s (%s)", hook_name, m_hook_prepare_job.c_str());
		dprintf(D_ERROR, "ERROR in ShadowHookMgr::tryHookPrepareJob: %s\n",
			err_msg.c_str());
		BaseShadow::log_except("Job hook execution failed");
		Shadow->shutDown(JOB_NOT_STARTED, "Shadow prepare hook failed");
	}

	dprintf(D_ALWAYS, "%s (%s) invoked.\n", hook_name, m_hook_prepare_job.c_str());
	return 1;
}


HookShadowPrepareJobClient::HookShadowPrepareJobClient(const std::string &hook_path)
	: HookClient(HOOK_SHADOW_PREPARE_JOB, hook_path.c_str(), true)
{}

void
HookShadowPrepareJobClient::hookExited(int exit_status) {
	std::string hook_name(getHookTypeString(type()));
	HookClient::hookExited(exit_status);

		// Always try to delete the credential directory.
	std::string cred_dir;
	if ((cred_dir = getCredDir()) == "") {
		dprintf(D_ERROR, "Failed to generate directory to potentially cleanup\n");
	}
	{
		TemporaryPrivSentry sentry(PRIV_ROOT);
		struct stat si{};
		if (stat(cred_dir.c_str(), &si) == 0) {
			Directory cred_dirp(cred_dir.c_str());
			cred_dirp.Remove_Entire_Directory();
		}
		if (-1 == rmdir(cred_dir.c_str())) {
			dprintf(D_ALWAYS, "Failed to delete the credentials directory %s: %s\n",
				cred_dir.c_str(), strerror(errno));
		}
	}


	ClassAd updateAd;
	std::string out(*getStdOut());
	if (!out.empty()) {
		initAdFromString(out.c_str(), updateAd);
		dprintf(D_FULLDEBUG, "%s output classad\n", hook_name.c_str());
		dPrintAd(D_FULLDEBUG, updateAd);
	} else {
		dprintf(D_FULLDEBUG, "%s output classad was empty (no job ad updates requested)\n", hook_name.c_str());
	}

	std::string exit_status_msg;
	statusString(exit_status, exit_status_msg);
	if (WIFSIGNALED(exit_status)) {
		exit_status = -WTERMSIG(exit_status);
	} else {
		exit_status = WEXITSTATUS(exit_status);
		int exit_status_tmp;
		if (updateAd.EvaluateAttrInt("HookStatusCode", exit_status_tmp)) {
			exit_status = exit_status_tmp;
			formatstr(exit_status_msg, "reported status %03d", exit_status);
		}
	}

	std::string msg_from_stdout;
	updateAd.EvaluateAttrString("HookStatusMessage", msg_from_stdout);
	std::string log_msg;
	formatstr(log_msg, "%s (%s) %s (%s): %s", hook_name.c_str(),
		condor_basename(m_hook_path),
		exit_status ? "failed" : "succeeded",
		exit_status_msg.c_str(),
		msg_from_stdout.empty() ? "<no message>" : msg_from_stdout.c_str());

	if (exit_status) {
		dprintf(D_ERROR, "ERROR in HookPrepareJobClient::hookExited: %s\n", log_msg.c_str());
		if (exit_status < 300) {
			Shadow->holdJobAndExit(log_msg.c_str(), CONDOR_HOLD_CODE::HookShadowPrepareJobFailure, exit_status);
		} else {
			BaseShadow::log_except(log_msg.c_str());
			Shadow->shutDown(JOB_NOT_STARTED, "Shadow prepare hook failed");
		}
		return;
	}

	auto job_ad = Shadow->getJobAd();
	job_ad->Update(updateAd);

	auto uni_shadow = reinterpret_cast<UniShadow *>(Shadow);
		// Only the UniShadow will launch the ShadowHookMgr; hence assume it's still a UniShadow
		// in the callback.
	ASSERT(uni_shadow)
	uni_shadow->spawnFinish();
}
