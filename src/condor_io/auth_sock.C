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

 



#define _POSIX_SOURCE

#include "condor_common.h"
#include "condor_constants.h"
#include "condor_adtypes.h"
#include "condor_io.h"
#include "condor_debug.h"
#include "internet.h"


#if !defined(GSS_AUTHENTICATION)
#define AuthSock ReliSock
#else

//include rest of file in this #ELSE
#include "globus_gss_assist.h"
#include "auth_sock.h"


/*******************************************************************/
gss_cred_id_t AuthSock::credential_handle = GSS_C_NO_CREDENTIAL;

/*******************************************************************/

int 
AuthSock::lookup_user( char *client_name ) { 
	/* return -1 for error */
	char filename[MAXPATHLEN];
	char command[MAXPATHLEN+32];

	sprintf( filename, "%s/index.txt", getenv( "X509_CERT_DIR" ) );

	FILE *index;
	if ( !(index = fopen(  filename, "r" ) ) ) {
		dprintf( D_ALWAYS, "unable to open index file %s, errno %d\n", 
				filename, errno );
		return -1;
	}

	//find entry
	int found = 0;
	char line[81];
	while ( !found && fgets( line, 80, index ) ) {
		//Valid user entries have 'V' as first byte in their cert db entry
		if ( line[0] == 'V' &&  strstr( line, client_name ) ) {
			found = 1;
		}
	}
	fclose( index );
	if ( !found ) {
		dprintf( D_ALWAYS, "unable to find V entry for %s in %s\n", 
				filename, client_name );
		return( -1 );
	}

	dprintf( D_ALWAYS,"GSS authenticated %s\n", client_name );
	return 0;
}

/*******************************************************************/
//cannot make this an AuthSock method, since gss_assist method expects
//three parms, methods have hidden "this" parm first. Couldn't figure out
//a way around this, so made AuthSock have a member of type AuthComms
//to pass in to this method to manage buffer space.  //mju
int 
authsock_get(void *arg, void **bufp, size_t *sizep)
{
	/* globus code which calls this function expects 0/-1 return vals */

	//authsock must "hold onto" GSS state, pass in struct with comms stuff
	AuthComms *comms = (AuthComms *) arg;
	AuthSock *sock = comms->sock;
	size_t stat;

	sock->decode();

	//read size of data to read
	stat = sock->code( *((int *)sizep) );

	*bufp = malloc( *((int *)sizep) );
	if ( !*bufp ) {
		dprintf( D_ALWAYS, "malloc failure authsock_get\n" );
		stat = FALSE;
	}

	//if successfully read size and malloced, read data
	if ( stat )
		sock->code_bytes( *bufp, *((int *)sizep) );

	sock->end_of_message();

	//check to ensure comms were successful
	if ( !stat ) {
		dprintf( D_ALWAYS, "authsock_get (read from socket) failure\n" );
		return -1;
	}
	return 0;
}

/*******************************************************************/
int 
authsock_put(void *arg,  void *buf, size_t size)
{
	//param is just a AS*
	AuthSock *sock = (AuthSock *) arg;
	int stat;

	sock->encode();

	//send size of data to send
	stat = sock->code( (int &)size );
	//if successful, send the data
	if ( stat ) {
		if ( !(stat = sock->code_bytes( buf, ((int) size )) ) ) {
			dprintf( D_ALWAYS, "failure sending data (%d bytes) over sock\n",size);
		}
	}
	else {
		dprintf( D_ALWAYS, "failure sending size (%d) over sock\n", size );
	}

	sock->end_of_message();

	//ensure data send was successful
	if ( !stat ) {
		dprintf( D_ALWAYS, "authsock_put (write to socket) failure\n" );
		return -1;
	}
	return 0;
}

/*******************************************************************/
int 
AuthSock::authenticate_user()
{
	OM_uint32 major_status;
	OM_uint32 minor_status;

	if ( credential_handle != GSS_C_NO_CREDENTIAL ) { // user already auth'd 
		dprintf( D_FULLDEBUG, "This process has a valid certificate & key\n" );
		return TRUE;
	}

	// ensure all env vars are in place, acquire cred will fail otherwise 
	if ( !( getenv( "X509_USER_CERT" ) && getenv( "X509_USER_KEY" ) 
			&& getenv( "X509_CERT_DIR" ) ) ) 
	{
		//don't log error, since this can be called before env vars are set!
		dprintf(D_FULLDEBUG,"X509 env vars not set yet (might not be error)\n");
		return FALSE;
	}

	//use gss-assist to verify user (not connection)
	//this method will prompt for password if private key is encrypted!
	int time = timeout(60 * 5);  //allow user 5 min to type passwd

	major_status = globus_gss_assist_acquire_cred(&minor_status,
		GSS_C_BOTH, &credential_handle);

	timeout(time); //put it back to what it was before

	if (major_status != GSS_S_COMPLETE)
	{
		dprintf( D_ALWAYS, "gss-api failure initializing user credentials, "
				"stats: 0x%x\n", major_status );
		credential_handle = GSS_C_NO_CREDENTIAL; 
		return FALSE;
	}

	dprintf( D_FULLDEBUG, "This process has a valid certificate & key\n" );
	return TRUE;
}

/*******************************************************************/
int 
AuthSock::auth_connection_client()
{
	OM_uint32						  major_status = 0;
	OM_uint32						  minor_status = 0;
	int								  token_status = 0;
	OM_uint32						  ret_flags = 0;
	gss_ctx_id_t					  context_handle = GSS_C_NO_CONTEXT;

	if ( !authenticate_user() ) {
		dprintf( D_ALWAYS, 
			"failure authenticating client from auth_connection_client\n" );
		return FALSE;
	}
	 
	gateKeeper = getenv( "CONDOR_GATEKEEPER" );

	if ( !gateKeeper ) {
		dprintf( D_ALWAYS, "env var CONDOR_GATEKEEPER not set" );
		return FALSE;
	}

	authComms.sock = this;
	authComms.buffer = NULL;
	authComms.size = 0;

	major_status = globus_gss_assist_init_sec_context(&minor_status,
		  credential_handle, &context_handle,
		  gateKeeper,
		  GSS_C_DELEG_FLAG|GSS_C_MUTUAL_FLAG,
		  &ret_flags, &token_status,
		  authsock_get, (void *) &authComms,
		  authsock_put, (void *) this 
	);

	if (major_status != GSS_S_COMPLETE)
	{
		dprintf( D_ALWAYS, "failed auth connection client, gss status: 0x%x\n",
								major_status );
		return FALSE;
	}

	/* 
	 * once connection is authenticated, don't need sec_context any more
	 * ???might need sec_context for PROXIES???
	 */
	gss_delete_sec_context( &minor_status, &context_handle, GSS_C_NO_BUFFER );

	conn_auth_state = auth_cert;

	dprintf(D_FULLDEBUG,"valid GSS connection established to %s\n", gateKeeper);
	return TRUE;
}

/*******************************************************************/
int 
AuthSock::auth_connection_server( AuthSock &authsock)
{
	int rc;
	OM_uint32 major_status = 0;
	OM_uint32 minor_status = 0;
	int		 token_status = 0;
	OM_uint32 ret_flags = 0;
	gss_ctx_id_t context_handle = GSS_C_NO_CONTEXT;

	if ( !authenticate_user() ) {
		dprintf( D_ALWAYS, 
			"failure authenticating self in auth_connection_server\n" );
		return FALSE;
	}
	if ( authsock.GSSClientname ) 
		free ( GSSClientname );
	authsock.GSSClientname = NULL;
	 
	authComms.sock = &authsock;
	authComms.buffer = NULL;
	authComms.size = 0;

	major_status = globus_gss_assist_accept_sec_context(&minor_status,
				 &context_handle, credential_handle,
				 &authsock.GSSClientname,
				 &ret_flags, NULL, /* don't need user_to_user */
				 &token_status,
				 authsock_get, (void *) &authComms,
				 authsock_put, (void *) &authsock
	);


	if ( (major_status != GSS_S_COMPLETE) ||
			( ( lookup_user( authsock.GSSClientname ) ) < 0 ) ) 
	{
		if (major_status != GSS_S_COMPLETE) {
			dprintf(D_ALWAYS, "server: GSS failure authenticating %s 0x%x\n",
					"client, status: ", major_status );
		}
		else
			dprintf( D_ALWAYS, "server: user lookup failure.\n" );
		if ( authsock.GSSClientname ) {
			free( authsock.GSSClientname );
			authsock.GSSClientname = NULL;
		}
		return FALSE;
	}

	/* 
	 * once connection is authenticated, don't need sec_context any more
	 * ???might need sec_context for PROXIES???
	 */
	gss_delete_sec_context( &minor_status, &context_handle, GSS_C_NO_BUFFER );

	authsock.Set_conn_auth_state( auth_cert );

	dprintf(D_FULLDEBUG,"valid GSS connection established to %s\n", 
			authsock.GSSClientname);
	return TRUE;
}

/*******************************************************************/
AuthSock::AuthSock():ReliSock() {
	conn_type = auth_none;
	auth_setup();
}

/*******************************************************************/
AuthSock::AuthSock(					/* listen on port		*/
	int		port
	)
	: ReliSock(port)
{
	conn_type = auth_server;
	auth_setup();
}

/*******************************************************************/
AuthSock::AuthSock(					/* listen on serv		*/
	char	*serv
	)
	: ReliSock(serv)
{
	conn_type = auth_server;
	auth_setup();
}

/*******************************************************************/
AuthSock::AuthSock(
	char	*host,
	int		port,
	int		timeout_val
	)
	: ReliSock(host,port,timeout_val) /* does connect */
{
	conn_type = auth_client;
	auth_setup();
}

/*******************************************************************/
AuthSock::AuthSock(
	char	*host,
	char	*serv,
	int		timeout_val
	)
	: ReliSock(host,serv,timeout_val) /* does connect */
{
	conn_type = auth_client;
	auth_setup();
}

/*******************************************************************/
AuthSock::~AuthSock()
{
	if ( GSSClientname ) free ( GSSClientname );
	close();
}

/*******************************************************************/
int
AuthSock::accept(
	AuthSock &sock
	)
{
	sock.Set_conn_type( auth_server );
	return ReliSock::accept( (ReliSock&) sock );
}

/*******************************************************************/
AuthSock *AuthSock::accept()
{
	AuthSock *c_rs;
	int c_sock;

	conn_type = auth_server;

	if (!(c_rs = new AuthSock())) return (AuthSock *)0;

	if ((c_sock = accept(*c_rs)) < 0) {
		delete c_rs;
		return (AuthSock *)0;
	}

	return c_rs;
}

/*******************************************************************/
int AuthSock::connect(
	char	*host,
	int		port
	)
{
	conn_type = auth_client;
	return ReliSock::connect(host, port);
}

/*******************************************************************/
ClassAd * 
AuthSock::authenticate() {
	int status = 1;
	//don't just return TRUE if isAuthenticated() == TRUE, since 
	//we should BALANCE calls of authenticate() on client/server side
	//just like end_of_message() calls must balance!

	if ( !authenticate_user() ) {
		dprintf( D_ALWAYS, "authenticate: user creds not established\n" );
		status = 0;
	}

	if ( status ) {
		//temporarily change timeout to 5 minutes so the user can type passwd
		//MUST do this even on server side, since client side might call
		//authenticate_user() while server is waiting on authenticate!!
		int time = timeout(60 * 5); 

		switch ( conn_type ) {
			case auth_server : 
				dprintf(D_FULLDEBUG,"about to authenticate client from server\n" );
				status = auth_connection_server( *this );
				break;
			case auth_client : 
				dprintf(D_FULLDEBUG,"about to authenticate server from client\n" );
				status = auth_connection_client();
				break;
			default : 
				dprintf(D_ALWAYS,"authenticate:should have connected/accepted\n");
		}
		timeout(time); //put it back to what it was before
	}

	ClassAd *ad = new ClassAd();
	char tmp[128];

	if ( status ) {
		sprintf( tmp, "%s = %s", USERAUTH_ADTYPE, "GSS-SSL" );
	}
	else {
		sprintf( tmp, "%s = %s", USERAUTH_ADTYPE, "none" );
	}
	ad->Insert( tmp );
	return( ad );
}

/*******************************************************************/
void AuthSock::auth_setup() {
	authComms.buffer = NULL;
	authComms.size = 0;
	GSSClientname = NULL;
}

#endif //#!defined GSS_AUTHENTICATION
