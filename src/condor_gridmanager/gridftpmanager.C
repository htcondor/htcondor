/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
#include "gridftpmanager.h"

GridftpManager::GridftpManager() {
	m_reaperid =
		daemonCore->RegisterReaper (
									"GridFTP Reaper",
									(ReaperHandlercpp)GridftpManager::Reaper,
									"GridftpManager::Reaper",
									this);
}

GridftpManager::~GridftpManager() {
		// TODO should unregister reaper here
}

void
GridftpManager::Reaper(Service*,int pid,int status) {
	GridftpServer * gridftp = m_gridftp;	// for now there's only one
	if (gridftp.m_pid == pid) {

		gridftp.m_pid = -1;

		gridftp->SetState (GridftpServer::DEAD);
		if ((gridftp->m_restart_count) > 3) {
			EXCEPT ("Error staring GridFTP!");
		} else {
			Restart (gridftp);
		}
	} else {
			// Who is this?
	}
}

GridftpServer *
GridftpManager::CreateNew() {
	m_gridftp = new GridftpServer();
	
	Restart (m_gridftp);
}

GridftpServer *
GridftpManager Restart(GridftpServer * gridftp) {
	gridftp->m_restart_count++;

	int io_redirect[3];
	io_redirect[0] = -1;
	io_redirect[1] = gridftp->m_stdout_fds[1];
	io_redirect[2] = -1;

	char * gridftp_path = NULL;
	gridftp_path = param ("GRIDFTP_WRAPPER");
	if (!gridftp_path) {
		return NULL;
	}

		// Re-use the old port if possible
	MyString gridftp_args = "";
	if (!gridftp->m_port.IsEmpty()) {
		gridftp_args += " -p ";
		gridftp_args += gridftp->m_port;
	}

	m_gridftp->m_pid = daemonCore->CreateProcess (	
			gridftp_path, 
			gridftp_args.Value(),		// Args
			PRIV_UNKNOWN,	// Priv State ---- keep the same 
			m_reaperid,		// id for our registered reaper
			FALSE,			// do not want a command port
			newenv,			// env
			NULL,			// cwd
			FALSE,			// new process group?
			NULL,			// network sockets to inherit
			io_redirect 	// redirect stdin/out/err
			);		

	return m_gridftp;
}

GridftpServer *
GridftpManager::FindOrCreate() {
	if (m_gridftp) {
		return m_gridftp;
	} else {
		return CreateNew();
	}
}

void
GridftpManager::ShutdownAll() {
	Shutdown (m_gridftp);
}

void
GridftpManager::Shutdown (GridftpServer * gridftp) {
	daemonCore->Shutdown_Graceful (gridftp->m_pid);
	gridftp->SetState (GridftpServer::DEAD);
}



GridftpServer::GridftpServer() {
	m_retry_count = 0;
	m_state = GridftpServer::STARTING;

	// GridFTP server will print it's port to stdout
	// We need to listen for it, so that we can figure out 
	// the port, and stick that into RSL in the future
	
	if (!daemonCore->Create_Pipe (m_stdout_fds)) {
		EXCEPT ("Unable to create GridFTP stdout pipe");
	}

	if (!daemonCore->Register_Pipe (
									m_stdout_fds[0],
									"GridFTP stdout",
									(PipeHandlercpp)GridftpServer::StdoutReady();
									"GridftpServer::StdoutReady()",
									this)) {
		EXCEPT ("Unable to register GridFTP stdout pipe");
	}
}

int
GridftpServer::StdoutReady() {
		// Wait till you can read a whole line, parse the port number
	if (this->GetState() == GridftpServer::STARTING) {
		char buff[501];		
		int bytes_read = 0;
		while ((bytes_read = read (m_stdout_fds[0], buff, 500)) > 0) {
			buff[byte_read] = '\0';
			m_stdout_buff += buff;
		}
		
		int retpos = m_stdout_buff.FindChar ((int)'\n');
		if (retpos > -1) {
			const char * search_for = "Server listening at ";
			m_address = m_stdout_buff.Substr(
							  strlen (search_for),
						      retpos);

			m_port = m_address.Substr (
									   m_address.FindChar (':') + 1,
									   m_address.Length());

			this->SetState (GridftpServer::RUNNING);
		}
	} else {
		daemonCore->Cancel_Pipe (m_stdout_fds[0]);
	}

	return TRUE;
}

const char *
GridftpServer::GetAddress() {
	if (GetState() != GridftpServer::RUNNING) 
		return NULL;

	return m_address.Value();
}

int 
GridftpServer::GetState() {
	return m_state;
}

void
GridftpServer::SetState(int state) {
	m_state = state;
}



