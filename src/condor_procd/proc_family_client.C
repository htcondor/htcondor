/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_debug.h"
#include "proc_family_client.h"
#include "proc_family_io.h"

ProcFamilyClient::ProcFamilyClient(const char* addr) :
	m_client(addr)
{	
	m_pid = getpid();
}

bool
ProcFamilyClient::register_subfamily(pid_t root_pid,
                                     pid_t watcher_pid,
                                     int max_snapshot_interval,
                                     PidEnvID* penvid,
                                     const char* login)
{
	// figure out how big a buffer we need for this message;
	// the format will be:
	//   - the root pid of the new subfamily
	//   - the pid the will be querying / controllering this subfamily
	//   - the requested maximum snapshot interval
	//   - size of penvid (0 if not present)
	//   - penvid (if present)
	//   - size of login (0 if not present)
	//   - login (if present)
	//
	int penvid_len = (penvid != NULL) ? sizeof(PidEnvID) : 0;
	int login_len = (login != NULL) ? (strlen(login) + 1) : 0;
	int message_len = sizeof(proc_family_command_t) +
	                  sizeof(pid_t) +
	                  sizeof(pid_t) +
	                  sizeof(int) +
	                  sizeof(int) +
	                  penvid_len +
	                  sizeof(int) +
	                  login_len;
	void* buffer = malloc(message_len);
	ASSERT(buffer != NULL);
	char* ptr = (char*)buffer;
	
	// fill in the commmand id
	//
	*(proc_family_command_t*)ptr = PROC_FAMILY_REGISTER_SUBFAMILY;
	ptr += sizeof(proc_family_command_t);

	// fill in the root pid
	//
	*(pid_t*)ptr = root_pid;
	ptr += sizeof(pid_t);

	// fill in the watcher pid
	//
	*(pid_t*)ptr = watcher_pid;
	ptr += sizeof(pid_t);

	// fill in the requested maximum snapshot interval
	//
	*(int*)ptr = max_snapshot_interval;
	ptr += sizeof(int);

	// fill in the PidEnvID info
	//
	if (penvid) {
		*(int*)ptr = sizeof(PidEnvID);
		ptr += sizeof(int);
		pidenvid_copy((PidEnvID*)ptr, penvid);
		ptr += sizeof(PidEnvID);
	}
	else {
		*(int*)ptr = 0;
		ptr += sizeof(int);
	}

	// fill in the login info
	//
	if (login_len) {
		*(int*)ptr = login_len;
		ptr += sizeof(int);
		memcpy(ptr, login, login_len);
		ptr += login_len;
	}
	else {
		*(int*)ptr = 0;
		ptr += sizeof(int);
	}

	// quick sanity check
	//
	ASSERT(ptr - (char*)buffer == message_len);

	// send the message to the server
	//
	m_client.start_connection(buffer, message_len);
	free(buffer);

	// receive the boolean response from the server
	//
	bool ok;
	m_client.read_data(&ok, sizeof(bool));

	// done with this command
	//
	m_client.end_connection();

	return ok;
}

bool
ProcFamilyClient::get_usage(pid_t pid, ProcFamilyUsage& usage)
{
	int message_len = sizeof(proc_family_command_t) +
	                  sizeof(pid_t);
	void* buffer = malloc(message_len);
	ASSERT(buffer != NULL);
	char* ptr = (char*)buffer;

	*(proc_family_command_t*)ptr = PROC_FAMILY_GET_USAGE;
	ptr += sizeof(proc_family_command_t);

	*(pid_t*)ptr = pid;
	ptr += sizeof(pid_t);

	ASSERT(ptr - (char*)buffer == message_len);

	m_client.start_connection(buffer, message_len);

	bool ok;
	m_client.read_data(&ok, sizeof(ok));
	if (ok) {
		m_client.read_data(&usage, sizeof(ProcFamilyUsage));
	}
	m_client.end_connection();

	return ok;
}

bool
ProcFamilyClient::signal_process(pid_t pid, int sig)
{
	int message_len = sizeof(proc_family_command_t) +
	                  sizeof(pid_t) +
	                  sizeof(int);
	void* buffer = malloc(message_len);
	ASSERT(buffer != NULL);
	char* ptr = (char*)buffer;

	*(proc_family_command_t*)ptr = PROC_FAMILY_SIGNAL_PROCESS;
	ptr += sizeof(proc_family_command_t);

	*(pid_t*)ptr = pid;
	ptr += sizeof(pid_t);

	*(int*)ptr = sig;
	ptr += sizeof(int);

	ASSERT(ptr - (char*)buffer == message_len);
	
	m_client.start_connection(buffer, message_len);

	bool ok;
	m_client.read_data(&ok, sizeof(bool));
	m_client.end_connection();

	return ok;
}

bool
ProcFamilyClient::kill_family(pid_t pid)
{
	return signal_family(pid, PROC_FAMILY_KILL_FAMILY);
}

bool
ProcFamilyClient::suspend_family(pid_t pid)
{
	return signal_family(pid, PROC_FAMILY_SUSPEND_FAMILY);
}

bool
ProcFamilyClient::continue_family(pid_t pid)
{
	return signal_family(pid, PROC_FAMILY_CONTINUE_FAMILY);
}

bool
ProcFamilyClient::signal_family(pid_t pid, proc_family_command_t command)
{
	int message_len = sizeof(proc_family_command_t) +
	                  sizeof(pid_t);
	void* buffer = malloc(message_len);
	ASSERT(buffer != NULL);
	char* ptr = (char*)buffer;

	*(proc_family_command_t*)ptr = command;
	ptr += sizeof(proc_family_command_t);

	*(pid_t*)ptr = pid;
	ptr += sizeof(pid_t);

	ASSERT(ptr - (char*)buffer == message_len);
	
	m_client.start_connection(buffer, message_len);

	bool ok;
	m_client.read_data(&ok, sizeof(bool));

	m_client.end_connection();

	return ok;
}

bool
ProcFamilyClient::unregister_family(pid_t pid)
{
	int message_len = sizeof(proc_family_command_t) +
	                  sizeof(pid_t);
	void* buffer = malloc(message_len);
	ASSERT(buffer != NULL);
	char* ptr = (char*)buffer;

	*(proc_family_command_t*)ptr = PROC_FAMILY_UNREGISTER_FAMILY;
	ptr += sizeof(proc_family_command_t);

	*(pid_t*)ptr = pid;
	ptr += sizeof(pid_t);

	m_client.start_connection(buffer, message_len);

	bool ok;
	m_client.read_data(&ok, sizeof(bool));

	m_client.end_connection();

	return ok;
}

bool
ProcFamilyClient::snapshot()
{
	proc_family_command_t command = PROC_FAMILY_TAKE_SNAPSHOT;

	m_client.start_connection(&command, sizeof(proc_family_command_t));

	bool ok;
	m_client.read_data(&ok, sizeof(bool));

	m_client.end_connection();

	return ok;
}

bool
ProcFamilyClient::quit()
{
	proc_family_command_t command = PROC_FAMILY_QUIT;

	m_client.start_connection(&command, sizeof(proc_family_command_t));

	bool ok;
	m_client.read_data(&ok, sizeof(bool));

	m_client.end_connection();

	return ok;
}

#if defined(PROCD_DEBUG)

LocalClient*
ProcFamilyClient::dump(pid_t pid)
{
	int message_len = sizeof(proc_family_command_t) +
	                  sizeof(pid_t);
	void* buffer = malloc(message_len);
	ASSERT(buffer != NULL);
	char* ptr = (char*)buffer;

	*(proc_family_command_t*)ptr = PROC_FAMILY_DUMP;
	ptr += sizeof(proc_family_command_t);

	*(pid_t*)ptr = pid;
	ptr += sizeof(pid_t);

	ASSERT(ptr - (char*)buffer == message_len);

	m_client.start_connection(buffer, message_len);

	bool ok;
	m_client.read_data(&ok, sizeof(bool));
	if (!ok) {
		m_client.end_connection();
		return NULL;
	}

	return &m_client;
}

#endif
