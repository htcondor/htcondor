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

class CCBServer;

class CCBListener: public Service, public ClassyCountedPtr {
 public:
	CCBListener(char const *ccb_address);
	~CCBListener();

		// When this listener belongs to a CCB server's upstream registration, the
		// server sets itself here so a routed (tunneled) reverse-connect request --
		// one carrying a remaining CCBRoute -- can be handed back to the server to
		// set up the next hop and relay, instead of being handled locally.
	void SetCCBServer( CCBServer *s ) { m_ccb_server = s; }

		// Fires each time this listener (re)registers and is assigned a ccbid --
		// used instead of polling to learn when upstream registration completes.
	void SetRegistrationCallback( std::function<void()> cb ) { m_on_registered = std::move(cb); }

		// This is called at initial creation time and on reconfig.
	void InitAndReconfig();

		// must call this after creating CCBListener, or it doesn't do anything
		// returns true if blocking and registration succeeded
		// otherwise, keeps trying periodically
	bool RegisterWithCCBServer(bool blocking=false);

		// unique ID of this CCBListener in namespace of the CCB server
	char const *getCCBID() const { return m_ccbid.c_str(); }

	char const *getAddress() const { return m_ccb_address.c_str(); }

	bool operator ==(CCBListener const &other);

 private:
	std::string m_ccb_address;
	std::string m_ccbid;
	std::string m_reconnect_cookie;
	Sock *m_sock;
	bool m_waiting_for_connect;
	bool m_waiting_for_registration;
	bool m_registered;
	int m_reconnect_timer;
	int m_heartbeat_timer;
	time_t m_heartbeat_interval;
	time_t m_last_contact_from_peer;
	bool m_heartbeat_disabled;
	bool m_heartbeat_initialized;
	CCBServer *m_ccb_server{nullptr};   // set iff this is a CCB's upstream listener
	std::function<void()> m_on_registered;   // fired when (re)registration completes

	bool SendMsgToCCB(ClassAd &msg,bool blocking);
	bool WriteMsgToCCB(ClassAd &msg);
	void handleCCBConnect(bool success, Sock *sock);
	void ReconnectTime(int timerID = -1);
	void Connected();
	void Disconnected();
	int HandleCCBMsg(Stream *sock);
	bool ReadMsgFromCCB();
	bool HandleCCBRegistrationReply( ClassAd &msg );
	bool HandleCCBRequest( ClassAd &msg );
	bool DoReversedCCBConnect( char const *address, char const *connect_id, char const *request_id,char const *peer_description, ClassAd const *route_ad = nullptr);
	int ReverseConnected(Stream *stream);
	void ReportReverseConnectResult(ClassAd *connect_msg,bool success,char const *error_msg=NULL);

	void HeartbeatTime(int timerID = -1);
	void RescheduleHeartbeat();
	void StopHeartbeat();
};

class CCBListeners {
 public:
		// format of addresses: "<ccb1> <ccb2> ..."
	void Configure(char const *addresses);

		// Propagate the owning CCB server to all current and future listeners (see
		// CCBListener::SetCCBServer).
	void SetCCBServer( CCBServer *s );

		// Propagate a registration-completion callback to all current and future
		// listeners (see CCBListener::SetRegistrationCallback).
	void SetRegistrationCallback( std::function<void()> cb );

		// find CCB listener for given CCB address
	CCBListener *GetCCBListener(char const *address);

		// returns string representation of list of CCB id(s)
		// example: "<ccb server>#<ccbid> <ccb server2>#<ccbid2> ..."
	void GetCCBContactString(std::string &result);

		// returns how many broker registrations succeeded
	int RegisterWithCCBServer(bool blocking=false);

	size_t size() const { return m_ccb_listeners.size(); }

 private:
	typedef std::list< classy_counted_ptr<CCBListener> > CCBListenerList;
	CCBListenerList m_ccb_listeners;
	std::string m_ccb_contact;
	CCBServer *m_ccb_server{nullptr};
	std::function<void()> m_on_registered;
};

#endif
