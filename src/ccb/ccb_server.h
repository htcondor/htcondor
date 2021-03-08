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

#ifndef __CCB_H__
#define __CCB_H__

/* CCBServer: the Condor Connection Broker
   This is instantiated by the collector to provide connection brokering
   service.  Daemons (called "targets") register with the server and keep
   a TCP connection to it to exchange messages with the server.  Clients
   send requests to the server to get a target daemon to do a reverse
   connect to the client.
 */

#include "dc_service.h"

class StatisticsPool;
void AddCCBStatsToPool(StatisticsPool& pool, int publevel);

class Sock;
class Stream;

class CCBTarget;
class CCBServerRequest;
class CCBReconnectInfo;

typedef unsigned long CCBID;

// condor/cedar connection broker
class CCBServer: Service {
 public:
	CCBServer();
	~CCBServer();

	void InitAndReconfig();

	static void CCBIDToContactString( char const * my_address, CCBID ccbid, std::string & ccb_contact );
	static bool CCBIDFromString( CCBID & ccbid, char const * ccbid_str );

	friend class CCBTarget;
 private:
	bool m_registered_handlers;
	HashTable<CCBID,CCBTarget *> m_targets;        // ccbid --> target
	HashTable<CCBID,CCBReconnectInfo *> m_reconnect_info;
	std::string m_address;
	std::string m_reconnect_fname;
	FILE *m_reconnect_fp;
	time_t m_last_reconnect_info_sweep;
	int m_reconnect_info_sweep_interval;
	bool m_reconnect_allowed_from_any_ip;
	CCBID m_next_ccbid;
	CCBID m_next_request_id;
	int m_read_buffer_size;
	int m_write_buffer_size;

		// we hold onto client requests so we can propagate failures
		// to them if things go wrong
	HashTable<CCBID,CCBServerRequest *> m_requests;// request_id --> req

	int m_polling_timer;
		// The epoll file descriptor.  Only used on platforms where
		// epoll is available.
	int m_epfd;

	void AddTarget( CCBTarget *target );
	void RemoveTarget( CCBTarget *target );
	CCBTarget *GetTarget( CCBID ccbid );
	bool ReconnectTarget( CCBTarget *target, CCBID reconnect_cookie );

	void AddRequest( CCBServerRequest *request, CCBTarget *target );
	void RemoveRequest( CCBServerRequest *request );
	CCBServerRequest *GetRequest( CCBID request_id );

	void SendHeartbeatResponse( CCBTarget *target );

	void ForwardRequestToTarget( CCBServerRequest *request, CCBTarget *target );
	void RequestReply( Sock *sock, bool success, char const *error_msg, CCBID request_cid, CCBID target_cid );

	void RequestFinished( CCBServerRequest *request, bool success, char const *error_msg );

	int HandleRequestDisconnect( Stream *stream );

	void PollSockets();
	int EpollSockets(int);
	void EpollAdd(CCBTarget *);
	void EpollRemove(CCBTarget *);
	void SetSmallBuffers(Sock *sock) const;

	int HandleRegistration(int cmd,Stream *stream);
	int HandleRequest(int cmd,Stream *stream);
	void HandleRequestResultsMsg( CCBTarget *target );
	int HandleRequestResultsMsg( Stream *stream );

	void RegisterHandlers();

	CCBReconnectInfo *GetReconnectInfo(CCBID ccbid);
	void AddReconnectInfo( CCBReconnectInfo *reconnect_info );
	void RemoveReconnectInfo( CCBReconnectInfo *reconnect_info );
	void CloseReconnectFile();
	bool OpenReconnectFileIfExists();
	bool OpenReconnectFile(bool only_if_exists=false);
	void LoadReconnectInfo();
	bool SaveReconnectInfo(CCBReconnectInfo *reconnect_info);
	void SaveAllReconnectInfo();
	void SweepReconnectInfo();
};

// the CCB server's state associated with a target daemon
class CCBTarget {
 public:
	CCBTarget(Sock *sock);
	~CCBTarget();

	CCBID getCCBID() const { return m_ccbid; }
	void setCCBID(CCBID ccbid) { m_ccbid = ccbid; }
	Sock *getSock() { return m_sock; }

	HashTable<CCBID,CCBServerRequest *> *getRequests() { return m_requests; }
	void AddRequest(CCBServerRequest *request, CCBServer *ccb_server);
	void RemoveRequest(CCBServerRequest *request);
	int NumRequests() { return m_requests ? m_requests->getNumElements() : 0; }

	// expecting a response from this target for a request we just sent it
	void incPendingRequestResults(CCBServer *ccb_server);
	// just received a response from this target
	void decPendingRequestResults();

 private:
	Sock *m_sock;         // used to send CCB messages to this target daemon
	CCBID m_ccbid;        // CCBServer-assigned identifier
	int m_pending_request_results;
	bool m_socket_is_registered;

		// requests directed to this target daemon
	HashTable<CCBID,CCBServerRequest *> *m_requests;// request_id --> req
};

// represents a client that connected to CCBServer and requested a connection
// from a registered target
class CCBServerRequest {
 public:
	CCBServerRequest(Sock *sock,CCBID target_ccbid,char const *return_addr,char const *connect_id);
	~CCBServerRequest();

	Sock *getSock() { return m_sock; }
	void setRequestID( CCBID request_id ) { m_request_id = request_id; }
	CCBID getRequestID() const { return m_request_id; }
	CCBID getTargetCCBID() const { return m_target_ccbid; }
	char const *getReturnAddr() { return m_return_addr.c_str(); }
	char const *getConnectID() { return m_connect_id.c_str(); }

 private:
	Sock *m_sock;
	CCBID m_target_ccbid;    // target daemon requested by client
	CCBID m_request_id;      // CCBServer-assigned identifier for this request
	std::string m_return_addr;
	std::string m_connect_id;
};

class CCBReconnectInfo {
public:
	CCBReconnectInfo(CCBID ccbid,CCBID reconnect_cookie,char const *peer_ip);

	void alive()                     { m_last_alive = time(NULL); }
	time_t getLastAlive() const      { return m_last_alive; }
	CCBID getCCBID() const           { return m_ccbid; }
	CCBID getReconnectCookie() const { return m_reconnect_cookie; }
	char const *getPeerIP() const    { return m_peer_ip; }

private:
	CCBID m_ccbid;
	CCBID m_reconnect_cookie;
	time_t m_last_alive;
	char m_peer_ip[IP_STRING_BUF_SIZE];
};

#endif
