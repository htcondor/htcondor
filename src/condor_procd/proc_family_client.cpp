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
#include "condor_debug.h"
#include "proc_family_client.h"
#include "proc_family_io.h"
#include "local_client.h"

// helper function for logging the return code from a ProcD
// operation
//
static void
log_exit(const char* op_str, proc_family_error_t error_code)
{
	int debug_level = D_PROCFAMILY;
	if (error_code != PROC_FAMILY_ERROR_SUCCESS) {
		debug_level = D_ALWAYS;
	}
	const char* result = proc_family_error_lookup(error_code);
	if (result == NULL) {
		result = "Unexpected return code";
	}
	dprintf(debug_level,
	        "Result of \"%s\" operation from ProcD: %s\n",
	        op_str,
	        result);
}

bool
ProcFamilyClient::initialize(const char* addr)
{
	// create the LocalClient object for communicating with the ProcD
	//
	m_client = new LocalClient;
	assert(m_client != NULL);
	if (!m_client->initialize(addr)) {
		dprintf(D_ALWAYS,
		        "ProcFamilyClient: error initializing LocalClient\n");
		delete m_client;
		m_client = NULL;
		return false;
	}

	m_initialized = true;
	return true;
}

ProcFamilyClient::~ProcFamilyClient()
{
	// nothing to do if we're not initialized
	//
	if (!m_initialized) {
		return;
	}

	// delete our LocalClient object
	//
	delete m_client;
}

bool
ProcFamilyClient::register_subfamily(pid_t root_pid,
                                     pid_t watcher_pid,
                                     int max_snapshot_interval,
                                     bool& response)
{
	assert(m_initialized);

	dprintf(D_PROCFAMILY,
	        "About to register family for PID %u with the ProcD\n",
	        root_pid);

	int message_len = sizeof(proc_family_command_t) +
	                  sizeof(pid_t) +
	                  sizeof(pid_t) +
	                  sizeof(int);
	void* buffer = malloc(message_len);
	assert(buffer != NULL);
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

	// quick sanity check
	//
	assert(ptr - (char*)buffer == message_len);

	// make the RPC to the ProcD
	//
	if (!m_client->start_connection(buffer, message_len)) {
		dprintf(D_ALWAYS,
		        "ProcFamilyClient: failed to start connection with ProcD\n");
		free(buffer);
		return false;
	}
	free(buffer);
	proc_family_error_t err;
	if (!m_client->read_data(&err, sizeof(proc_family_error_t))) {
		dprintf(D_ALWAYS,
		        "ProcFamilyClient: failed to read response from ProcD\n");
		return false;
	}
	m_client->end_connection();

	log_exit("register_subfamily", err);
	response = (err == PROC_FAMILY_ERROR_SUCCESS);
	return true;
}

bool
ProcFamilyClient::track_family_via_environment(pid_t pid,
                                               PidEnvID& penvid,
                                               bool& response)
{
	assert(m_initialized);

	dprintf(D_PROCFAMILY,
	        "About to tell ProcD to track family with root %u "
	            "via environment\n",
	        pid);

	int message_len = sizeof(proc_family_command_t) +
	                  sizeof(pid_t) +
	                  sizeof(int) +
	                  sizeof(PidEnvID);
	void* buffer = malloc(message_len);
	assert(buffer != NULL);
	char* ptr = (char*)buffer;

	*(proc_family_command_t*)ptr = PROC_FAMILY_TRACK_FAMILY_VIA_ENVIRONMENT;
	ptr += sizeof(proc_family_command_t);

	*(pid_t*)ptr = pid;
	ptr += sizeof(pid_t);

	*(int*)ptr = sizeof(PidEnvID);
	ptr += sizeof(int);

	pidenvid_copy((PidEnvID*)ptr, &penvid);
	ptr += sizeof(PidEnvID);

	assert(ptr - (char*)buffer == message_len);

	if (!m_client->start_connection(buffer, message_len)) {
		dprintf(D_ALWAYS,
		        "ProcFamilyClient: failed to start connection with ProcD\n");
		free(buffer);
		return false;
	}
	free(buffer);
	proc_family_error_t err;
	if (!m_client->read_data(&err, sizeof(proc_family_error_t))) {
		dprintf(D_ALWAYS,
		        "ProcFamilyClient: failed to read response from ProcD\n");
		return false;
	}
	m_client->end_connection();

	log_exit("track_family_via_environment", err);
	response = (err == PROC_FAMILY_ERROR_SUCCESS);
	return true;
}

bool
ProcFamilyClient::track_family_via_login(pid_t pid,
                                         const char* login,
                                         bool& response)
{
	assert(m_initialized);

	dprintf(D_PROCFAMILY,
	        "About to tell ProcD to track family with root %u "
	            "via login %s\n",
	        pid,
	        login);

	int login_len = strlen(login) + 1;
	int message_len = sizeof(proc_family_command_t) +
	                  sizeof(pid_t) +
	                  sizeof(int) +
	                  login_len;
	void* buffer = malloc(message_len);
	assert(buffer != NULL);
	char* ptr = (char*)buffer;

	*(proc_family_command_t*)ptr = PROC_FAMILY_TRACK_FAMILY_VIA_LOGIN;
	ptr += sizeof(proc_family_command_t);

	*(pid_t*)ptr = pid;
	ptr += sizeof(pid_t);

	*(int*)ptr = login_len;
	ptr += sizeof(int);

	memcpy(ptr, login, login_len);
	ptr += login_len;

	assert(ptr - (char*)buffer == message_len);

	if (!m_client->start_connection(buffer, message_len)) {
		dprintf(D_ALWAYS,
		        "ProcFamilyClient: failed to start connection with ProcD\n");
		free(buffer);
		return false;
	}
	free(buffer);
	proc_family_error_t err;
	if (!m_client->read_data(&err, sizeof(proc_family_error_t))) {
		dprintf(D_ALWAYS,
		        "ProcFamilyClient: failed to read response from ProcD\n");
		return false;
	}
	m_client->end_connection();

	log_exit("track_family_via_login", err);
	response = (err == PROC_FAMILY_ERROR_SUCCESS);
	return true;
}

#if defined(LINUX)
bool
ProcFamilyClient::track_family_via_allocated_supplementary_group(pid_t pid,
                                                       bool& response,
                                                       gid_t& gid)
{
	assert(m_initialized);

	dprintf(D_PROCFAMILY,
	        "About to tell ProcD to track family with root %u "
	            "via GID\n",
	        pid);

	int message_len = sizeof(proc_family_command_t) +
	                  sizeof(pid_t);
	void* buffer = malloc(message_len);
	assert(buffer != NULL);
	char* ptr = (char*)buffer;

	*(proc_family_command_t*)ptr =
		PROC_FAMILY_TRACK_FAMILY_VIA_ALLOCATED_SUPPLEMENTARY_GROUP;
	ptr += sizeof(proc_family_command_t);

	*(pid_t*)ptr = pid;
	ptr += sizeof(pid_t);

	assert(ptr - (char*)buffer == message_len);
	
	if (!m_client->start_connection(buffer, message_len)) {
		dprintf(D_ALWAYS,
		        "ProcFamilyClient: failed to start connection with ProcD\n");
		free(buffer);
		return false;
	}
	free(buffer);
	proc_family_error_t err;
	if (!m_client->read_data(&err, sizeof(proc_family_error_t))) {
		dprintf(D_ALWAYS,
		        "ProcFamilyClient: failed to read response from ProcD\n");
		return false;
	}
	if (err == PROC_FAMILY_ERROR_SUCCESS) {
		if (!m_client->read_data(&gid, sizeof(gid_t))) {
			dprintf(D_ALWAYS,
			        "ProcFamilyClient: failed to read group ID from ProcD\n");
			return false;
		}
		dprintf(D_PROCFAMILY,
		        "tracking family with root PID %u using group ID %u\n",
		        pid,
		        gid);
	}
	m_client->end_connection();

	log_exit("track_family_via_allocated_supplementary_group", err);
	response = (err == PROC_FAMILY_ERROR_SUCCESS);
	return true;
}

bool
ProcFamilyClient::track_family_via_associated_supplementary_group(pid_t pid,
                                                       gid_t gid,
                                                       bool& response)
{
	assert(m_initialized);

	dprintf(D_PROCFAMILY,
	        "About to tell ProcD to track family with root %u "
	            "via GID %u\n",
	        pid, 
			gid);

	int message_len = sizeof(proc_family_command_t) +
	                  sizeof(pid_t) +
					  sizeof(gid_t);
	void* buffer = malloc(message_len);
	assert(buffer != NULL);
	char* ptr = (char*)buffer;

	*(proc_family_command_t*)ptr =
		PROC_FAMILY_TRACK_FAMILY_VIA_ASSOCIATED_SUPPLEMENTARY_GROUP;
	ptr += sizeof(proc_family_command_t);

	*(pid_t*)ptr = pid;
	ptr += sizeof(pid_t);

	*(gid_t*)ptr = gid;
	ptr += sizeof(gid_t);

	assert(ptr - (char*)buffer == message_len);
	
	if (!m_client->start_connection(buffer, message_len)) {
		dprintf(D_ALWAYS,
		        "ProcFamilyClient: failed to start connection with ProcD\n");
		free(buffer);
		return false;
	}
	free(buffer);
	proc_family_error_t err;
	if (!m_client->read_data(&err, sizeof(proc_family_error_t))) {
		dprintf(D_ALWAYS,
		        "ProcFamilyClient: failed to read response from ProcD\n");
		return false;
	}
	m_client->end_connection();

	log_exit("track_family_via_associated_supplementary_group", err);
	response = (err == PROC_FAMILY_ERROR_SUCCESS);
	return true;
}
#endif

#if defined(HAVE_EXT_LIBCGROUP)
bool
ProcFamilyClient::track_family_via_cgroup(pid_t pid,
                                          const char * cgroup,
                                          bool& response)
{
	assert(m_initialized);

	dprintf(D_FULLDEBUG,
		"About to tell ProcD to track family with root %u "
		    "via cgroup %s\n",
		pid,
		cgroup);

	size_t cgroup_len = strlen(cgroup);

	int message_len = sizeof(proc_family_command_t) +
			  sizeof(pid_t) +
			  sizeof(size_t) +
			  sizeof(char)*cgroup_len;
	void* buffer = malloc(message_len);
	assert(buffer != NULL);
	char* ptr = (char*)buffer;

	*(proc_family_command_t*)ptr =
		PROC_FAMILY_TRACK_FAMILY_VIA_CGROUP;
	ptr += sizeof(proc_family_command_t);

	*(pid_t*)ptr = pid;
	ptr += sizeof(pid_t);

	*(size_t*)ptr = cgroup_len;
	ptr += sizeof(size_t);

	memcpy((void *)ptr, (const void *)cgroup, sizeof(char)*cgroup_len);
	ptr += cgroup_len*sizeof(char);

	assert(ptr - (char*)buffer == message_len);

	if (!m_client->start_connection(buffer, message_len)) {
		dprintf(D_ALWAYS,
			"ProcFamilyClient: failed to start connection with ProcD\n");
		free(buffer);
		return false;
        }
	free(buffer);
	proc_family_error_t err;
	if (!m_client->read_data(&err, sizeof(proc_family_error_t))) {
		dprintf(D_ALWAYS,
			"ProcFamilyClient: failed to read response from ProcD\n");
		return false;
	}
	m_client->end_connection();

	log_exit("track_family_via_cgroup", err);
	response = (err == PROC_FAMILY_ERROR_SUCCESS);
	return true;
}
#endif

bool
ProcFamilyClient::use_glexec_for_family(pid_t pid,
                                        const char* proxy,
                                        bool& response)
{
	assert(m_initialized);

	dprintf(D_PROCFAMILY,
	        "About to tell ProcD to use glexec for family with root %u "
	            "with proxy %s\n",
	        pid,
	        proxy);

	int proxy_len = strlen(proxy) + 1;
	int message_len = sizeof(proc_family_command_t) +
	                  sizeof(pid_t) +
	                  sizeof(int) +
	                  proxy_len;
	void* buffer = malloc(message_len);
	assert(buffer != NULL);
	char* ptr = (char*)buffer;

	*(proc_family_command_t*)ptr = PROC_FAMILY_USE_GLEXEC_FOR_FAMILY;
	ptr += sizeof(proc_family_command_t);

	*(pid_t*)ptr = pid;
	ptr += sizeof(pid_t);

	*(int*)ptr = proxy_len;
	ptr += sizeof(int);

	memcpy(ptr, proxy, proxy_len);
	ptr += proxy_len;

	assert(ptr - (char*)buffer == message_len);

	if (!m_client->start_connection(buffer, message_len)) {
		dprintf(D_ALWAYS,
		        "ProcFamilyClient: "
		            "failed to start connection with ProcD\n");
		free(buffer);
		return false;
	}
	free(buffer);
	proc_family_error_t err;
	if (!m_client->read_data(&err, sizeof(proc_family_error_t))) {
		dprintf(D_ALWAYS,
		        "ProcFamilyClient: "
		            "failed to read response from ProcD\n");
		return false;
	}
	m_client->end_connection();

	log_exit("use_glexec_for_family", err);
	response = (err == PROC_FAMILY_ERROR_SUCCESS);
	return true;
}

bool
ProcFamilyClient::get_usage(pid_t pid, ProcFamilyUsage& usage, bool& response)
{
	assert(m_initialized);

	dprintf(D_PROCFAMILY,
	        "About to get usage data from ProcD for family with root %u\n",
	        pid);

	int message_len = sizeof(proc_family_command_t) +
	                  sizeof(pid_t);
	void* buffer = malloc(message_len);
	assert(buffer != NULL);
	char* ptr = (char*)buffer;

	*(proc_family_command_t*)ptr = PROC_FAMILY_GET_USAGE;
	ptr += sizeof(proc_family_command_t);

	*(pid_t*)ptr = pid;
	ptr += sizeof(pid_t);

	assert(ptr - (char*)buffer == message_len);

	if (!m_client->start_connection(buffer, message_len)) {
		dprintf(D_ALWAYS,
		        "ProcFamilyClient: failed to start connection with ProcD\n");
		free(buffer);
		return false;
	}
	free(buffer);
	proc_family_error_t err;
	if (!m_client->read_data(&err, sizeof(proc_family_error_t))) {
		dprintf(D_ALWAYS,
		        "ProcFamilyClient: failed to read response from ProcD\n");
		return false;
	}
	if (err == PROC_FAMILY_ERROR_SUCCESS) {
		if (!m_client->read_data(&usage, sizeof(ProcFamilyUsage))) {
			dprintf(D_ALWAYS,
		        	"ProcFamilyClient: error getting usage from ProcD\n");
			return false;
		}
	}
	m_client->end_connection();

	log_exit("get_usage", err);
	response = (err == PROC_FAMILY_ERROR_SUCCESS);
	return true;
}

bool
ProcFamilyClient::signal_process(pid_t pid, int sig, bool& response)
{
	assert(m_initialized);

	dprintf(D_PROCFAMILY,
	        "About to send process %u signal %d via the ProcD\n",
	        pid,
	        sig);

	int message_len = sizeof(proc_family_command_t) +
	                  sizeof(pid_t) +
	                  sizeof(int);
	void* buffer = malloc(message_len);
	assert(buffer != NULL);
	char* ptr = (char*)buffer;

	*(proc_family_command_t*)ptr = PROC_FAMILY_SIGNAL_PROCESS;
	ptr += sizeof(proc_family_command_t);

	*(pid_t*)ptr = pid;
	ptr += sizeof(pid_t);

	*(int*)ptr = sig;
	ptr += sizeof(int);

	assert(ptr - (char*)buffer == message_len);
	
	if (!m_client->start_connection(buffer, message_len)) {
		dprintf(D_ALWAYS,
		        "ProcFamilyClient: failed to start connection with ProcD\n");
		free(buffer);
		return false;
	}
	free(buffer);
	proc_family_error_t err;
	if (!m_client->read_data(&err, sizeof(proc_family_error_t))) {
		dprintf(D_ALWAYS,
		        "ProcFamilyClient: failed to read response from ProcD\n");
		return false;
	}
	m_client->end_connection();

	log_exit("signal_process", err);
	response = (err == PROC_FAMILY_ERROR_SUCCESS);
	return true;
}

bool
ProcFamilyClient::kill_family(pid_t pid, bool& response)
{
	assert(m_initialized);

	dprintf(D_PROCFAMILY,
	        "About to kill family with root process %u using the ProcD\n",
	        pid);

	return signal_family(pid, PROC_FAMILY_KILL_FAMILY, response);
}

bool
ProcFamilyClient::suspend_family(pid_t pid, bool& response)
{
	assert(m_initialized);

	dprintf(D_PROCFAMILY,
	        "About to suspend family with root process %u using the ProcD\n",
	        pid);

	return signal_family(pid, PROC_FAMILY_SUSPEND_FAMILY, response);
}

bool
ProcFamilyClient::continue_family(pid_t pid, bool& response)
{
	assert(m_initialized);

	dprintf(D_PROCFAMILY,
	        "About to continue family with root process %u using the ProcD\n",
	        pid);

	return signal_family(pid, PROC_FAMILY_CONTINUE_FAMILY, response);
}

bool
ProcFamilyClient::signal_family(pid_t pid,
                                proc_family_command_t command,
                                bool& response)
{
	assert(m_initialized);

	int message_len = sizeof(proc_family_command_t) +
	                  sizeof(pid_t);
	void* buffer = malloc(message_len);
	assert(buffer != NULL);
	char* ptr = (char*)buffer;

	*(proc_family_command_t*)ptr = command;
	ptr += sizeof(proc_family_command_t);

	*(pid_t*)ptr = pid;
	ptr += sizeof(pid_t);

	assert(ptr - (char*)buffer == message_len);
	
	if (!m_client->start_connection(buffer, message_len)) {
		dprintf(D_ALWAYS,
		        "ProcFamilyClient: failed to start connection with ProcD\n");
		free(buffer);
		return false;
	}
	free(buffer);
	proc_family_error_t err;
	if (!m_client->read_data(&err, sizeof(proc_family_error_t))) {
		dprintf(D_ALWAYS,
		        "ProcFamilyClient: failed to read response from ProcD\n");
		return false;
	}
	m_client->end_connection();

	log_exit("signal_family", err);
	response = (err == PROC_FAMILY_ERROR_SUCCESS);
	return true;
}

bool
ProcFamilyClient::unregister_family(pid_t pid, bool& response)
{
	assert(m_initialized);

	dprintf(D_PROCFAMILY,
	        "About to unregister family with root %u from the ProcD\n",
	        pid);

	int message_len = sizeof(proc_family_command_t) +
	                  sizeof(pid_t);
	void* buffer = malloc(message_len);
	assert(buffer != NULL);
	char* ptr = (char*)buffer;

	*(proc_family_command_t*)ptr = PROC_FAMILY_UNREGISTER_FAMILY;
	ptr += sizeof(proc_family_command_t);

	*(pid_t*)ptr = pid;
	ptr += sizeof(pid_t);

	if (!m_client->start_connection(buffer, message_len)) {
		dprintf(D_ALWAYS,
		        "ProcFamilyClient: failed to start connection with ProcD\n");
		free(buffer);
		return false;
	}
	free(buffer);
	proc_family_error_t err;
	if (!m_client->read_data(&err, sizeof(proc_family_error_t))) {
		dprintf(D_ALWAYS,
		        "ProcFamilyClient: failed to read response from ProcD\n");
		return false;
	}
	m_client->end_connection();

	log_exit("unregister_family", err);
	response = (err == PROC_FAMILY_ERROR_SUCCESS);
	return true;
}

bool
ProcFamilyClient::snapshot(bool& response)
{
	assert(m_initialized);

	dprintf(D_PROCFAMILY, "About to tell the ProcD to take a snapshot\n");

	proc_family_command_t command = PROC_FAMILY_TAKE_SNAPSHOT;

	if (!m_client->start_connection(&command, sizeof(proc_family_command_t))) {
		dprintf(D_ALWAYS,
		        "ProcFamilyClient: failed to start connection with ProcD\n");
		return false;
	}
	proc_family_error_t err;
	if (!m_client->read_data(&err, sizeof(proc_family_error_t))) {
		dprintf(D_ALWAYS,
		        "ProcFamilyClient: failed to read response from ProcD\n");
		return false;
	}
	m_client->end_connection();

	log_exit("snapshot", err);
	response = (err == PROC_FAMILY_ERROR_SUCCESS);
	return true;
}

bool
ProcFamilyClient::quit(bool& response)
{
	assert(m_initialized);

	dprintf(D_ALWAYS, "About to tell the ProcD to exit\n");

	proc_family_command_t command = PROC_FAMILY_QUIT;

	if (!m_client->start_connection(&command, sizeof(proc_family_command_t))) {
		dprintf(D_ALWAYS,
		        "ProcFamilyClient: failed to start connection with ProcD\n");
		return false;
	}
	proc_family_error_t err;
	if (!m_client->read_data(&err, sizeof(proc_family_error_t))) {
		dprintf(D_ALWAYS,
		        "ProcFamilyClient: failed to read response from ProcD\n");
		return false;
	}
	m_client->end_connection();

	log_exit("quit", err);
	response = (err == PROC_FAMILY_ERROR_SUCCESS);
	return true;
}

bool
ProcFamilyClient::dump(pid_t pid,
                       bool& response,
                       std::vector<ProcFamilyDump>& vec)
{
	assert(m_initialized);

	dprintf(D_PROCFAMILY, "About to retrive snapshot state from ProcD\n");

	int message_len = sizeof(proc_family_command_t) +
	                  sizeof(pid_t);
	void* buffer = malloc(message_len);
	assert(buffer != NULL);
	char* ptr = (char*)buffer;

	*(proc_family_command_t*)ptr = PROC_FAMILY_DUMP;
	ptr += sizeof(proc_family_command_t);

	*(pid_t*)ptr = pid;
	ptr += sizeof(pid_t);

	assert(ptr - (char*)buffer == message_len);

	if (!m_client->start_connection(buffer, message_len)) {
		dprintf(D_ALWAYS,
		        "ProcFamilyClient: "
			    "failed to start connection with ProcD\n");

		free(buffer);
		return false;
	}
	free(buffer);

	proc_family_error_t err;
	if (!m_client->read_data(&err, sizeof(proc_family_error_t))) {
		dprintf(D_ALWAYS,
		        "ProcFamilyClient: "
			    "failed to read response from ProcD\n");
		return false;
	}
	response = (err == PROC_FAMILY_ERROR_SUCCESS);
	if (!response) {
		m_client->end_connection();
		log_exit("dump", err);
		return true;
	}

	vec.clear();

	int family_count;
	if (!m_client->read_data(&family_count, sizeof(int))) {
		dprintf(D_ALWAYS,
		        "ProcFamilyClient: "
		            "failed to read family count from ProcD\n");
		return false;
	}
	vec.resize(family_count);
	for (int i = 0; i < family_count; ++i) {
		if (!m_client->read_data(&vec[i].parent_root, sizeof(pid_t)) ||
		    !m_client->read_data(&vec[i].root_pid, sizeof(pid_t)) ||
			!m_client->read_data(&vec[i].watcher_pid, sizeof(pid_t)))
		{
			dprintf(D_ALWAYS,
			        "ProcFamilyClient: "
			            "failed reading family dump info from ProcD\n");
			return false;
		}
		int proc_count;
		if (!m_client->read_data(&proc_count, sizeof(int))) {
			dprintf(D_ALWAYS,
			        "ProcFamilyClient: "
			            "failed reading process count from ProcD\n");
			return false;
		}
		vec[i].procs.resize(proc_count);
		for (int j = 0; j < proc_count; ++j) {
			if (!m_client->read_data(&vec[i].procs[j],
			                         sizeof(ProcFamilyProcessDump)))
			{
			dprintf(D_ALWAYS,
			        "ProcFamilyClient: "
			            "failed reading process dump info from ProcD\n");
			return false;
			}
		}
	}
	m_client->end_connection();

	log_exit("dump", err);
	return true;
}
