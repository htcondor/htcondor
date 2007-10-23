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
		EXCEPT("ProcFamilyServer: error setting client principal\n");
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
		EXCEPT("ProcFamilyServer: error reading from client\n");
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
void
ProcFamilyServer::track_family_via_supplementary_group()
{
	pid_t pid;
	read_from_client(&pid, sizeof(pid_t));

	gid_t gid;
	proc_family_error_t err =
		m_monitor.track_family_via_supplementary_group(pid, gid);

	write_to_client(&err, sizeof(proc_family_error_t));
	if (err == PROC_FAMILY_ERROR_SUCCESS) {
		write_to_client(&gid, sizeof(gid_t));
	}
}
#endif

void
ProcFamilyServer::get_usage()
{
	pid_t pid;
	read_from_client(&pid, sizeof(pid_t));

	ProcFamilyUsage usage;
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

#if defined(PROCD_DEBUG)
void
ProcFamilyServer::dump()
{
	pid_t pid;
	read_from_client(&pid, sizeof(pid_t));

	m_monitor.output(*m_server, pid);
}
#endif

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
			m_monitor.snapshot();
			snapshot_countdown = snapshot_interval;
		}
	
		time_t time_before = time(NULL);
		bool command_ready;
		bool ok = m_server->accept_connection(snapshot_countdown,
		                                      command_ready);
		if (!ok) {
			EXCEPT("ProcFamilyServer: failed trying to accept client\n");
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
			case PROC_FAMILY_TRACK_FAMILY_VIA_SUPPLEMENTARY_GROUP:
				dprintf(D_ALWAYS,
				        "PROC_FAMILY_TRACK_FAMILY_VIA_SUPPLEMENTARY_GROUP\n");
				track_family_via_supplementary_group();
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
				dprintf(D_ALWAYS, "PROC_FAMILY_GET_USAGE\n");
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

#if defined(PROCD_DEBUG)
			case PROC_FAMILY_DUMP:
				dprintf(D_ALWAYS, "PROC_FAMILY_DUMP\n");
				dump();
				break;
#endif

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
