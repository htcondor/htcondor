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

 


#ifndef SOCK_H
#define SOCK_H

#include "stream.h"

// retry failed connects for CONNECT_TIMEOUT seconds
#define CONNECT_TIMEOUT 10

#if !defined(WIN32)
typedef int SOCKET;
#define INVALID_SOCKET -1
#endif

/*
**	B A S E    S O C K
*/

/** The Sock class defines an interface for connection establishment.
This interface is implemented with TCP in the ReliSock class and with
UDP in the SafeSock class.  ReliSock and SafeSock are the only Cedar
classes which should be instantiated; however, pointers to the Stream
base class are often used at points in the application where it is not
important if the underlying medium is TCP or UDP (for example, in a
daemon which handles the same commands over both TCP and UDP).
*/
class Sock : public Stream {

//	PUBLIC INTERFACE TO ALL SOCKS
//
public:

	friend class DaemonCore;

	/*
	**	Methods
	*/

	// Virtual socket services
	//

	virtual int handle_incoming_packet() { assert(0); return 0; }

	/** Connect the socket to a remote peer.
		@param host Hostname of peer, either a DNS name or an IP address, or 
		an IP address and port together in the format "&lt;IP:PORT&gt;", such as 
		<p> &lt;128.105.44.66:3354&gt;
		<b>Note:</b> The "&lt;" and "&gt;" are required when giving both the
		IP and Port.
		@param port The port to connect to.  If host is in the form <IP:PORT>,
		then the port parameter is ignored.
	*/
	virtual int connect(char *, int port=0) { assert(0); return port; }

	/** Connect the socket to a remote peer.
		@param host Hostname of the peer, either a DNS name or IP address.
		@param service The name of a service that represents a port address,
		which can be passed to getportbyserv().
	*/
	inline int connect(char *host, char *service) { 
		return connect(host,getportbyserv(service)); 
	}



	//	Socket services
	//

	int assign(SOCKET =INVALID_SOCKET);
#if defined(WIN32) && defined(_WINSOCK2API_)
	int assign(LPWSAPROTOCOL_INFO);		// to inherit sockets from other processes
#endif
	int bind(int =0);
    int setsockopt(SOCKET, int, const char*, int); 
	inline int bind(char *s) { return bind(getportbyserv(s)); }
	int close();

	// if any operation takes more than sec seconds, timeout
	// call timeout(0) to set blocking mode (default)
	// returns previous timeout
	int timeout(int sec);


	/*
	**	Stream protocol
	*/

	virtual ~Sock() {}


//	PRIVATE INTERFACE TO ALL SOCKS
//
protected:

	/*
	**	Type definitions
	*/

	enum sock_state { sock_virgin, sock_assigned, sock_bound, sock_connect,
						sock_writemsg, sock_readmsg, sock_special };


	/*
	**	Methods
	*/

	Sock();

	int getportbyserv(char *);
	int do_connect(char *, int);
	inline SOCKET get_socket (void) { return _sock; }
	char * do_serialize(char *);
	inline char * do_serialize() { return(do_serialize(NULL)); };
#ifdef WIN32
	int set_inheritable( int flag );
#else
	// On unix, sockets are always inheritable
	inline int set_inheritable( int ) { return TRUE; }
#endif

	bool test_connection();

	/*
	**	Data structures
	*/

	SOCKET			_sock;
	sock_state		_state;
	int				_timeout;
	struct sockaddr_in _who;	// endpoint of "connection"
};



#endif
