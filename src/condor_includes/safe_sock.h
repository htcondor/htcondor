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

#include "buffers.h"
#include "sock.h"
#include "condor_constants.h"

/*
**	S A F E    S O C K
*/

/** The SafeSock class implements the Sock interface with UDP. */

class SafeSock : public Sock {

//	PUBLIC INTERFACE TO SAFE SOCKS
//
public:

	friend class DaemonCore;

	/*
	**	Methods
	*/

	// Virtual socket services
	//
    ///
	virtual int handle_incoming_packet();
    ///
	virtual int end_of_message();

    /** Connect to a host on a port
        @param port the port to connect to, ignorred if s includes port
        @param s can be a hostname or sinful string
    **/
	virtual int connect(char *s, int port=0);

    ///
	inline int connect(char *h, char *s) { return connect(h,getportbyserv(s));}

    ///
	SafeSock();

    ///
	~SafeSock();

    ///
	void init();				/* shared initialization method */

#ifndef WIN32
	// interface no longer supported
	int attach_to_file_desc(int);
#endif
	
	/*
	**	Stream protocol
	*/

    ///
	virtual stream_type type() { return Stream::safe_sock; }

	//	byte operations
	//
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
protected:

	/*
	**	Types
	*/

	enum safesock_state { safesock_none, safesock_listen };

	/*
	**	Methods
	*/
	char * serialize(char *);
	inline char * serialize() { return(serialize(NULL)); }

	/*
	**	Data structures
	*/

	int rcv_packet(SOCKET);
	class RcvMsg {
	public:
		RcvMsg() : buf(65536), ready(0) {}

		Buf			buf;
		int			ready;
	} rcv_msg;

	int snd_packet(int);
	class SndMsg {
	public:
		SndMsg() : buf(65536) {}

		Buf			buf;
	} snd_msg;

	safesock_state	_special_state;
};

#endif /* SAFE_SOCK_H */
