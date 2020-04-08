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


#ifndef _CONDOR_CCB_CLIENT_H
#define _CONDOR_CCB_CLIENT_H

/*
 CCBClient: the Condor Connection Broker Client
 This is instantiated by CEDAR when a client wants to connect to a daemon
 that is not directly accessible (e.g. because the daemon is in a private
 network).  The client requests the daemon's CCB server to inform the
 daemon of the request.  If all goes well, the daemon connects to the client
 and then CEDAR continues on its way as though it had created the connection
 the normal way from the client to the server.
 */

#include "classy_counted_ptr.h"
#include "reli_sock.h"
#include "dc_message.h"
#include "shared_port_endpoint.h"

//
// ReliSock (the only constructor of CCBClient objects) doesn't need to
// to know if the CCB client immediately makes a request or if the requests
// are being batched up -- if it's making a nonblocking connection, it
// has to wait for the callback in either case.
//
class CCBClientFactory;

class CCBClient: public Service, public ClassyCountedPtr {
 // Only CCBClientFactory can make instances of this class.
 friend class CCBClientFactory;
 protected:
	CCBClient( char const *ccb_contact, ReliSock *target_sock );

 public:
	virtual ~CCBClient();

	virtual bool ReverseConnect( CondorError *error, bool non_blocking );
	virtual void CancelReverseConnect();

	virtual const std::string & connectID() { return m_connect_id; }

	static std::string myName();

	void RegisterReverseConnectCallback();
	void UnregisterReverseConnectCallback();

 protected:
	virtual bool try_next_ccb();

	static bool SplitCCBContact( char const *ccb_contact, std::string &ccb_address, std::string &ccbid, const std::string & peer, CondorError *error );

	MyString        m_ccb_contact;
	StringList      m_ccb_contacts;
	ReliSock *      m_target_sock;
	std::string     m_target_peer_description;

	std::string     m_cur_ccb_address;

 private:
	Sock *          m_ccb_sock;
	std::string     m_connect_id;
	DCMsgCallback * m_ccb_cb;
	int             m_deadline_timer;

	bool ReverseConnect_blocking( CondorError *error );

	bool AcceptReversedConnection(std::shared_ptr<ReliSock> listen_sock,std::shared_ptr<SharedPortEndpoint> shared_listener);
	bool HandleReversedConnectionRequestReply(CondorError *error);

	void CCBResultsCallback(DCMsgCallback *cb);
	void ReverseConnectCallback(Sock *sock);
	static int ReverseConnectCommandHandler(int cmd,Stream *stream);
	void DeadlineExpired();

	// CCB contact information should be an opaque token to everyone, but
	// Sinful needs to be able parse CCB IDs to generate v1 addresses.
	friend class Sinful;
};

class CCBClientFactory {
	// This class is a static singleton.
	private:
		CCBClientFactory();
		~CCBClientFactory();

	public:
		static CCBClient * make( bool nonBlocking,
		                         const char * ccbContact, ReliSock * target );
};

class BatchedCCBClient;
class CCBConnectionBatcher {
	private:
		CCBConnectionBatcher();
		~CCBConnectionBatcher();

	public:
		static bool make( BatchedCCBClient * client ) ;

	protected:
		static void BatchTimerCallback();

	private:
		class Broker {
			public:
				Broker() { }

				int timerID {-1};
				time_t deadline {0};
				std::map< std::string, BatchedCCBClient * > clients;

                void HandleBrokerFailure();
                void ForgetClientAndTryNextBroker( const std::string & connectID, BatchedCCBClient * client = NULL );
		};

		static std::map< std::string, Broker > brokers;
};

class BatchedCCBClient : public CCBClient {
	// Only CCBClientFactory can make instances of this class.
	friend class CCBClientFactory;
	protected:
		BatchedCCBClient( char const * ccbContact, ReliSock * target );

	public:
		virtual ~BatchedCCBClient();

		bool ReverseConnect( CondorError * error, bool nonBlocking ) override;
		void CancelReverseConnect() override;

		const std::string & currentCCBAddress() { return m_cur_ccb_address; }
		const std::string & ccbID() { return m_ccb_id; }

        virtual void FailReverseConnect();

	protected:
		bool try_next_ccb() override;

		std::string m_ccb_id;

    // Should only be needed for try_next_ccb(); remove when possible.
    friend class CCBConnectionBatcher;
};

#endif
