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
#include "condor_classad.h"

#define MAX_USERNAMELEN 128

#if defined(GSS_AUTHENTICATION)

#include "globus_gss_assist.h"

/**
	 To use GSS ReliSock, you must define the following environment
	 variables (pseudo-code samples are shown):
	   CONDOR_GATEKEEPER=/CN=schedd@hostname.cs.wisc.edu (CLIENT ONLY)
	   X509_CERT_DIR=$HOME/CertClient
	   X509_USER_CERT=$HOME/CertClient/newcert.pem
	   X509_USER_KEY=$HOME/CertClient/private/newreq.pem
 */

#endif

/** Communications structure used with GSS's send/receive methods.
    Use of GSS-API requires us to supply our own send/received methods
    because we are sending/receiving over an existing ReliSock.
 */
class ReliSock;

//GSS CODE
class GSSComms {
public:
	ReliSock *sock;
	void *buffer;
	int size;
};


/*
**	R E L I A B L E    S O C K
*/

class ReliSock : public Sock {

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

	ReliSock(); /* virgin reli_sock */

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

	virtual stream_type type();

	//	byte operations
	//
	virtual int put_bytes(const void *, int);
	virtual int get_bytes(void *, int);
	virtual int get_ptr(void *&, char);
	virtual int peek(char &);

	
// AUTHENTICATION SPECIFIC STUFF, should really be in a subclass, but
// it wasn't my decision...

	/// States to track status/authentication level 
	enum authentication_state { CAUTH_NONE=0, CAUTH_ANY=1, CAUTH_GSS=2, 
		CAUTH_FILESYSTEM=4,
		//put other methods here as 4, 8, etc. to use as flags.
		CAUTH_CLIENT=1024, CAUTH_SERVER=1025
	};
	int authenticate();
	int isAuthenticated();
	char *getOwner();
	void setOwner( char *owner );
	int getOwnerUid();
	void setOwnerUid( int ownerUid );
	void setFile( char *file );
	char *getFile();
	void canTryFilesystem();
	void canTryGSS();
	void setState( authentication_state state ) { conn_type = state; };
	int getState() { return conn_type; };
//	ClassAd * getStateAd();
	void setAuthType( authentication_state auth_type );

private:
#if defined (GSS_AUTHENTICATION)
   /// Personal credentials, set by authenticate_user(), used by authenticate().
   static gss_cred_id_t credential_handle;
#endif

	/// Track client/server.
	authentication_state conn_type;

	/// Track accomplished authentication state.
	authentication_state auth_status;

	/// Track which methods to try (none, filesystem, gss, etc.)
	int canUseFlags;

	/** Name of client extracted from handshake during authentication.
	    Client gets this value back during handshake.
	 */
	char *GSSClientname;
//	ClassAd stateClassad;
	ClassAd * stateClassad;
	GSSComms authComms;
	char rendevousFile[_POSIX_PATH_MAX+1];
	char claimToBe[MAX_USERNAMELEN+1];
	int ownerUid;

	/// do initial common setup for all ReliSockets
	void auth_setup( authentication_state type );
	void set_conn_type( authentication_state sock_type );
	int handshake();
	int authenticate_self_gss();
	int authenticate_gss();
	int authenticate_filesystem();
	int lookup_user_gss( char *username );
	int authenticate_client_gss();
	int authenticate_server_gss();
	int selectAuthenticationType( int clientCanUse );
	int nameGssToLocal();

#if 0
	/** Helper method to update the state of the AuthSock.
	    @param sock: AuthSock to set connection state for.
	 */
	void Set_conn_auth_state( authentication_state sockstate ) { conn_auth_state = sock_state; };

#endif


//end of AUTHENTICATION stuff


//	PRIVATE INTERFACE TO RELIABLE SOCKS
//
protected:
	/*
	**	Types & Data structures
	*/

	enum relisock_state { relisock_listen };


	relisock_state	_special_state;
	int	ignore_next_encode_eom;
	int	ignore_next_decode_eom;


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

	/*
	** Members
	*/
	/*
	**	Methods
	*/
	virtual char * serialize(char *);
	inline char * serialize() { return(serialize(NULL)); }
	int prepare_for_nobuffering( stream_coding = stream_unknown);
};

#endif
