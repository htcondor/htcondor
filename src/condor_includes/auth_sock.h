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

/** Communications structure used with AuthSock's send/receive methods.
    Use of GSS-API requires us to supply our own send/received methods
    because we are sending/receiving over an existing ReliSock.
 */
typedef struct {
	AuthSock *sock;
	void *buffer;
	int size;
} AuthComms;


/** AuthSock is an authenticated subclass of Cedar's ReliSock class.
    An AuthSock is NOT authenticated until the successful return of the
    AuthSock::authenticate() method.
    AuthSock subclasses Cedar's ReliSock socket communications methods
    to provide authentication services over Cedar's modified sockets.
    It uses Globus' Generic Security Service-API implementation based
    on RFC 2078. Globus (and therefore, Condor) uses SSL-eay, an 
    implementation of Secure Sockets Layer, for X.509 certificate 
    generation, administration, and verification.

    To use AuthSock, you must define these the following environment 
    variables (pseudo-code samples are shown):
	 	CONDOR_GATEKEEPER=/CN=schedd@hostname.cs.wisc.edu (CLIENT ONLY)
    	X509_CERT_DIR=$HOME/CertClient
    	X509_USER_CERT=$HOME/CertClient/newcert.pem
    	X509_USER_KEY=$HOME/CertClient/private/newreq.pem
    
    AuthSock provides the same interface as ReliSock, with the addition
    of two methods: authenticate_user( void ) and authenticate( void ),
    so any or all instances of ReliSock can be replaced with AuthSock.
    In fact, on platforms where GSS-API and SSL-eay are not available,
    AuthSock is simply defined as ReliSock in a #define statement.
 */
class AuthSock : public ReliSock 
{
public:
	friend class DaemonCore;

	/*
	**	Methods
	*/


	/// Default constructor, creates bare AuthSock.
	AuthSock();					/* virgin AuthSock */

	/** Server side AuthSock, performs socket, bind and listen calls on port.
	    @param port: server IP port number */
	AuthSock(int port);

	/** Server side AuthSock, performs socket, bind & listen calls, for service.
	    @param serv: server service name (as in /etc/services) */
	AuthSock(char *serv);

	/** Client side AuthSock, performs socket and connect to server by port.
	    @param host: form of "host.org.domain" OR "<x.x.x.x:x>" (sinful string)
	    @param port: server port number (ignored if host is sinful string)
	    @param timeout_val: wait n seconds for input, else fail (0 no timeout)
	 */
	AuthSock(char *host, int port, int timeout_val=0);

	/** Client side AuthSock, performs socket and connect to server by service.
	    @param host: form of "host.org.domain" OR "<x.x.x.x:x>" (sinful string)
	    @param serv: server service name (ignored if host is sinful string)
	    @param timeout_val: wait n seconds for input, else fail (0 no timeout)
	 */
	AuthSock(char *host, char *serv, int timeout_val=0);	

	/// AuthSock destructor.
	~AuthSock(); 

	/** Connect is inherited from ReliSock for clients who haven't yet 
	    connected to the server, e.g. called AuthSock() for a bare socket 
	    @param host: form of "host.org.domain" OR "<x.x.x.x:x>" (sinful string)
	    @param port: server port number (ignored if host is sinful string)
	    @return: TRUE on success else FALSE
	 */
	virtual int connect(char *host, int port);

	/** Helper function for AuthSock *accept(), does setup and calls 
                                       ReliSock::accept(ReliSock &). 
	    @param sock: current AuthSock, copied to a new accepted socket connection
	    @return: TRUE on success else FALSE
	 */
	int accept(AuthSock &sock);

	/// Returns a new AuthSock with an ::accept()'ed connection.
	AuthSock *accept();

	/** Perform synchronous GSS authentication over current AuthSock.
	    Must be balanced by a call to authenticate on the other side 
       of the connection. 
	    @return TRUE on success else FALSE
	 */
	int authenticate();

	/** Check X509 certificate and possibly prompt for passphrase.
	    Passphrase is needed for certificates of submittors to Condor. 
	    @return TRUE on success else FALSE
	 */
	int authenticate_user();

	/** Check to see if the connection is authenticated.
	    @return TRUE on success else FALSE
	 */
	int isAuthenticated() { return ( conn_auth_state == auth_cert ); };

	/// Maintain communications state between send/receive calls by GSS-API.
	AuthComms authComms;

	/** Name of client extracted from handshake during authentication.
	    Client gets this value back during handshake.
	 */
	char *GSSClientname;

protected:
	/// States to track status/authentication level of current AuthSock.
	enum AuthState { auth_none=0, auth_cert=1, auth_client=2, auth_server=4 };

	/** Helper method to keep track whether it's client or server side.
	    @param sock: AuthSock to set connection type for.
	 */
	void Set_conn_type( AuthState sock ) { conn_type = sock; };

	/** Helper method to update the state of the AuthSock.
	    @param sock: AuthSock to set connection state for.
	 */
	void Set_conn_auth_state( AuthState sock ) { conn_auth_state = sock; };

private:

	/// Personal credentials, set by authenticate_user(), used by authenticate().
	static gss_cred_id_t credential_handle;

	/// Current state of connection
	AuthState conn_auth_state;

	/// Whether connection is client side, server side, or unknown (virgin sock).
	AuthState conn_type;

	/// Name that client requires server will present in its certificate.
	char *gateKeeper;


	/** Look up username from database of local condor users (currently NO-OP).
	    @param client_name: name to lookup as appears in certificate
	 */
	int lookup_user( char *client_name );

	/// Perform correct "side" of authentication (client initiates).
	int auth_connection_client();

	/** Perform correct "side" of authentication (server is passive).
	    @param authsock: new AuthSock (from accept() ) to authenticate
	 */
	int auth_connection_server( AuthSock &authsock );

	/// Perform miscellaneous setup needed in all constructors.
	void auth_setup();
};

#endif
