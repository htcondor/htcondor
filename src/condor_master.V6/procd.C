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
#include "procd.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "../condor_procapi/procapi.h"

ProcD::ProcD() :
	m_pid(-1)
{
	m_binary_path = param("PROCD");
	if (m_binary_path == NULL) {
		EXCEPT("error: PROCD not defined in configuration");
	}

	m_address = param("PROCD_ADDRESS");
	if (m_address == NULL) {
		EXCEPT("error: PROCD_ADDRESS not defined in configuration");
	}

	m_log_file = param("PROCD_LOG");

	m_max_snapshot_interval = param("PROCD_MAX_SNAPSHOT_INTERVAL");

	m_debug = param_boolean("PROCD_DEBUG", false);

	// gather the information we need about the procd's "root process"
	// (which is going to be us, the master). we'll go ahead and get this
	// info into string-form, since we'll be using it in arguments on the
	// procd's command line

	// root PID
	//
	int ret;
	pid_t root_pid = daemonCore->getpid();
	ret = snprintf(m_root_pid, MAX_PID_STR_LEN, "%u", root_pid);
	if (ret < 0) {
		EXCEPT("snprintf error: %s (%d)", strerror(errno), errno);
	}
	if (ret >= MAX_PID_STR_LEN) {
		EXCEPT("string form of PID %u exceeds %d chars",
		       root_pid,
		       MAX_PID_STR_LEN);
	}

	// root birthday
	//
	int ignored;
	procInfo* pi = NULL;
	if (ProcAPI::getProcInfo(root_pid, pi, ignored) != PROCAPI_SUCCESS) {
		EXCEPT("unable to get own process information");
	}
	ret = snprintf(m_root_birthday,
	               MAX_BIRTHDAY_STR_LEN,
	               PROCAPI_BIRTHDAY_FORMAT,
	               pi->birthday);
	if (ret < 0) {
		EXCEPT("snprintf error: %s (%d)", strerror(errno), errno);
	}
	if (ret >= MAX_BIRTHDAY_STR_LEN) {
		EXCEPT("string form of process birthday " PROCAPI_BIRTHDAY_FORMAT " exceeds %d chars",
		       pi->birthday,
		       MAX_BIRTHDAY_STR_LEN);
	}
	free(pi);

#if defined(WIN32)
	// on Windows, we need to tell the procd what program to use to send
	// softkills
	//
	m_softkill_binary = param("SOFTKILL_BINARY");
	if (m_softkill_binary == NULL) {
		EXCEPT("error: SOFTKILL_BINARY not defined");
	}
#endif

}

ProcD::~ProcD()
{
	if (m_pid != -1) {
		stop();
	}
	free(m_binary_path);
	free(m_address);
	if (m_log_file != NULL) {
		free(m_log_file);
	}
	if (m_max_snapshot_interval != NULL) {
		free(m_max_snapshot_interval);
	}
#if defined(WIN32)
	free(m_softkill_binary);
#endif
}

void
ProcD::start()
{
	// register a reaper for notification when the procd exits
	//
	int reaper_id = daemonCore->Register_Reaper("condor_procd reaper",
	                                            (ReaperHandlercpp)&ProcD::reaper,
	                                            "condor_procd reaper",
	                                            this);
	if (reaper_id == FALSE) {
		EXCEPT("unable to register a reaper for the procd");
	}

	// we start the procd with a pipe coming back to us on its stderr.
	// the procd will close this pipe after it is listening for commands.
	//
	int pipe_ends[2];
	if (daemonCore->Create_Pipe(pipe_ends) == FALSE) {
		EXCEPT("error creating pipe for the procd");
	}
	int std_io[3];
	std_io[0] = -1;
	std_io[1] = -1;
	std_io[2] = pipe_ends[1];

	// start constructing the argument list
	//
	ArgList args;
	MyString err;
	if (!args.AppendArgsV1RawOrV2Quoted(m_binary_path, &err)) {
		EXCEPT("error constructing procd command line: %s", err.Value());
	}

	// the procd's address
	//
	args.AppendArg("-A");
	args.AppendArg(m_address);

	// information about the procd's "root process"
	//
	args.AppendArg("-P");
	args.AppendArg(m_root_pid);
	args.AppendArg(m_root_birthday);

	// the (optional) procd log file
	//
	if (m_log_file != NULL) {
		args.AppendArg("-L");
		args.AppendArg(m_log_file);
	}

	// the (optional) maximum snapshot interval
	// (the procd will default to every minute)
	//
	if (m_max_snapshot_interval != NULL) {
		args.AppendArg("-S");
		args.AppendArg(m_max_snapshot_interval);
	}

	// (optional) make the procd sleep on startup so a
	// debugger can attach
	//
	if (m_debug) {
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
	// on Windows, tell the procd what program to use for sending softkills
	//
	args.AppendArg("-K");
	args.AppendArg(m_softkill_binary);
#endif

	// use Create_Process to start the procd
	//
	m_pid = daemonCore->Create_Process(args.GetArg(0),
	                                   args,
	                                   PRIV_ROOT,
	                                   reaper_id,
	                                   FALSE,
	                                   NULL,
	                                   NULL,
	                                   NULL,
	                                   NULL,
	                                   std_io);
	if (m_pid == FALSE) {
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
}

void
ProcD::stop()
{
	// send a quit command to the procd
	//
	ProcFamilyClient client(m_address);
	if (!client.quit()) {
		dprintf(D_ALWAYS, "error sending QUIT command to condor_procd\n");
	}
	
	// reset the PID so that our reaper doesn't except if it
	// goes off
	//
	m_pid = -1;
}

int
ProcD::reaper(int pid, int status)
{
	if (m_pid == -1) {
		dprintf(D_ALWAYS, "procd (pid = %d) exited with status %d\n",
		        pid,
		        status);
	}
	else {
		ASSERT(pid == m_pid);
		EXCEPT("procd (pid = %d) exited unexpectedly with status %d",
		       pid,
		       status);
	}

	return 0;
}
