#ifndef REPLICATION_H
#define REPLICATION_H

#include "constants2.h"
#include "_condor_fix_types.h"
#include <netinet/in.h>

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
