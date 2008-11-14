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

char*
NamedPipeWatchdogServer::get_path()
{
	ASSERT(m_initialized);
	return m_path;
}
