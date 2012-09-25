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
#include "local_server.h"

#if defined(WIN32)
#define PIPE_ADDR "\\\\.\\pipe\\local_server_test"
#else
#define PIPE_ADDR "/tmp/local_server_test"
#endif

int
main()
{
	dprintf_set_tool_debug("TOOL", 0);

	LocalServer* server = new LocalServer;
	ASSERT(server != NULL);
	if (!server->initialize(PIPE_ADDR)) {
		EXCEPT("unable to initialize LocalServer\n");
	}

	while (true) {
		bool ready;
		if (!server->accept_connection(10, ready)) {
			EXCEPT("error in LocalServer::accept_connection\n");
		}
		if (ready) {
			char c;
			if (!server->read_data(&c, sizeof(char))) {
				EXCEPT("error in LocalServer::read_data");
			}
			if (!server->write_data(&c, sizeof(char))) {
				EXCEPT("error in LocalServer::write_data");
			}
			if (!server->close_connection()) {
				EXCEPT("error in LocalServer::close_connection");
			}
			printf("received: %c\n", c);
			if (c == 'q') {
				break;
			}
		}
		else {
			printf("timeout!\n");
		}
	}

	return 0;
}
