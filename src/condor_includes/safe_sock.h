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

#ifndef SAFE_SOCK_H
#define SAFE_SOCK_H

#if defined(WIN32)
#include <winsock.h>
#else
#include <netinet/in.h>
#endif

#include "sock.h"
#include "condor_constants.h"
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

	virtual stream_type type() { return Stream::safe_sock; }

	/** Connect to a host on a port
        @param port the port to connect to, ignorred if s includes port
        @param s can be a hostname or sinful string
		@param do_not_block if True, call returns immediately
    **/
	virtual int connect(char *s, int port=0, bool do_not_block = false);

	//
	inline int connect(char *h, char *s) { return connect(h,getportbyserv(s));}
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

	// Destructor
	~SafeSock();

	// Methods
	void init();	/* shared initialization method */

    const char * isIncomingDataMD5ed();
    const char * isIncomingDataEncrypted();

	virtual void         setFullyQualifiedUser(char * u);
	virtual const char * getFullyQualifiedUser();
    virtual int          isAuthenticated();
    void                 setAuthenticated(bool authenticated = true);

#ifdef DEBUG
	int getMsgSize();
	void dumpSock();
#endif

#ifndef WIN32
	// interface no longer supported
	int attach_to_file_desc(int);
#endif
	

	//	byte operations
	///
	virtual int put_bytes(const void *, int);
	///
	virtual int get_bytes(void *, int);
	///
	virtual int get_ptr(void *&, char);
	///
	virtual int peek(char &);

//	PRIVATE INTERFACE TO SAFE SOCKS
//

// next line should be uncommented after testing
protected:

    bool init_MD(CONDOR_MD_MODE mode, KeyInfo * key, const char * keyId);
    bool set_encryption_id(const char * keyId);

	static _condorMsgID _outMsgID;

	enum safesock_state { safesock_none, safesock_listen };

	char * serialize(char *);
	char * serialize() const;
	inline bool SafeSock::same(const _condorMsgID msgA,
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
	int _tOutBtwPkts;
	char* _fqu;  // fully qualified username
    int   _authenticated;

	// statistics variables
	static unsigned long _noMsgs;
	static unsigned long _whole;;
	static unsigned long _deleted;
	static unsigned long _avgSwhole;
	static unsigned long _avgSdeleted;
};


#endif /* SAFE_SOCK_H */
