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
#include "condor_config.h"
#include "condor_daemon_core.h"
#include "HookClientMgr.h"
#include "HookClient.h"
#include "hook_utils.h"
#include "status_string.h"
#include <algorithm>


HookClientMgr::HookClientMgr() {
	m_reaper_output_id = -1;
	m_reaper_ignore_id = -1;
}


HookClientMgr::~HookClientMgr() {
	for (HookClient *client: m_client_list) {
			// TODO: kill them, too?
		delete client;
	}
	m_client_list.clear();

	if (daemonCore && m_reaper_output_id != -1) {
		daemonCore->Cancel_Reaper(m_reaper_output_id);
	}
	if (daemonCore && m_reaper_ignore_id != -1) {
		daemonCore->Cancel_Reaper(m_reaper_ignore_id);
	}
}


bool
HookClientMgr::initialize() {
	m_reaper_output_id = daemonCore->
		Register_Reaper("HookClientMgr Output Reaper",
						(ReaperHandlercpp) &HookClientMgr::reaperOutput,
						"HookClientMgr Output Reaper", this);
	m_reaper_ignore_id = daemonCore->
		Register_Reaper("HookClientMgr Ignore Reaper",
						(ReaperHandlercpp) &HookClientMgr::reaperIgnore,
						"HookClientMgr Ignore Reaper", this);

	return (m_reaper_output_id != FALSE && m_reaper_ignore_id != FALSE);
}

bool
HookClientMgr::spawn(HookClient* client, ArgList* args, const std::string & hook_stdin, priv_state priv, Env *env) {
	int reaper_id;
	bool wants_output = client->wantsOutput();
	const char* hook_path = client->path();

	ArgList final_args;
	final_args.AppendArg(hook_path);
	if (args) {
		final_args.AppendArgsFromArgList(*args);
	}

    int std_fds[3] = {DC_STD_FD_NOPIPE, DC_STD_FD_NOPIPE, DC_STD_FD_NOPIPE};
    if (hook_stdin.length()) {
		std_fds[0] = DC_STD_FD_PIPE;
	}
	if (wants_output) {
		std_fds[1] = DC_STD_FD_PIPE;
		std_fds[2] = DC_STD_FD_PIPE;
		reaper_id = m_reaper_output_id;
	}
	else {
		reaper_id = m_reaper_ignore_id;
	}

		// Tell DaemonCore to register the process family so we can
		// safely kill everything from the reaper. 
	FamilyInfo fi;
	fi.max_snapshot_interval = param_integer("PID_SNAPSHOT_INTERVAL", 15);

	std::string create_process_error_msg;
	OptionalCreateProcessArgs cpArgs(create_process_error_msg);
	cpArgs.priv(priv).reaperID(reaper_id).env(env).std(std_fds);
	// Only set up family info if we want to utilize the procd
	// 		Don't want to for shadow hooks
	if (useProcd()) {
		cpArgs.familyInfo(&fi);
	}
	int pid = daemonCore->CreateProcessNew(hook_path, final_args, cpArgs);
	client->setPid(pid);
	if (pid == FALSE) {
		dprintf( D_ALWAYS, "ERROR: Create_Process failed in HookClient::spawn(): %s\n",
				 create_process_error_msg.c_str());
		return false;
	}

		// If we've got initial input to write to stdin, do so now.
    if (hook_stdin.length()) {
		daemonCore->Write_Stdin_Pipe(pid, hook_stdin.c_str(),
									 hook_stdin.length());
	}

	m_client_list.emplace_back(client);
	return true;
}


bool
HookClientMgr::remove(HookClient* client) {
	auto it = std::find(m_client_list.begin(), m_client_list.end(), client);
	if (it != m_client_list.end()) {
		m_client_list.erase(it);
		return true;
	} else {
		return false;
	}
}

int
HookClientMgr::reaperOutput(int exit_pid, int exit_status)
{
		// First, make sure the hook didn't leak any processes.
	if (useProcd()) {
		daemonCore->Kill_Family(exit_pid);
	}


	bool found_it = false;
	HookClient *client = nullptr;	
	for (HookClient *local_client : m_client_list) {
		client = local_client;
		if (exit_pid == client->getPid()) {
			found_it = true;
			break;
		}
	}

	if (!found_it) {
			// Uhh... now what?
		dprintf(D_ERROR, "Unexpected: HookClientMgr::reaper() "
				"called with pid %d but no HookClient found that matches.\n",
				exit_pid);
		return FALSE;
	}

		// Now that hookExited() returned, we need to delete this
		// client object and remove it from our list.
	auto it = std::find(m_client_list.begin(), m_client_list.end(), client);
	if (it != m_client_list.end()) {
		m_client_list.erase(it);
	}

	client->hookExited(exit_status);

	delete client;
	return TRUE;
}


int
HookClientMgr::reaperIgnore(int exit_pid, int exit_status)
{
		// First, make sure the hook didn't leak any processes.
	if (useProcd()) {
		daemonCore->Kill_Family(exit_pid);
	}

		// Some hook that we don't care about the output for just
		// exited.  All we need is to print a log message (if that).
	std::string status_txt;
	formatstr(status_txt, "Hook (pid %d) ", exit_pid);
	statusString(exit_status, status_txt);
	dprintf(D_FULLDEBUG, "%s\n", status_txt.c_str());
	return TRUE;
}


bool
JobHookClientMgr::initialize(classad::ClassAd* job_ad)
{
	// If the admin says we must use this hook, use it.
	if (param(m_hook_keyword, (paramPrefix() + "_JOB_HOOK_KEYWORD").c_str())) {
		dprintf(D_ALWAYS, "Using %s_JOB_HOOK_KEYWORD value from config file: \"%s\"\n",
			paramPrefix().c_str(), m_hook_keyword.c_str());
	}

	// If the admin did not insist on a hook, see if the job wants one.  However, if the job
	// specifies a hookname that does not exist at all in the config file,
	// then use the default hook provided by the EP admin. This prevents the user from bypassing
	// the EP admin's default hook by specifying an invalid hook name.
	if (m_hook_keyword.empty() && job_ad->EvaluateAttrString(ATTR_HOOK_KEYWORD, m_hook_keyword) ) {
		auto config_has_this_hook = false;
		for (int i = 0; !config_has_this_hook; i++) {
			auto h = static_cast<HookType>(i);
			if (getHookTypeString(h) == nullptr) break;  // iterated thru all hook types
			std::string tmp;
			getHookPath(h, tmp);
			if (!tmp.empty()) {
				config_has_this_hook = true;
				break;
			}
		}
		if (config_has_this_hook) {
			dprintf(D_ALWAYS, "Using %s value from job ClassAd: \"%s\"\n", ATTR_HOOK_KEYWORD,
				m_hook_keyword.c_str());
		} else {
			dprintf(D_ALWAYS, "Ignoring %s value of \"%s\" from job ClassAd because hook not defined"
				" in config file\n", ATTR_HOOK_KEYWORD, m_hook_keyword.c_str());
		}
	}

        // If we don't have a hook by now, see if the admin defined a default one.
        if (m_hook_keyword.empty() && param(m_hook_keyword, (paramPrefix() + "_DEFAULT_JOB_HOOK_KEYWORD").c_str())) {
                dprintf(D_ALWAYS, "Using %s_DEFAULT_JOB_HOOK_KEYWORD value from config file: \"%s\"\n",
			paramPrefix().c_str(), m_hook_keyword.c_str());
        }
        if (m_hook_keyword.empty()) {
                dprintf(D_FULLDEBUG, "Job does not define %s, no config file hooks, not invoking any job hooks.\n",
			ATTR_HOOK_KEYWORD);
		return true;
	}

	if (!reconfig()) {return false;}

	return HookClientMgr::initialize();
}


bool
JobHookClientMgr::getHookPath(HookType hook_type, std::string &path)
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
JobHookClientMgr::getHookTimeout(HookType hook_type, int def_value)
{
	if (m_hook_keyword.empty()) return 0;
	std::string param = m_hook_keyword + "_HOOK_" + getHookTypeString(hook_type) + "_TIMEOUT";
	return param_integer(param.c_str(), def_value);
}


bool
JobHookClientMgr::getHookArgs(HookType hook_type, ArgList &args, CondorError &err)
{
	if (m_hook_keyword.empty()) {
		return true;
	}

	const std::string param_name = m_hook_keyword + "_HOOK_" + getHookTypeString(hook_type) + "_ARGS";
	std::string arg_string;
	if (!param(arg_string, param_name.c_str())) {
		return true;
	}

	std::string err_msg;
	if (!args.AppendArgsV2Raw(arg_string.c_str(), err_msg)) {
		err.push("JOB_HOOK_MGR", 2, err_msg.c_str());
		return false;
	}
	return true;
}
