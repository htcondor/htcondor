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

 


#ifndef RELI_SOCK_H
#define RELI_SOCK_H

#include "buffers.h"
#include "sock.h"
//#include "authentication.h"
#include "condor_adtypes.h"

/*
**	R E L I A B L E    S O C K
*/

class Authentication;

class ReliSock : public Sock {
	friend class Authentication;

//	PUBLIC INTERFACE TO RELIABLE SOCKS
//
public:

	friend class DaemonCore;

	/*
	**	Methods
	*/

	// Virtual socket services
	//
	virtual int handle_incoming_packet();
	virtual int end_of_message();
	virtual int connect(char *, int);


	// Reliable socket services
	//

	ReliSock();             /* virgin ReliSock */
	ReliSock(int);				/* listen on port		*/
	ReliSock(char *);			/* listen on serv 		*/
	ReliSock(char *, int, int timeout_val=0);		/* connect to host/port	*/
	ReliSock(char *, char *, int timeout_val=0);	/* connect to host/serv	*/
	~ReliSock();

	int listen();
	inline int listen(int p) { if (!bind(p)) return FALSE; return listen(); }
	inline int listen(char *s) { if (!bind(s)) return FALSE; return listen(); }

	ReliSock *accept();
	int accept(ReliSock &);
	int accept(ReliSock *);

	int put_bytes_nobuffer(char *buf, int length, int send_size=1);
	int get_bytes_nobuffer(char *buffer, int max_length,
							 int receive_size=1);
	int get_file(const char *destination);
	int put_file(const char *source);

	int get_port();
	struct sockaddr_in *endpoint();

	int get_file_desc();

#ifndef WIN32
	// interface no longer supported 
	int attach_to_file_desc(int);
#endif

	/*
	**	Stream protocol
	*/

//	virtual stream_type type() { return Stream::reli_sock; }
	virtual stream_type type();

	//	byte operations
	//
	virtual int put_bytes(const void *, int);
	virtual int get_bytes(void *, int);
	virtual int get_ptr(void *&, char);
	virtual int peek(char &);

   int authenticate();
   int isAuthenticated();
	void unAuthenticate();
   char *getOwner();
   int setOwner( char *owner );
   int getOwnerUid();
   int setOwnerUid( int ownerUid );
	void setGenericAuthentication();
	void canTryGSS();
	void canTryFilesystem();

	int isClient() { return is_client; };


//	PROTECTED INTERFACE TO RELIABLE SOCKS
//
protected:
	/*
	**	Types
	*/

	enum relisock_state { relisock_listen };

	/*
	**	Methods
	*/
	virtual char * serialize(char *);
	inline char * serialize() { return(serialize(NULL)); }
	int prepare_for_nobuffering( stream_coding = stream_unknown);

	/*
	**	Data structures
	*/

	class RcvMsg {
	public:
		RcvMsg() : ready(0) {}
		int rcv_packet(SOCKET, int);

		ChainBuf	buf;
		int			ready;
	} rcv_msg;

	class SndMsg {
	public:
		int snd_packet(int, int, int);

		Buf			buf;
	} snd_msg;

	relisock_state	_special_state;
	int	ignore_next_encode_eom;
	int	ignore_next_decode_eom;

	Authentication * authob;
	int is_client;
	char *hostAddr;
	int canUseFlags;
};

#endif
