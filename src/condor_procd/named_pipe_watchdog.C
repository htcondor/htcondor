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
#include "named_pipe_watchdog.h"

bool
NamedPipeWatchdog::initialize(const char* path)
{
	ASSERT(!m_initialized);

	m_pipe_fd = safe_open_wrapper(path, O_RDONLY | O_NONBLOCK);
	if (m_pipe_fd == -1) {
		dprintf(D_ALWAYS,
		        "error opening watchdog pipe %s: %s (%d)\n",
		        path,
		        strerror(errno),
		        errno);
		return false;
	}

	m_initialized = true;
	return true;
}

NamedPipeWatchdog::~NamedPipeWatchdog()
{
	if (!m_initialized) {
		return;
	}

	close(m_pipe_fd);
}

int
NamedPipeWatchdog::get_file_descriptor()
{
	ASSERT(m_initialized);

	return m_pipe_fd;
}
