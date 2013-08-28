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

#ifndef __CCB_LISTENER_H__
#define __CCB_LISTENER_H__

/*
 CCBListener: the Condor Connection Brokern Listener
 This is used by daemonCore to register with the CCB server
 so that this daemon can respond to requests from clients for
 reverse connects (e.g. because this server is inside of a private
 network and the client is on a public network).
 */

#include "condor_daemon_core.h"
#include "simplelist.h"

class CCBListener: public Service, public ClassyCountedPtr {
 public:
	CCBListener(char const *ccb_address);
	~CCBListener();

		// This is called at initial creation time and on reconfig.
	void InitAndReconfig();

		// must call this after creating CCBListener, or it doesn't do anything
		// returns true if blocking and registration succeeded
		// otherwise, keeps trying periodically
	bool RegisterWithCCBServer(bool blocking=false);

		// unique ID of this CCBListener in namespace of the CCB server
	char const *getCCBID() const { return m_ccbid.Value(); }

	char const *getAddress() const { return m_ccb_address.Value(); }

	bool operator ==(CCBListener const &other);

 private:
	MyString m_ccb_address;
	MyString m_ccbid;
	MyString m_reconnect_cookie;
	Sock *m_sock;
	bool m_waiting_for_connect;
	bool m_waiting_for_registration;
	bool m_registered;
	int m_reconnect_timer;
	int m_heartbeat_timer;
	int m_heartbeat_interval;
	int m_last_contact_from_peer;
	bool m_heartbeat_disabled;
	bool m_heartbeat_initialized;

	bool SendMsgToCCB(ClassAd &msg,bool blocking);
	bool WriteMsgToCCB(ClassAd &msg);
	static void CCBConnectCallback(bool success,Sock *sock,CondorError *errstack,void *misc_data);
	void ReconnectTime();
	void Connected();
	void Disconnected();
	int HandleCCBMsg(Stream *sock);
	bool ReadMsgFromCCB();
	bool HandleCCBRegistrationReply( ClassAd &msg );
	bool HandleCCBRequest( ClassAd &msg );
	bool DoReversedCCBConnect( char const *address, char const *connect_id, char const *request_id,char const *peer_description);
	int ReverseConnected(Stream *stream);
	void ReportReverseConnectResult(ClassAd *connect_msg,bool success,char const *error_msg=NULL);

	void HeartbeatTime();
	void RescheduleHeartbeat();
	void StopHeartbeat();
};

class CCBListeners {
 public:
		// format of addresses: "<ccb1> <ccb2> ..."
	void Configure(char const *addresses);

		// find CCB listener for given CCB address
	CCBListener *GetCCBListener(char const *address);

		// returns string representation of list of CCB id(s)
		// example: "<ccb server>#<ccbid> <ccb server2>#<ccbid2> ..."
	void GetCCBContactString(MyString &result);

	bool RegisterWithCCBServer(bool blocking=false);

 private:
	typedef std::list< classy_counted_ptr<CCBListener> > CCBListenerList;
	CCBListenerList m_ccb_listeners;
	MyString m_ccb_contact;
};

#endif
