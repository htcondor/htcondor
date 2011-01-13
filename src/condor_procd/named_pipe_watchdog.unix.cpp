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
#include "named_pipe_watchdog.unix.h"

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
