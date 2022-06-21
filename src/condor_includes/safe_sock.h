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


#ifndef SAFE_SOCK_H
#define SAFE_SOCK_H

#if defined(WIN32)
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif

#include "sock.h"
#include "my_hostname.h"
#include "condor_random_num.h"
#include "SafeMsg.h"

static const int SAFE_SOCK_HASH_BUCKET_SIZE = 7;
static const int SAFE_SOCK_MAX_BTW_PKT_ARVL = 10;

/*
**	S A F E    S O C K
*/

/** The SafeSock class implements the Sock interface with UDP. */

class SafeSock : public Sock {

//	PUBLIC INTERFACE TO SAFE SOCKS
//
public:
	friend class DaemonCore;

	// Virtual socket services
	virtual int handle_incoming_packet();
	virtual int end_of_message();
	virtual bool peek_end_of_message();

	/**	@return true if a complete message is ready to be read
	*/
	virtual bool msgReady();

	virtual stream_type type() const { return Stream::safe_sock; }

	/** Connect to a host on a port
        @param port the port to connect to, ignorred if s includes port
        @param s can be a hostname or sinful string
		@param do_not_block if True, call returns immediately
    **/
	virtual int connect(char const *s, int port=0, bool do_not_block = false);

	virtual int close();

	virtual int do_reverse_connect(char const *ccb_contact,bool nonblocking);

	virtual void cancel_reverse_connect();
	virtual int do_shared_port_local_connect( char const *shared_port_id, bool nonblocking,char const *sharedPortIP );

	/// my IP address, string version (e.g. "128.105.101.17")
	virtual const char* my_ip_str() const;

	//
	inline int connect(char const *h, char *s) { return connect(h,getportbyserv(s));}
	void getStat(unsigned long &noMsgs,
			 unsigned long &noWhole,
			 unsigned long &noDeleted,
			 unsigned long &avgMsgSize,
                   unsigned long &szComplete,
			 unsigned long &szDeleted);
	void resetStat();

	//
	SafeSock();

	// Copy constructor
	SafeSock(const SafeSock &);

	/// Create a copy of this stream (e.g. dups underlying socket).
	/// Caller should delete the returned stream when finished with it.
	Stream *CloneStream();

	// Destructor
	~SafeSock();

	// Methods
	void init();	/* shared initialization method */

    const char * isIncomingDataHashed();
    const char * isIncomingDataEncrypted();

#ifdef DEBUG
	int getMsgSize();
	void dumpSock();
#endif

#ifndef WIN32
	// interface no longer supported
	int attach_to_file_desc(int);
#endif
	static int recvQueueDepth(int port);
	

	//	byte operations
	///
	virtual int put_bytes(const void *, int);
	///
	virtual int get_bytes(void *, int);
	///
	virtual int get_ptr(void *&, char);
	///
	virtual int peek(char &);

	// serialize and deserialize
	const char * serialize(const char *);
	char * serialize() const;

//	PRIVATE INTERFACE TO SAFE SOCKS
//

// next line should be uncommented after testing
protected:

	virtual void setTargetSharedPortID( char const *id );
	virtual bool sendTargetSharedPortID();

    bool init_MD(CONDOR_MD_MODE mode, KeyInfo * key, const char * keyId);
    bool set_encryption_id(const char * keyId);

	static _condorMsgID _outMsgID;

	enum safesock_state { safesock_none, safesock_listen };

	inline bool same(const _condorMsgID msgA,
					 const _condorMsgID msgB)
	{
		if(msgA.ip_addr == msgB.ip_addr &&
		   msgA.pid == msgB.pid &&
		   msgA.time == msgB.time &&
		   msgA.msgNo == msgB.msgNo)
			return true;
		else return false;
	}

	safesock_state	_special_state;
	_condorOutMsg _outMsg;
	_condorInMsg *_inMsgs[SAFE_SOCK_HASH_BUCKET_SIZE];
	_condorPacket _shortMsg;
	bool _msgReady;
	_condorInMsg *_longMsg;
    Condor_MD_MAC * mdChecker_;
	int _tOutBtwPkts;
	int m_udp_network_mtu;
	int m_udp_loopback_mtu;

	// statistics variables
	static unsigned long _noMsgs;
	static unsigned long _whole;;
	static unsigned long _deleted;
	static unsigned long _avgSwhole;
	static unsigned long _avgSdeleted;
};


#endif /* SAFE_SOCK_H */
