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

ProcFamilyServer::ProcFamilyServer(ProcFamilyMonitor& monitor, const char* addr) :
	m_monitor(monitor),
	m_server(addr)
{
}

void
ProcFamilyServer::register_subfamily()
{
	// the payload for this command has the following format:
	//   - the "root pid" of this new subfamily
	//   - the "watcher pid", who'll be able to control the
	//     subfamily
	//   - requested maximum snapshot interval
	//   - size of penvid (0 if not present)
	//   - penvid (if present)
	//   - size of login (0 if not present)
	//   - login (if present)
	//

	pid_t root_pid;
	m_server.read_data(&root_pid, sizeof(pid_t));

	pid_t watcher_pid;
	m_server.read_data(&watcher_pid, sizeof(pid_t));

	int max_snapshot_interval;
	m_server.read_data(&max_snapshot_interval, sizeof(int));

	int penvid_len;
	m_server.read_data(&penvid_len, sizeof(int));

	PidEnvID* penvid = NULL;
	if (penvid_len) {
		ASSERT(penvid_len == sizeof(PidEnvID));
		penvid = new PidEnvID;
		ASSERT(penvid != NULL);
		m_server.read_data(penvid, penvid_len);
	}

	int login_len;
	m_server.read_data(&login_len, sizeof(int));

	char* login = NULL;
	if (login_len) {
		login = new char[login_len];
		ASSERT(login != NULL);
		m_server.read_data(login, login_len);
	}

	bool ok = m_monitor.register_subfamily(root_pid,
	                                       watcher_pid,
	                                       max_snapshot_interval,
	                                       penvid,
	                                       login);

	m_server.write_data(&ok, sizeof(bool));
}

void
ProcFamilyServer::get_usage()
{
	pid_t pid;
	m_server.read_data(&pid, sizeof(pid_t));

	ProcFamilyUsage usage;
	bool ok = m_monitor.get_family_usage(pid, &usage);
	
	m_server.write_data(&ok, sizeof(bool));
	if (ok) {
		m_server.write_data(&usage, sizeof(ProcFamilyUsage));
	}
}

void
ProcFamilyServer::signal_process()
{
	pid_t pid;
	m_server.read_data(&pid, sizeof(pid_t));

	int sig;
	m_server.read_data(&sig, sizeof(int));

	bool ok = m_monitor.signal_process(pid, sig);

	m_server.write_data(&ok, sizeof(bool));
}

void
ProcFamilyServer::suspend_family()
{
	pid_t pid;
	m_server.read_data(&pid, sizeof(pid_t));

	bool ok = m_monitor.signal_family(pid, SIGSTOP);

	m_server.write_data(&ok, sizeof(bool));
}

void
ProcFamilyServer::continue_family()
{
	pid_t pid;
	m_server.read_data(&pid, sizeof(pid_t));

	bool ok = m_monitor.signal_family(pid, SIGCONT);

	m_server.write_data(&ok, sizeof(bool));
}

void ProcFamilyServer::kill_family()
{
	pid_t pid;
	m_server.read_data(&pid, sizeof(pid_t));

	bool ok = m_monitor.signal_family(pid, SIGKILL);

	m_server.write_data(&ok, sizeof(bool));
}

void
ProcFamilyServer::unregister_family()
{
	pid_t pid;
	m_server.read_data(&pid, sizeof(pid_t));

	bool ok = m_monitor.unregister_subfamily(pid);

	m_server.write_data(&ok, sizeof(bool));
}

#if defined(PROCD_DEBUG)
void
ProcFamilyServer::dump()
{
	pid_t pid;
	m_server.read_data(&pid, sizeof(pid_t));

	m_monitor.output(m_server, pid);
}
#endif

void
ProcFamilyServer::snapshot()
{
	m_monitor.snapshot();

	bool ok = true;

	m_server.write_data(&ok, sizeof(bool));
}

void
ProcFamilyServer::quit()
{
	bool ok = true;
	m_server.write_data(&ok, sizeof(bool));
}

void
ProcFamilyServer::wait_loop()
{
	int snapshot_interval;
	int snapshot_countdown = INT_MAX;

	bool quit_received = false;
	while(!quit_received) {

		snapshot_interval = m_monitor.get_snapshot_interval();
		dprintf(D_ALWAYS, "current snapshot interval: %d\n", snapshot_interval);
		if (snapshot_interval == -1) {
			snapshot_countdown = -1;
		}
		else if (snapshot_countdown > snapshot_interval) {
			snapshot_countdown = snapshot_interval;
		}

		// see if we need to run our timer handler
		//
		dprintf(D_ALWAYS, "snapshot countdown is %d\n", snapshot_countdown);
		if (snapshot_countdown == 0) {
			m_monitor.snapshot();
			snapshot_countdown = snapshot_interval;
		}
	
		time_t time_before = time(NULL);
		bool command_ready = m_server.accept_connection(snapshot_countdown);

		if (!command_ready) {
			// timeout; make sure we execute the timer handler
			// next time around by explicitly setting the
			// countdown to zero
			//
			dprintf(D_ALWAYS, "timeout!\n");
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
		m_server.read_data(&command, sizeof(int));

		// execute the received command
		//
		switch (command) {
		
			case PROC_FAMILY_REGISTER_SUBFAMILY:
				dprintf(D_ALWAYS, "PROC_FAMILY_REGISTER_SUBFAMILY\n");
				register_subfamily();
				break;

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

		m_server.close_connection();
	}
}
