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

 


#ifndef AUTHENTICATION_H
#define AUTHENTICATION_H

#if defined(GSS_AUTHENTICATION)
#include "globus_gss_assist.h"
#endif

#include "reli_sock.h"

#define MAX_USERNAMELEN 128

/**
    To use GSS ReliSock, you must define the following environment
	 variable: X509_CERT_DIR. This is read and other necessary vars
	 are set in member methods.
 */


/** Communications structure used with GSS's send/receive methods.
    Use of GSS-API requires us to supply our own send/received methods
    because we are sending/receiving over an existing ReliSock.
 */

class GSSComms {
public:
   ReliSock *sock;
   void *buffer;
   int size;
};

class Authentication {
	
friend class ReliSock;

	/// States to track status/authentication level
   enum authentication_state { 
		CAUTH_NONE=0, 
		CAUTH_ANY=1,
		CAUTH_GSS=2, 
		CAUTH_FILESYSTEM=4, 
		CAUTH_NT=8 
		//, 16, 32, etc.
   };

	Authentication( ReliSock *sock );
	~Authentication();
	int authenticate( char *hostAddr );
	int isAuthenticated();
	void unAuthenticate();

	void setAuthType( authentication_state state );
	int setOwner( char *owner );
	char *getOwner();
	int setOwnerUid( int uid );
	int getOwnerUid();

private:
	Authentication() {}; //should never be called, make private to help that!
	int handshake();
#if defined(WIN32)
	int authenticate_nt();
#else
	int authenticate_filesystem();
#endif
	int selectAuthenticationType( int clientCanUse );
	int setupEnv( char *hostAddr );

#if defined (GSS_AUTHENTICATION)
	int authenticate_gss();
	int authenticate_self_gss();
	int authenticate_client_gss();
	int authenticate_server_gss();
	int lookup_user_gss( char *username );
	int nameGssToLocal();

	/// Personal credentials
	static gss_cred_id_t credential_handle;
#endif

	/// Track accomplished authentication state.
	authentication_state auth_status;

	/// Track which methods to try (none, filesystem, gss, etc.)
//	int canUseFlags;

	ReliSock *mySock;
	GSSComms authComms;
	char *GSSClientname;
	char *claimToBe;
	int ownerUid;

};
#endif
