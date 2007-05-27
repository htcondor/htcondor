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
#include "named_pipe_watchdog_server.h"
#include "named_pipe_util.h"

bool
NamedPipeWatchdogServer::initialize(const char* path)
{
	ASSERT(!m_initialized);

	if (!named_pipe_create(path, m_read_fd, m_write_fd)) {
		dprintf(D_ALWAYS,
		        "failed to initialize watchdog named pipe at %s\n",
		        path);
		return false;
	}

	m_path = strdup(path);
	ASSERT(m_path != NULL);

	m_initialized = true;

	return true;
}

NamedPipeWatchdogServer::~NamedPipeWatchdogServer()
{
	if (!m_initialized) {
		return;
	}

	close(m_write_fd);
	close(m_read_fd);

	unlink(m_path);

	free(m_path);
}
