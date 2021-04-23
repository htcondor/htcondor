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

#ifndef __SHARED_PORT_SERVER_H__
#define __SHARED_PORT_SERVER_H__

#include "shared_port_client.h"
#include "forkwork.h"

// SharedPortServer forwards connections received on this daemon's
// command port to other daemons on the same machine through their
// SharedPortEndpoint.  This enables Condor daemons to share a single
// network port.   For security reasons, the target daemons must
// all use the same DAEMON_SOCKET_DIR as configured in this daemon.

class SharedPortServer: Service {
 public:
	SharedPortServer();
	~SharedPortServer();

	void InitAndReconfig();

		// Remove address file left over from a previous run that exited
		// without proper cleanup.
	static void RemoveDeadAddressFile();

 private:
	bool m_registered_handlers;
	std::string m_shared_port_server_ad_file;
	int m_publish_addr_timer;
	SharedPortClient m_shared_port_client;
	std::string m_default_id;
	ForkWork forker;

	int HandleConnectRequest(int cmd,Stream *sock);
	int HandleDefaultRequest(int cmd,Stream *sock);
	int PassRequest(Sock *sock, const char *shared_port_id);
	void PublishAddress();
};

#endif
