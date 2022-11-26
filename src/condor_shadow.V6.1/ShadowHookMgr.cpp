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


namespace {
std::string getCredDir()
{
	auto job_ad = Shadow->getJobAd();
	if (!job_ad) {
		dprintf(D_ALWAYS|D_FAILURE, "Shadow does not have a copy of the job ad.\n");
		return "";
	}
	std::string spool;
	if (!param(spool, "SPOOL")) {
		dprintf(D_ALWAYS|D_FAILURE, "Cannot create necessary spool directory because SPOOL is unset.\n");
		return "";
	}
	int cluster, proc;
	if (!job_ad->EvaluateAttrInt(ATTR_CLUSTER_ID, cluster) || !job_ad->EvaluateAttrInt(ATTR_PROC_ID, proc)) {
		dprintf(D_ALWAYS|D_FAILURE, "Cannot create necessary spool directory because job cluster / proc is unset\n");
		return "";
	}
	std::string cred_dir;
	formatstr(cred_dir, "%s%c%d%c_condor_creds.cluster%d.proc%d", spool.c_str(), DIR_DELIM_CHAR, cluster % 10000,
		DIR_DELIM_CHAR, cluster, proc);
	return cred_dir;
}
}


ShadowHookMgr::ShadowHookMgr(ClassAd *job_ad)
	: HookClientMgr()
{
	dprintf(D_FULLDEBUG, "Instantiating a shadow hook manager.\n");

	// Rules for the shadow hook selection are identical to that in the starter;
	// see StarterHookMgr.cpp for more information
	if (param(m_hook_keyword, "SHADOW_JOB_HOOK_KEYWORD")) {
		dprintf(D_ALWAYS, "Using SHADOW_JOB_HOOK_KEYWORD from config file: \"%s\"\n", m_hook_keyword.c_str());
	}
	if (m_hook_keyword.empty() && job_ad->EvaluateAttrString(ATTR_HOOK_KEYWORD, m_hook_keyword)) {
		auto config_has_hook = false;
		for (int idx = 0; ; idx++) {
			auto hook_type = static_cast<HookType>(idx);
			if (getHookTypeString(hook_type) == nullptr) break;
			std::string tmp;
			getHookPath(hook_type, tmp);
			if (!tmp.empty()) {
				config_has_hook = true;
				break;
			}
		}
		if (config_has_hook) {
			dprintf(D_ALWAYS, "Using %s value from job ClassAd: \"%s\"\n", ATTR_HOOK_KEYWORD,
				m_hook_keyword.c_str());
		} else {
			dprintf(D_ALWAYS, "Ignoring %s value of \"%s\" from job ClassAd because hook not defined"
				" in config file\n", ATTR_HOOK_KEYWORD, m_hook_keyword.c_str());
		}
	}
	if (m_hook_keyword.empty() && param(m_hook_keyword, "SHADOW_DEFAULT_JOB_HOOK_KEYWORD")) {
		dprintf(D_ALWAYS, "Using SHADOW_DEFAULT_JOB_HOOK_KEYWORD value from config file: \"%s\"\n",
			m_hook_keyword.c_str());
	}
	if (m_hook_keyword.empty()) {
		dprintf(D_FULLDEBUG, "Job does not define %s, no config file hooks, not invoking any job hooks.\n",
			ATTR_HOOK_KEYWORD);
		return;
	}

	reconfig();
	HookClientMgr::initialize();
}


ShadowHookMgr::~ShadowHookMgr()
{
		// Always try to delete the credential directory.
	std::string cred_dir;
	if ((cred_dir = getCredDir()) == "") {
		dprintf(D_ALWAYS|D_FAILURE, "Failed to generate directory to potentially cleanup\n");
	}
	{
		TemporaryPrivSentry sentry(PRIV_ROOT);
		StatInfo si(cred_dir.c_str());
		if (si.Error() != SINoFile) {
			Directory cred_dirp(cred_dir.c_str());
			cred_dirp.Remove_Entire_Directory();
		}
	}
}


bool
ShadowHookMgr::reconfig()
{
	m_hook_prepare_job = "";

	if (!getHookPath(HOOK_SHADOW_PREPARE_JOB, m_hook_prepare_job)) return false;

	return true;
}


bool
ShadowHookMgr::getHookPath(HookType hook_type, std::string &path)
{
	if (m_hook_keyword.empty()) return false;
	auto hook_string = getHookTypeString(hook_type);
	if (!hook_string) return false;

	std::string param = m_hook_keyword + "_HOOK_" + hook_string;

	char *hpath;
	auto result = validateHookPath(param.c_str(), hpath);
	if (hpath) {
		path = hpath;
		free(hpath);
	}
	return result;
}


int
ShadowHookMgr::getHookTimeout(HookType hook_type, int def_value)
{
	if (m_hook_keyword.empty()) return 0;
	std::string param = m_hook_keyword + "_HOOK_" + getHookTypeString(hook_type) + "_TIMEOUT";
	return param_integer(param.c_str(), def_value);
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
		dprintf(D_ALWAYS|D_FAILURE, "Shadow does not have a copy of the job ad.\n");
		return -1;
	}

	auto hook_client = new HookShadowPrepareJobClient(m_hook_prepare_job);
	auto hook_name = getHookTypeString(hook_client->type());

	Env env;
	std::string services_needed;
	if (job_ad->EvaluateAttrString(ATTR_OAUTH_SERVICES_NEEDED, services_needed)) {
		dprintf(D_FULLDEBUG, "Will populate credentials directory for hook.\n");
	} else {
		dprintf(D_FULLDEBUG, "No OAuth services are requested.\n");
	}
	bool send_krb5_credential = false;
	if (!job_ad->EvaluateAttrBool(ATTR_JOB_SEND_CREDENTIAL, send_krb5_credential)) {
		send_krb5_credential = false;
	}

	// Automatically delete the creds directory on failure.
	std::unique_ptr<std::string, void(*)(std::string*)> cred_dir_deleter(nullptr, [](std::string *path) {
		Directory cred_dirp(path->c_str(), PRIV_ROOT);
		cred_dirp.Remove_Entire_Directory();
	});

	std::string cred_dir;
	if (send_krb5_credential || !services_needed.empty()) {
		if ((cred_dir = getCredDir()) == "") {
			return -1;
		}
#ifdef WIN32
		if (!mkdir_and_parents_if_needed(cred_dir.c_str(), 0755, PRIV_USER))
#else
		if (!mkdir_and_parents_if_needed(cred_dir.c_str(), 0755, PRIV_CONDOR))
#endif
		{
			dprintf(D_ALWAYS|D_FAILURE, "Failed to create credentials directory %s for job hook: %s\n",
				cred_dir.c_str(), strerror(errno));
			return -1;
		}
		cred_dir_deleter.reset(&cred_dir);
		env.SetEnv("_CONDOR_CREDS", cred_dir.c_str());
	}

	std::string job_user;
	if (!job_ad->EvaluateAttrString(ATTR_USER, job_user)) {
		dprintf(D_ALWAYS|D_FAILURE, "Shadow copy of the job ad does not have user attribute.\n");
		return -1;
	}
	auto at_pos = job_user.find('@');
	auto user = job_user.substr(0, at_pos);
	
	if (send_krb5_credential) {
		std::string domain;
		if (at_pos != std::string::npos) {
			domain = job_user.substr(at_pos + 1);
		}
		int credlen;
		auto cred = getStoredCredential(STORE_CRED_USER_KRB, user.c_str(), domain.c_str(), credlen);
		if (!cred) {
			dprintf(D_ALWAYS|D_FAILURE, "Unable to read stored credential for shadow prepare job hook.\n");
			return -1;
		}

		std::string ccfilename;
		dircat(cred_dir.c_str(), user.c_str(), ".cc", ccfilename);
		{
			// Ideally, we write out credential directories as condor
			// and then chown the credential file itself over to the user;
			// this prevents the user from putting garbage into the credential
			// directory.
#ifdef WIN32
			TemporaryPrivSentry sentry(PRIV_USER);
#else
			TemporaryPrivSentry sentry(PRIV_CONDOR);
#endif
			if (!write_secure_file(ccfilename.c_str(), cred, credlen, false, false)) {
				dprintf(D_ALWAYS|D_FAILURE, "Failed to write out kerberos-style credential for shadow prepare job hook: %s\n", strerror(errno));
				return -1;
			}
		}
#ifndef WIN32
		{
			TemporaryPrivSentry sentry(PRIV_ROOT);
			if (-1 == chmod(ccfilename.c_str(), 0400)) {
				dprintf(D_ALWAYS|D_FAILURE, "Failed to chmod credential to 0400.\n");
				return -1;
			}
			if (-1 == chown(ccfilename.c_str(), get_user_uid(), get_user_gid())) {
				dprintf(D_ALWAYS|D_FAILURE, "Failed to chown credential to user.\n");
				return -1;
			}
		}
#endif
	}
	if (!services_needed.empty()) {
		StringList services_list(services_needed.c_str());
		services_list.rewind();
		char *curr;
		auto trust_cred_dir = param_boolean("TRUST_CREDENTIAL_DIRECTORY", false);
		std::string oauth_cred_dir;
		if (!param(oauth_cred_dir, "SEC_CREDENTIAL_DIRECTORY_OAUTH")) {
			dprintf(D_ALWAYS|D_FAILURE, "Unable to retrieve OAuth2-style credentials as SEC_CREDENTIAL_DIRECTORY_OAUTH is unset.\n");
			return -1;
		}

		while((curr = services_list.next())) {
			std::string fname, fullname;
			formatstr(fname, "%s.use", curr);
			replace_str(fname, "*", "_");

			formatstr(fullname, "%s%c%s%c%s", oauth_cred_dir.c_str(), DIR_DELIM_CHAR, user.c_str(), DIR_DELIM_CHAR, fname.c_str());
			const int verify_mode = trust_cred_dir ? 0 : SECURE_FILE_VERIFY_ALL;
			size_t len = 0;
			unsigned char *buf = nullptr;
			if (!read_secure_file(fullname.c_str(), (void**)(&buf), &len, true, verify_mode)) {
				dprintf(D_ALWAYS|D_FAILURE, "Failed to read credential file %s: %s\n",
					fullname.c_str(), errno ? strerror(errno) : "unknown error");
				return -1;
			}
			formatstr(fullname, "%s%c%s", cred_dir.c_str(), DIR_DELIM_CHAR, fname.c_str());
			{
#ifdef WIN32
				TemporaryPrivSentry sentry(PRIV_USER);
#else
				TemporaryPrivSentry sentry(PRIV_CONDOR);
#endif
				if (!write_secure_file(fullname.c_str(), buf, len, false, false)) {
					dprintf(D_ALWAYS|D_FAILURE, "Failed to write out credential file %s: %s\n",
						fullname.c_str(), strerror(errno));
					return -1;
				}
			}
#ifndef WIN32
			{
				TemporaryPrivSentry sentry(PRIV_ROOT);
				if (-1 == chmod(fullname.c_str(), 0400)) {
					dprintf(D_ALWAYS|D_FAILURE, "Failed to chmod OAuth2 credential to 0400.\n");
					return -1;
				}
				if (-1 == chown(fullname.c_str(), get_user_uid(), get_user_gid())) {
					dprintf(D_ALWAYS|D_FAILURE, "Failed to chown OAuth2 credential to user.\n");
					return -1;
				}
			}
#endif
		}
	}
	cred_dir_deleter.release();
	

	if (!spawn(hook_client, nullptr, hook_stdin, PRIV_USER_FINAL, &env)) {
		delete hook_client;
		std::string err_msg;
		formatstr(err_msg, "failed to execute %s (%s)", hook_name, m_hook_prepare_job.c_str());
		dprintf(D_ALWAYS|D_FAILURE, "ERROR in ShadowHookMgr::tryHookPrepareJob: %s\n",
			err_msg.c_str());
		BaseShadow::log_except("Job hook execution failed");
		Shadow->shutDown(JOB_NOT_STARTED);
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
		dprintf(D_ALWAYS|D_FAILURE, "Failed to generate directory to potentially cleanup\n");
	}
	{
		TemporaryPrivSentry sentry(PRIV_ROOT);
		StatInfo si(cred_dir.c_str());
		if (si.Error() != SINoFile) {
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
		dprintf(D_ALWAYS|D_FAILURE, "ERROR in HookPrepareJobClient::hookExited: %s\n", log_msg.c_str());
		if (exit_status < 300) {
			Shadow->holdJobAndExit(log_msg.c_str(), CONDOR_HOLD_CODE::HookShadowPrepareJobFailure, exit_status);
		} else {
			BaseShadow::log_except(log_msg.c_str());
			Shadow->shutDown(JOB_NOT_STARTED);
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
