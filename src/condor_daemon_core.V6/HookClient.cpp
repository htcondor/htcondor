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
#include "condor_daemon_core.h"
#include "HookClient.h"
#include "status_string.h"


HookClient::HookClient(HookType hook_type, const char* hook_path,
					   bool wants_output)
{
	m_hook_path = strdup(hook_path);
	m_hook_type = hook_type;
	m_pid = -1;
	m_exit_status = -1;
	m_has_exited = false;
	m_wants_output = wants_output;
}


HookClient::~HookClient() {
	if (m_hook_path) {
		free(m_hook_path);
		m_hook_path = NULL;
	}
	if (m_pid != -1 && !m_has_exited) {
			// TODO
			// kill -9 m_pid
	}
}


MyString*
HookClient::getStdOut() {
	if (m_has_exited) {
		return &m_std_out;
	}
	return daemonCore->Read_Std_Pipe(m_pid, 1);
}


MyString*
HookClient::getStdErr() {
	if (m_has_exited) {
		return &m_std_err;
	}
	return daemonCore->Read_Std_Pipe(m_pid, 2);
}


void
HookClient::hookExited(int exit_status) {
	m_has_exited = true;
	m_exit_status = exit_status;

	std::string status_txt;
	formatstr(status_txt, "HookClient %s (pid %d) ", m_hook_path, m_pid);
	statusString(exit_status, status_txt);
	dprintf(D_FULLDEBUG, "%s\n", status_txt.c_str());

	MyString* std_out = daemonCore->Read_Std_Pipe(m_pid, 1);
	if (std_out) {
		m_std_out = *std_out;
	}
	MyString* std_err = daemonCore->Read_Std_Pipe(m_pid, 2);
	if (std_err) {
		m_std_err = *std_err;
	}
}
