/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
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
