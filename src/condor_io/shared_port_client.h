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

#ifndef __SHARED_PORT_CLIENT_H__
#define __SHARED_PORT_CLIENT_H__

#include "reli_sock.h"

class SharedPortState;

class SharedPortClient {

friend class SharedPortState;

 public:
	bool sendSharedPortID(char const *shared_port_id,Sock *sock);

	// PassSocket() returns TRUE on success, FALSE on False, or KEEP_STREAM if 
	// non_blocking == true and the caller should NOT delete sock_to_pass because
	// the operation is still pending (it will be deleted once the operation is complete).
	int PassSocket(Sock *sock_to_pass,char const *shared_port_id,char const *requested_by=NULL, bool non_blocking = false);

	unsigned int get_currentPendingPassSocketCalls() 
		{return m_currentPendingPassSocketCalls;}
	unsigned int get_maxPendingPassSocketCalls() 
		{return m_maxPendingPassSocketCalls;}
	unsigned int get_successPassSocketCalls() 
		{return m_successPassSocketCalls;}
	unsigned int get_failPassSocketCalls() 
		{return m_failPassSocketCalls;}
	unsigned int get_wouldBlockPassSocketCalls() 
		{return m_wouldBlockPassSocketCalls;}

 private:
	std::string myName();
	bool static SharedPortIdIsValid(char const *name);

	// Some operational metrics filled in by the SharedPortState
	// class, which does the heavy lifting during a call to PassSocket().
	// For now, these are just simple counters.  Eventually they
	// should use statistics from generic_stats.h.
	// These metrics are static so that SharedPortState does not 
	// need to hold a pointer back to a SharedPortClient instance which
	// may go stale (during shutdown).
	static unsigned int m_currentPendingPassSocketCalls;
	static unsigned int m_maxPendingPassSocketCalls;
	static unsigned int m_successPassSocketCalls;
	static unsigned int m_failPassSocketCalls;
	static unsigned int m_wouldBlockPassSocketCalls;
};

#endif
