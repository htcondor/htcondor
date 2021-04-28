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
#include "status_string.h"


HookClientMgr::HookClientMgr() {
	m_reaper_output_id = -1;
	m_reaper_ignore_id = -1;
}


HookClientMgr::~HookClientMgr() {
	HookClient *client;	
	m_client_list.Rewind();
	while (m_client_list.Next(client)) {
			// TODO: kill them, too?
		m_client_list.DeleteCurrent();
		delete client;
	}
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
    MyString ms(hook_stdin);
    return spawn(client, args, &ms, priv, env);
}

bool
HookClientMgr::spawn(HookClient* client, ArgList* args, MyString *hook_stdin, priv_state priv, Env *env) {
	int reaper_id;
	bool wants_output = client->wantsOutput();
	const char* hook_path = client->path();

	ArgList final_args;
	final_args.AppendArg(hook_path);
	if (args) {
		final_args.AppendArgsFromArgList(*args);
	}

    int std_fds[3] = {DC_STD_FD_NOPIPE, DC_STD_FD_NOPIPE, DC_STD_FD_NOPIPE};
    if (hook_stdin && hook_stdin->length()) {
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

	int pid = daemonCore->
		Create_Process(hook_path, final_args, priv,
					  reaper_id, FALSE, FALSE, env, NULL, &fi,
					  NULL, std_fds);
	client->setPid(pid);
	if (pid == FALSE) {
		dprintf( D_ALWAYS, "ERROR: Create_Process failed in HookClient::spawn()!\n");
		return false;
	}

		// If we've got initial input to write to stdin, do so now.
    if (hook_stdin && hook_stdin->length()) {
		daemonCore->Write_Stdin_Pipe(pid, hook_stdin->c_str(),
									 hook_stdin->length());
	}

	if (wants_output) {
		m_client_list.Append(client);
	}
	return true;
}


bool
HookClientMgr::remove(HookClient* client) {
    return m_client_list.Delete(client);
}


int
HookClientMgr::reaperOutput(int exit_pid, int exit_status)
{
		// First, make sure the hook didn't leak any processes.
	daemonCore->Kill_Family(exit_pid);

	bool found_it = false;
	HookClient *client;	
	m_client_list.Rewind();
	while (m_client_list.Next(client)) {
		if (exit_pid == client->getPid()) {
			found_it = true;
			break;
		}
	}

	if (!found_it) {
			// Uhh... now what?
		dprintf(D_ALWAYS|D_FAILURE, "Unexpected: HookClientMgr::reaper() "
				"called with pid %d but no HookClient found that matches.\n",
				exit_pid);
		return FALSE;
	}
	client->hookExited(exit_status);

		// Now that hookExited() returned, we need to delete this
		// client object and remove it from our list.
	m_client_list.DeleteCurrent();
	delete client;

	return TRUE;
}


int
HookClientMgr::reaperIgnore(int exit_pid, int exit_status)
{
		// First, make sure the hook didn't leak any processes.
	daemonCore->Kill_Family(exit_pid);

		// Some hook that we don't care about the output for just
		// exited.  All we need is to print a log message (if that).
	std::string status_txt;
	formatstr(status_txt, "Hook (pid %d) ", exit_pid);
	statusString(exit_status, status_txt);
	dprintf(D_FULLDEBUG, "%s\n", status_txt.c_str());
	return TRUE;
}
