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
#include <iostream>
#include "condor_debug.h"
#include "local_client.h"

#if defined(WIN32)
#define PIPE_ADDR "\\\\.\\pipe\\local_server_test"
#else
#define PIPE_ADDR "/tmp/local_server_test"
#endif

int
main()
{
	dprintf_set_tool_debug("TOOL", 0);

	LocalClient* client = new LocalClient;
	ASSERT(client != NULL);

	if (!client->initialize(PIPE_ADDR)) {
		EXCEPT("unable to initialize LocalClient");
	}

	while (true) {
		char c1, c2;
		sin >> c1;
		if (!std::cin) {
			if (!std::cin.eof()) {
				std::cout << "error in input stream" << std::endl;
			}
			break;
		}
		if (!client->start_connection(&c1, sizeof(char))) {
			EXCEPT("error in LocalClient::start_connection");
		}
		if (!client->read_data(&c2, sizeof(char))) {
			EXCEPT("error in LocalClient::read_data");
		}
		client->end_connection();
		std::cout << "received " << c2 << std::endl;
		if (c2 == 'q') {
			break;
		}
	}

	return 0;
}
