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

 

#ifndef AUTH_SOCK_H
#define AUTH_SOCK_H

#include "buffers.h"
#include "sock.h"
#include "reli_sock.h"

#include "globus_gss_assist.h"


class AuthSock;


typedef struct {
	AuthSock *sock;
	void *buffer;
	int size;
} AuthComms;


class AuthSock : public ReliSock {

//	PUBLIC INTERFACE TO AUTHENTICATED SOCKETS
//
public:

	friend class DaemonCore;

	/*
	**	Methods
	*/

	AuthSock();					/* virgin AuthSock		*/
	AuthSock(int);				/* listen on port		*/
	AuthSock(char *);			/* listen on serv 		*/
	AuthSock(char *, int, int timeout_val=0);		/* connect to host/port	*/
	AuthSock(char *, char *, int timeout_val=0);	/* connect to host/serv	*/
	~AuthSock(); 

	virtual int connect(char *, int);
	int accept(AuthSock &);
	AuthSock *accept();

	enum AuthState { auth_none=0, auth_cert=1, auth_client=2, auth_server=4 };
	void Set_conn_type( AuthState s ) { conn_type = s; };
	void Set_conn_auth_state( AuthState s ) { conn_auth_state = s; };
	int authenticate();
	int isAuthenticated() { return ( conn_auth_state == auth_cert ); };
	int authenticate_user();

private:
	static gss_cred_id_t credential_handle;
	AuthState conn_auth_state;
	AuthState conn_type;

	char *gateKeeper;
	AuthComms authComms;

	int lookup_user( char *client_name );
	int auth_connection_client();
	int auth_connection_server( AuthSock & );
};

#endif
