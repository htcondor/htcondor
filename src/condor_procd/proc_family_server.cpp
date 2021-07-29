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
#include "proc_family_server.h"
#include "proc_family_monitor.h"
#include "local_server.h"

ProcFamilyServer::ProcFamilyServer(ProcFamilyMonitor& monitor,
                                   const char* addr) :
	m_monitor(monitor)
{
	m_server = new LocalServer;
	ASSERT(m_server != NULL);
	if (!m_server->initialize(addr)) {
		EXCEPT("ProcFamilyServer: could not initialize LocalServer");
	}
}

ProcFamilyServer::~ProcFamilyServer()
{
	delete m_server;
}

void
ProcFamilyServer::set_client_principal(char* p)
{
	if (!m_server->set_client_principal(p)) {
		EXCEPT("ProcFamilyServer: error setting client principal");
	}
}

void
ProcFamilyServer::read_from_client(void* buf, int len)
{
	// TODO
	//
	// on UNIX, if we hit an error reading from a client we're
	// screwed since all clients share a single named pipe for
	// sending us commands and we expect that our clients write
	// valid messages to us atomically
	//
	// on Windows, we get a dedicated connection and could do
	// better - for now, however, we give up in any case
	//
	if (!m_server->read_data(buf, len)) {
		EXCEPT("ProcFamilyServer: error reading from client");
	}
}

void
ProcFamilyServer::write_to_client(void* buf, int len)
{
	// write errors shouldn't kill us, so we'll just log them
	// and plug on
	//
	if (!m_server->write_data(buf, len)) {
		dprintf(D_ALWAYS,
		        "ProcFamilyServer: error writing to client\n");
	}
}

void
ProcFamilyServer::register_subfamily()
{
	pid_t root_pid;
	read_from_client(&root_pid, sizeof(pid_t));

	pid_t watcher_pid;
	read_from_client(&watcher_pid, sizeof(pid_t));

	int max_snapshot_interval;
	read_from_client(&max_snapshot_interval, sizeof(int));

	proc_family_error_t err = m_monitor.register_subfamily(root_pid,
	                                                       watcher_pid,
	                                                       max_snapshot_interval);

	write_to_client(&err, sizeof(proc_family_error_t));
}

void
ProcFamilyServer::track_family_via_environment()
{
	pid_t pid;
	read_from_client(&pid, sizeof(pid_t));

	int penvid_length;
	read_from_client(&penvid_length, sizeof(int));

	proc_family_error_t err;
	if (penvid_length != sizeof(PidEnvID)) {
		err = PROC_FAMILY_ERROR_BAD_ENVIRONMENT_INFO;
	}
	else {
		PidEnvID* penvid = new PidEnvID;
		ASSERT(penvid != NULL);
		read_from_client(penvid, sizeof(PidEnvID));
		err = m_monitor.track_family_via_environment(pid, penvid);
	}

	write_to_client(&err, sizeof(proc_family_error_t));
}

void
ProcFamilyServer::track_family_via_login()
{
	pid_t pid;
	read_from_client(&pid, sizeof(pid_t));

	int login_length;
	read_from_client(&login_length, sizeof(int));

	proc_family_error_t err;
	if (login_length <= 0) {
		err = PROC_FAMILY_ERROR_BAD_LOGIN_INFO;
	}
	else {
		char* login = new char[login_length];
		ASSERT(login != NULL);
		read_from_client(login, login_length);
		err = m_monitor.track_family_via_login(pid, login);
	}

	write_to_client(&err, sizeof(proc_family_error_t));
}

#if defined(LINUX)
// This call is mutually exclusive with 
// track_family_via_associated_supplementary_group(). The procd supports only
// on or the other at runtime.
void
ProcFamilyServer::track_family_via_allocated_supplementary_group()
{
	pid_t pid;
	read_from_client(&pid, sizeof(pid_t));

	gid_t gid;

	// When the procd is started (not using -P) such that it knows the parent
	// of the procd is the root pid of the tree and the watcher process of the
	// procd, the it is in self-allocation mode for gid tracking of process
	// families.  In that context, this call fills in the gid with a
	// self-allocated gid for this process family.
	proc_family_error_t err =
		m_monitor.track_family_via_supplementary_group(pid, gid);

	write_to_client(&err, sizeof(proc_family_error_t));
	if (err == PROC_FAMILY_ERROR_SUCCESS) {
		write_to_client(&gid, sizeof(gid_t));
	}
}

// This call is mutually exclusive with 
// track_family_via_allocated_supplementary_group(). The procd supports only
// one or the other at runtime.
void
ProcFamilyServer::track_family_via_associated_supplementary_group()
{
	pid_t pid;
	read_from_client(&pid, sizeof(pid_t));

	gid_t gid;

	// Associate this specific gid with that family.
	read_from_client(&gid, sizeof(gid_t));

	// When the procd is started (using -P) such that it is managing a root
	// tree underneath the procd and there is no watcher process for the procd,
	// it will only associate gids to a process family when told the specific
	// gid by the procd_ctl tool. In this context, this call accepts the
	// gid given and assigns it to track the family.
	proc_family_error_t err =
		m_monitor.track_family_via_supplementary_group(pid, gid);
	
	write_to_client(&err, sizeof(proc_family_error_t));
}
#endif

#if defined(HAVE_EXT_LIBCGROUP)
void
ProcFamilyServer::track_family_via_cgroup()
{
	pid_t pid;
	read_from_client(&pid, sizeof(pid_t));

	size_t cgroup_len;
	read_from_client(&cgroup_len, sizeof(size_t));
	char * cgroup = (char *)malloc(sizeof(char)*cgroup_len+1);
	read_from_client(cgroup, sizeof(char)*cgroup_len);
	// String sent over pipe is NOT null-terminated.
	cgroup[cgroup_len] = '\0';

	proc_family_error_t err = m_monitor.track_family_via_cgroup(pid, cgroup);
	write_to_client(&err, sizeof(proc_family_error_t));

	free(cgroup);
}
#endif

#if !defined(WIN32)
void
ProcFamilyServer::use_glexec_for_family()
{
	pid_t pid;
	read_from_client(&pid, sizeof(pid_t));

	int proxy_len;
	read_from_client(&proxy_len, sizeof(int));

	proc_family_error_t err;
	// prevent the client from causing the server to malloc unbounded memory
	if ((proxy_len <= 0) || (proxy_len > (1024 * 1024)))  {
		err = PROC_FAMILY_ERROR_BAD_GLEXEC_INFO;
	}
	else {
		char* proxy = new char[proxy_len];
		ASSERT(proxy != NULL);
		read_from_client(proxy, proxy_len);
		proxy[proxy_len - 1] = '\0';
		err = m_monitor.use_glexec_for_family(pid, proxy);
		delete[] proxy;
	}

	write_to_client(&err, sizeof(proc_family_error_t));
}
#endif

void
ProcFamilyServer::get_usage()
{
	pid_t pid;
	read_from_client(&pid, sizeof(pid_t));

	ProcFamilyUsage usage;
	dprintf(D_ALWAYS, "PROC_FAMILY_GET_USAGE for pid %d\n", pid);

	proc_family_error_t err = m_monitor.get_family_usage(pid, &usage);
	
	write_to_client(&err, sizeof(proc_family_error_t));
	if (err == PROC_FAMILY_ERROR_SUCCESS) {
		write_to_client(&usage, sizeof(ProcFamilyUsage));
	}
}

void
ProcFamilyServer::signal_process()
{
	pid_t pid;
	read_from_client(&pid, sizeof(pid_t));

	int sig;
	read_from_client(&sig, sizeof(int));

	proc_family_error_t err = m_monitor.signal_process(pid, sig);

	write_to_client(&err, sizeof(proc_family_error_t));
}

void
ProcFamilyServer::suspend_family()
{
	pid_t pid;
	read_from_client(&pid, sizeof(pid_t));

	proc_family_error_t err = m_monitor.signal_family(pid, SIGSTOP);

	write_to_client(&err, sizeof(proc_family_error_t));
}

void
ProcFamilyServer::continue_family()
{
	pid_t pid;
	read_from_client(&pid, sizeof(pid_t));

	proc_family_error_t err = m_monitor.signal_family(pid, SIGCONT);

	write_to_client(&err, sizeof(proc_family_error_t));
}

void ProcFamilyServer::kill_family()
{
	pid_t pid;
	read_from_client(&pid, sizeof(pid_t));

	proc_family_error_t err = m_monitor.signal_family(pid, SIGKILL);

	write_to_client(&err, sizeof(proc_family_error_t));
}

void
ProcFamilyServer::unregister_family()
{
	pid_t pid;
	read_from_client(&pid, sizeof(pid_t));

	proc_family_error_t err = m_monitor.unregister_subfamily(pid);

	write_to_client(&err, sizeof(proc_family_error_t));
}

void
ProcFamilyServer::dump()
{
	pid_t pid;
	read_from_client(&pid, sizeof(pid_t));

	std::vector<ProcFamilyDump> vec;
	proc_family_error_t err = m_monitor.dump(pid, vec);

	write_to_client(&err, sizeof(proc_family_error_t));
	if (err != PROC_FAMILY_ERROR_SUCCESS) {
		return;
	}

	int family_count = vec.size();
	write_to_client(&family_count, sizeof(int));
	for (int i = 0; i < family_count; ++i) {
		write_to_client(&vec[i].parent_root, sizeof(pid_t));
		write_to_client(&vec[i].root_pid, sizeof(pid_t));
		write_to_client(&vec[i].watcher_pid, sizeof(pid_t));
		int proc_count = vec[i].procs.size();
		write_to_client(&proc_count, sizeof(int));
		for (int j = 0; j < proc_count; j++) {
			write_to_client(&vec[i].procs[j], sizeof(ProcFamilyProcessDump));
		}
	}
}

void
ProcFamilyServer::snapshot()
{
	m_monitor.snapshot();

	proc_family_error_t err = PROC_FAMILY_ERROR_SUCCESS;

	write_to_client(&err, sizeof(proc_family_error_t));
}

void
ProcFamilyServer::quit()
{
	proc_family_error_t err = PROC_FAMILY_ERROR_SUCCESS;
	write_to_client(&err, sizeof(proc_family_error_t));
}

void
ProcFamilyServer::wait_loop()
{
	int snapshot_interval;
	int snapshot_countdown = INT_MAX;

	bool quit_received = false;
	while(!quit_received) {

		snapshot_interval = m_monitor.get_snapshot_interval();
		if (snapshot_interval == -1) {
			snapshot_countdown = -1;
		}
		else if (snapshot_countdown > snapshot_interval) {
			snapshot_countdown = snapshot_interval;
		}

		// see if we need to run our timer handler
		//
		if (snapshot_countdown == 0) {

			// touch our named pipe(s), to ensure the don't get
			// garbage collected by tmpwatch or whatever
			//
			m_server->touch();

			// Now after we touch it, let's confirm that everything is still
			// consistent. This means that the touched named pipe must be the
			// named pipe that we originally opened.
			//
			// NOTE: This is not a security measure of any kind. This is simply
			// here to allow the procd to die in case some high level
			// controller software for it cleans up the pipes but fails to
			// clean up the procd process itself. This exact situation happened
			// in the OSG use case for the proc.
			if (m_server->consistent() == false) {
				EXCEPT("ProcFamilyServer: Namedpipe reader isn't consistent");
			}

			// take our periodic snapshot
			//
			m_monitor.snapshot();

			snapshot_countdown = snapshot_interval;
		}
	
		time_t time_before = time(NULL);
		bool command_ready;
		bool ok = m_server->accept_connection(snapshot_countdown,
		                                      command_ready);
		if (!ok) {
			EXCEPT("ProcFamilyServer: failed trying to accept client");
		}
		if (!command_ready) {
			// timeout; make sure we execute the timer handler
			// next time around by explicitly setting the
			// countdown to zero
			//
			snapshot_countdown = 0;
			continue;
		}

		// adjust the countdown according to how long it took
		// to receive the command. we need to make sure this
		// doesn't go below 0 since -1 means "INFINITE"
		//
		snapshot_countdown -= (time(NULL) - time_before);
		if (snapshot_countdown < 0) {
			snapshot_countdown = 0;
		}

		// read the command int from the client
		//
		int command;
		read_from_client(&command, sizeof(int));

		// execute the received command
		//
		switch (command) {
		
			case PROC_FAMILY_REGISTER_SUBFAMILY:
				dprintf(D_ALWAYS, "PROC_FAMILY_REGISTER_SUBFAMILY\n");
				register_subfamily();
				break;

			case PROC_FAMILY_TRACK_FAMILY_VIA_ENVIRONMENT:
				dprintf(D_ALWAYS,
				        "PROC_FAMILY_TRACK_FAMILY_VIA_ENVIRONMENT\n");
				track_family_via_environment();
				break;

			case PROC_FAMILY_TRACK_FAMILY_VIA_LOGIN:
				dprintf(D_ALWAYS,
				        "PROC_FAMILY_TRACK_FAMILY_VIA_LOGIN\n");
				track_family_via_login();
				break;

#if defined(LINUX)
			case PROC_FAMILY_TRACK_FAMILY_VIA_ALLOCATED_SUPPLEMENTARY_GROUP:
				dprintf(D_ALWAYS,
				        "PROC_FAMILY_TRACK_FAMILY_VIA_ALLOCATED_SUPPLEMENTARY_GROUP\n");
				track_family_via_allocated_supplementary_group();
				break;

			case PROC_FAMILY_TRACK_FAMILY_VIA_ASSOCIATED_SUPPLEMENTARY_GROUP:
				dprintf(D_ALWAYS,
				        "PROC_FAMILY_TRACK_FAMILY_VIA_ASSOCIATED_SUPPLEMENTARY_GROUP\n");
				track_family_via_associated_supplementary_group();
				break;
#endif

#if defined(HAVE_EXT_LIBCGROUP)
			case PROC_FAMILY_TRACK_FAMILY_VIA_CGROUP:
				dprintf(D_ALWAYS,
					"PROC_FAMILY_TRACK_FAMILY_VIA_CGROUP\n");
				track_family_via_cgroup();
				break;
#endif

#if !defined(WIN32)
			case PROC_FAMILY_USE_GLEXEC_FOR_FAMILY:
				dprintf(D_ALWAYS,
				        "PROC_FAMILY_USE_GLEXEC_FOR_FAMILY\n");
				use_glexec_for_family();
				break;
#endif

			case PROC_FAMILY_SIGNAL_PROCESS:
				dprintf(D_ALWAYS, "PROC_FAMILY_SIGNAL_PROCESS\n");
				signal_process();
				break;

			case PROC_FAMILY_SUSPEND_FAMILY:
				dprintf(D_ALWAYS, "PROC_FAMILY_SUSPEND_FAMILY\n");
				suspend_family();
				break;

			case PROC_FAMILY_CONTINUE_FAMILY:
				dprintf(D_ALWAYS, "PROC_FAMILY_CONTINUE_FAMILY\n");
				continue_family();
				break;

			case PROC_FAMILY_KILL_FAMILY:
				dprintf(D_ALWAYS, "PROC_FAMILY_KILL_FAMILY\n");
				kill_family();
				break;
				
			case PROC_FAMILY_GET_USAGE:
				// print this out on the same line with the pid of interest
				//dprintf(D_ALWAYS, "PROC_FAMILY_GET_USAGE\n");
				get_usage();
				break;

			case PROC_FAMILY_UNREGISTER_FAMILY:
				dprintf(D_ALWAYS, "PROC_FAMILY_UNREGISTER_FAMILY\n");
				unregister_family();
				break;

			case PROC_FAMILY_TAKE_SNAPSHOT:
				dprintf(D_ALWAYS, "PROC_FAMILY_TAKESNAPSHOT\n");
				snapshot();
				break;

			case PROC_FAMILY_DUMP:
				dprintf(D_ALWAYS, "PROC_FAMILY_DUMP\n");
				dump();
				break;

			case PROC_FAMILY_QUIT:
				dprintf(D_ALWAYS, "PROC_FAMILY_QUIT\n");
				quit();
				quit_received = true;
				break;

			default:
				EXCEPT("unknown command %d received", command);
		}

		m_server->close_connection();
	}
}
