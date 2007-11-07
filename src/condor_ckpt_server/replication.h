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

#ifndef REPLICATION_H
#define REPLICATION_H

#include "constants2.h"

class ReplicationEvent
{
public:
	ReplicationEvent();
	~ReplicationEvent();
	int Prio() { return _prio; }
	void Prio(int x) { _prio = x; }
	struct sockaddr_in ServerAddr() { return _server_addr; }
	void ServerAddr(struct sockaddr_in s) { _server_addr = s; }
	struct in_addr ShadowIP() { return _shadow_IP; }
	void ShadowIP(struct in_addr s) { _shadow_IP = s; }
	char *Owner() { return _owner_name; }
	void Owner(char *o);		// stores a copy
	char *File() { return _file; }
	void File(char *f);			// stores a copy
	friend class ReplicationSchedule;
private:
	int _prio;
	struct sockaddr_in _server_addr;
	struct in_addr _shadow_IP;
	char *_owner_name;
	char *_file;
	ReplicationEvent *next;
};

// note: this class does not allocate/deallocate ReplicationEvents
class ReplicationSchedule
{
public:
	ReplicationSchedule();
	~ReplicationSchedule();
	int InsertReplicationEvent(ReplicationEvent *event);
	ReplicationEvent *GetNextReplicationEvent();
private:
	struct ReplicationNode
	{
		int prio;
		ReplicationEvent *head, *tail;
		ReplicationNode *next;
	};
	ReplicationNode *_q;
};

#endif
