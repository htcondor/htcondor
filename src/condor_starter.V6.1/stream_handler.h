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

#ifndef STREAM_HANDLER_H
#define STREAM_HANDLER_H

#include "condor_common.h"
#include "condor_daemon_core.h"
#include <list>

#define STREAM_BUFFER_SIZE 4096

class StreamHandler : public Service {
public:
	StreamHandler();

	bool Init( const char * filename, const char * streamname, bool is_output, int f = -1 );
	int Handler( int fd );
	int GetJobPipe() const;

	static int ReconnectAll();

private:
	char            buffer[STREAM_BUFFER_SIZE];
	std::string     filename;
	std::string     streamname;
	bool            is_output;
	int             pipe_fds[2];
	int             job_pipe;
	int             handler_pipe;
	int             remote_fd;
	off_t           offset;
	int             flags;

	static std::list< StreamHandler * > handlers;

	bool Reconnect();
	void Disconnect();
	bool VerifyOutputFile();

	bool done;
	bool connected;
	int pending;
};

#endif
