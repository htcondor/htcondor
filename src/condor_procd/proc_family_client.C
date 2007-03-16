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
#include "condor_config.h"
#include "proc_family_client.h"
#include "proc_family_io.h"
#include "local_client.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "setenv.h"

// helper function for logging the return code from a ProcD
// operation
//
static void
log_exit(char* op_str, proc_family_error_t error_code)
{
	int debug_level = D_PROCFAMILY;
	if (error_code != PROC_FAMILY_ERROR_SUCCESS) {
		debug_level = D_ALWAYS;
	}
	dprintf(debug_level,
	        "Result of \"%s\" operation from ProcD: %s\n",
	        op_str,
	        proc_family_error_strings[error_code]);
}

// static data
//
int ProcFamilyClient::s_num_objects = 0;
int ProcFamilyClient::s_procd_pid = -1;

ProcFamilyClient::ProcFamilyClient()
{
	// update object count
	//
	s_num_objects++;

	// get the address that we'll use to contact the ProcD
	//
	char* addr = param("PROCD_ADDRESS");
	if (addr == NULL) {
		EXCEPT("PROCD_ADDRESS not defined");
	}

	// determine if we need to launch a ProcD
	//
	// if a parent daemon created a ProcD that we can use, it will
	// have handed us an environment variable indicating its address.
	// if this address matches the ProcD address that we are configured
	// to use, we don't need to create a ProcD. if the env var isn't
	// there or they don't match, we need to create a ProcD and also set
	// the environment variable so any DC children can share this ProcD
	//
	// we'll only launch one ProcD, so first check if s_procd_pid is set
	//
	if (s_procd_pid == -1) {
		const char* addr_from_env = GetEnv("CONDOR_PROCD_ADDRESS");
		if ((addr_from_env == NULL) || strcmp(addr_from_env, addr)) {
			start_procd(addr);
			SetEnv("CONDOR_PROCD_ADDRESS", addr);
		}
	}

	// create the LocalClient object for communicating with the ProcD
	//
	m_client = new LocalClient(addr);
	ASSERT(m_client != NULL);
}

ProcFamilyClient::~ProcFamilyClient()
{
	// update object count
	//
	s_num_objects--;

	// if we started a ProcD and there are no other objects of this class,
	// now's the time to shut it down
	//
	if ((s_procd_pid != -1) && (s_num_objects == 0)) {
		stop_procd();
	}

	// delete our LocalClient object
	//
	delete m_client;
}

bool
ProcFamilyClient::register_subfamily(pid_t root_pid,
                                     pid_t watcher_pid,
                                     int max_snapshot_interval,
                                     PidEnvID* penvid,
                                     const char* login)
{
	dprintf(D_PROCFAMILY,
	        "About to register family for PID %u with the ProcD\n",
	        root_pid);

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
	m_client->start_connection(buffer, message_len);
	free(buffer);

	// receive the boolean response from the server
	//
	proc_family_error_t err;
	m_client->read_data(&err, sizeof(proc_family_error_t));

	// done with this command
	//
	m_client->end_connection();

	log_exit("register_subfamily", err);

	return (err == PROC_FAMILY_ERROR_SUCCESS);
}

bool
ProcFamilyClient::get_usage(pid_t pid, ProcFamilyUsage& usage)
{
	dprintf(D_PROCFAMILY,
	        "About to get usage data from ProcD for family with root %u\n",
	        pid);

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

	m_client->start_connection(buffer, message_len);

	proc_family_error_t err;
	m_client->read_data(&err, sizeof(proc_family_error_t));
	if (err == PROC_FAMILY_ERROR_SUCCESS) {
		m_client->read_data(&usage, sizeof(ProcFamilyUsage));
	}
	m_client->end_connection();

	log_exit("get_usage", err);

	return (err == PROC_FAMILY_ERROR_SUCCESS);
}

bool
ProcFamilyClient::signal_process(pid_t pid, int sig)
{
	dprintf(D_PROCFAMILY,
	        "About to send process %u signal %d via the ProcD\n",
	        pid,
	        sig);

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
	
	m_client->start_connection(buffer, message_len);

	proc_family_error_t err;
	m_client->read_data(&err, sizeof(proc_family_error_t));
	m_client->end_connection();

	log_exit("signal_process", err);

	return (err == PROC_FAMILY_ERROR_SUCCESS);
}

bool
ProcFamilyClient::kill_family(pid_t pid)
{
	dprintf(D_PROCFAMILY,
	        "About to kill family with root process %u using the ProcD\n",
	        pid);

	return signal_family(pid, PROC_FAMILY_KILL_FAMILY);
}

bool
ProcFamilyClient::suspend_family(pid_t pid)
{
	dprintf(D_PROCFAMILY,
	        "About to suspend family with root process %u using the ProcD\n",
	        pid);

	return signal_family(pid, PROC_FAMILY_SUSPEND_FAMILY);
}

bool
ProcFamilyClient::continue_family(pid_t pid)
{
	dprintf(D_PROCFAMILY,
	        "About to continue family with root process %u using the ProcD\n",
	        pid);

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
	
	m_client->start_connection(buffer, message_len);

	proc_family_error_t err;
	m_client->read_data(&err, sizeof(proc_family_error_t));

	m_client->end_connection();

	log_exit("signal_family", err);

	return (err == PROC_FAMILY_ERROR_SUCCESS);
}

bool
ProcFamilyClient::unregister_family(pid_t pid)
{
	dprintf(D_PROCFAMILY,
	        "About to unregister family with root %u from the ProcD\n",
	        pid);

	int message_len = sizeof(proc_family_command_t) +
	                  sizeof(pid_t);
	void* buffer = malloc(message_len);
	ASSERT(buffer != NULL);
	char* ptr = (char*)buffer;

	*(proc_family_command_t*)ptr = PROC_FAMILY_UNREGISTER_FAMILY;
	ptr += sizeof(proc_family_command_t);

	*(pid_t*)ptr = pid;
	ptr += sizeof(pid_t);

	m_client->start_connection(buffer, message_len);

	proc_family_error_t err;
	m_client->read_data(&err, sizeof(proc_family_error_t));

	m_client->end_connection();

	log_exit("unregister_family", err);

	return (err == PROC_FAMILY_ERROR_SUCCESS);
}

bool
ProcFamilyClient::snapshot()
{
	dprintf(D_PROCFAMILY, "About to tell the ProcD to take a snapshot\n");

	proc_family_command_t command = PROC_FAMILY_TAKE_SNAPSHOT;

	m_client->start_connection(&command, sizeof(proc_family_command_t));

	proc_family_error_t err;
	m_client->read_data(&err, sizeof(proc_family_error_t));

	m_client->end_connection();

	log_exit("snapshot", err);

	return (err == PROC_FAMILY_ERROR_SUCCESS);
}

#if defined(PROCD_DEBUG)

LocalClient*
ProcFamilyClient::dump(pid_t pid)
{
	dprintf(D_PROCFAMILY, "About to retrive snapshot state from ProcD\n");

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

	m_client->start_connection(buffer, message_len);

	proc_family_error_t err;
	m_client->read_data(&err, sizeof(proc_family_error_t));
	if (err != PROC_FAMILY_ERROR_SUCCESS) {
		m_client->end_connection();
		return NULL;
	}

	return m_client;
}

#endif

void
ProcFamilyClient::start_procd(char* address)
{
	// we'll only spawn one ProcD
	//
	ASSERT(s_procd_pid == -1);

	// the ProcD's "root pid" (a.k.a. the daddy pid) is going to be us
	//
	pid_t root_pid = daemonCore->getpid();

	// now, we build up an ArgList for the procd
	//
	ArgList args;
	args.SetArgV1SyntaxToCurrentPlatform();

	// path to the executable
	//
	char* path = param("PROCD");
	if (path == NULL) {
		EXCEPT("error: PROCD not defined in configuration");
	}
	MyString err;
	if (!args.AppendArgsV1RawOrV2Quoted(path, &err)) {
		EXCEPT("error constructing procd command line: %s", err.Value());
	}
	free(path);

	// the procd's address
	//
	args.AppendArg("-A");
	args.AppendArg(address);

	// PID and birthday of the procd's "root process" (which is going to be us)
	//
	MyString root_pid_arg;
	root_pid_arg.sprintf("%u", root_pid);
	int ignored;
	procInfo* pi = NULL;
	if (ProcAPI::getProcInfo(root_pid, pi, ignored) != PROCAPI_SUCCESS) {
		EXCEPT("unable to get own process information");
	}
	MyString root_birthday_arg;
	root_birthday_arg.sprintf(PROCAPI_BIRTHDAY_FORMAT, pi->birthday);
	free(pi);
	args.AppendArg("-P");
	args.AppendArg(root_pid_arg.Value());
	args.AppendArg(root_birthday_arg.Value());

	// the (optional) procd log file
	//
	char* log_file = param("PROCD_LOG");
	if (log_file != NULL) {
		args.AppendArg("-L");
		args.AppendArg(log_file);
		free(log_file);
	}

	// the (optional) maximum snapshot interval
	// (the procd will default to every minute)
	//
	char* max_snapshot_interval = param("PROCD_MAX_SNAPSHOT_INTERVAL");
	if (max_snapshot_interval != NULL) {
		args.AppendArg("-S");
		args.AppendArg(max_snapshot_interval);
		free(max_snapshot_interval);
	}

	// (optional) make the procd sleep on startup so a
	// debugger can attach
	//
	bool debug = param_boolean("PROCD_DEBUG", false);
	if (debug) {
		args.AppendArg("-D");
	}

#if !defined(WIN32)
	// On UNIX, we need to tell the procd to allow connections from the
	// condor user
	//
	args.AppendArg("-C");
	args.AppendArg(get_condor_uid());
#endif

#if defined(WIN32)
	// on Windows, we need to tell the procd what program to use to send
	// softkills
	//
	char* softkill_path = param("WINDOWS_SOFTKILL");
	if (softkill_path == NULL) {
		dprintf(D_ALWAYS,
		        "WINDOWS_SOFTKILL undefined; ProcD won't be able to send WM_CLOSE to jobs");
	}
	else {
		args.AppendArg("-K");
		args.AppendArg(softkill_path);
		free(softkill_path);
	}
#endif

	// done constructing the argument list; now register a reaper for
	// notification when the procd exits
	//
	int reaper_id = daemonCore->Register_Reaper("condor_procd reaper",
	                                            &ProcFamilyClient::procd_reaper,
	                                            "condor_procd reaper");
	if (reaper_id == FALSE) {
		EXCEPT("unable to register a reaper for the procd");
	}

	// we start the procd with a pipe coming back to us on its stderr.
	// the procd will close this pipe after it startd listening for
	// commands.
	//
	int pipe_ends[2];
	if (daemonCore->Create_Pipe(pipe_ends) == FALSE) {
		EXCEPT("error creating pipe for the procd");
	}
	int std_io[3];
	std_io[0] = -1;
	std_io[1] = -1;
	std_io[2] = pipe_ends[1];

	// use Create_Process to start the procd
	//
	s_procd_pid = daemonCore->Create_Process(args.GetArg(0),
	                                         args,
	                                         PRIV_ROOT,
	                                         reaper_id,
	                                         FALSE,
	                                         NULL,
	                                         NULL,
	                                         NULL,
	                                         NULL,
	                                         std_io);
	if (s_procd_pid == FALSE) {
		EXCEPT("unable to spawn the procd");
	}

	// now close the pipe end we handed to the child and then block on the
	// pipe until it closes (which tells us the procd is listening for
	// commands)
	//
	if (daemonCore->Close_Pipe(pipe_ends[1]) == FALSE) {
		EXCEPT("error closing procd's pipe end");
	}
	const int MAX_PROCD_ERR_LEN = 80;
	char err_msg[MAX_PROCD_ERR_LEN + 1];
	int ret = daemonCore->Read_Pipe(pipe_ends[0], err_msg, MAX_PROCD_ERR_LEN);
	if (ret != 0) {
		if (ret == -1) {
			EXCEPT("error reading pipe from procd");
		}
		err_msg[ret] = '\0';
		EXCEPT("error received from procd: %s", err_msg);
	}
	if (daemonCore->Close_Pipe(pipe_ends[0]) == FALSE) {
		EXCEPT("error closing pipe to procd");
	}

	// OK, the ProcD's up and running!
}

void
ProcFamilyClient::stop_procd()
{
	dprintf(D_PROCFAMILY, "About to tell the ProcD to exit\n");

	proc_family_command_t command = PROC_FAMILY_QUIT;

	m_client->start_connection(&command, sizeof(proc_family_command_t));

	proc_family_error_t err;
	m_client->read_data(&err, sizeof(proc_family_error_t));

	m_client->end_connection();

	log_exit("quit", err);

	ASSERT(err == PROC_FAMILY_ERROR_SUCCESS);

	// set s_procd_pid back to -1 so the reaper knows its OK to see
	// the ProcD exit
	//
	s_procd_pid = -1;
}

int
ProcFamilyClient::procd_reaper(Service*, int pid, int status)
{
	if (s_procd_pid == -1) {
		dprintf(D_ALWAYS,
		        "procd (pid = %d) exited with status %d\n",
		        pid,
		        status);
	}
	else {
		ASSERT(pid == s_procd_pid);
		EXCEPT("procd (pid = %d) exited unexpectedly with status %d",
		       pid,
		       status);
	}

	return 0;
}
