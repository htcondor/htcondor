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

const char STR_X509_USER_PROXY[]       = "X509_USER_PROXY";
const char STR_X509_DIRECTORY[]        = "X509_DIRECTORY";
const char STR_X509_CERT_DIR[]         = "X509_CERT_DIR";
const char STR_X509_USER_CERT[]        = "X509_USER_CERT";
const char STR_X509_USER_KEY[]         = "X509_USER_KEY";
const char STR_SSLEAY_CONF[]           = "SSLEAY_CONF";

#include "globus_gss_assist.h"
#include "sslutils.h"
#include "credential.h"
#include "condor_credential.h"

#endif

#if defined(KERBEROS_AUTHENTICATION)
extern "C" {
#include "krb5.h"
#include "com_err.h"            // error_message is defined here
}
#endif

#ifdef WIN32
#define	SECURITY_WIN32 1
#include "sspi.NT.h"
#endif

#include "reli_sock.h"

#define MAX_USERNAMELEN 128

/**
    To use GSS ReliSock, you must define the following environment
	 variable: X509_DIRECTORY. This is read and other necessary vars
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


enum transfer_mode {
  NORMAL = 1,
  ENCRYPT,
  ENCRYPT_HDR
};

class Authentication {
	
friend class ReliSock;

public:
	/// States to track status/authentication level
   enum authentication_state { 
		CAUTH_NONE=0, 
		CAUTH_ANY=1,
		CAUTH_CLAIMTOBE=2,
		CAUTH_FILESYSTEM=4, 
		CAUTH_NTSSPI=8,
		CAUTH_GSS=16,
		CAUTH_FILESYSTEM_REMOTE=32,
		CAUTH_KERBEROS=64
		//, 32, 64, etc.
   };

	Authentication( ReliSock *sock );
	~Authentication();
	int authenticate( char *hostAddr );
	int isAuthenticated();
	void unAuthenticate();

	void setAuthAny();
	int setOwner( const char *owner );
	const char *getOwner();
	const char *getDomain();

	bool is_valid();
	//------------------------------------------
	// PURPOSE: Whether the security context supports
	//          encryption or not?
	// REQUIRE: NONE
	// RETURNS: TRUE or FALSE
	//------------------------------------------

	int encrypt(bool);
	//------------------------------------------
	// PURPOSE: Turn encryption mode on or off
	// REQUIRE: bool flag
	// RETURNS: TRUE or FALSE
	//------------------------------------------
	
	bool is_encrypt();
	//------------------------------------------
	// PURPOSE: Return encryption mode
	// REQUIRE: NONE
	// RETURNS: TRUE -- encryption is on
	//          FALSE -- otherwise
	//------------------------------------------
	
	int hdr_encrypt();
	//------------------------------------------
	// PURPOSE: set header encryption?
	// REQUIRE:
	// RETURNS:
	//------------------------------------------
	
	bool is_hdr_encrypt();
	//------------------------------------------
	// PURPOSE: Return status of hdr encryption
	// REQUIRE:
	// RETURNS: TRUE -- header encryption is on
	//          FALSE -- otherwise
	//------------------------------------------
	
	int wrap(char* input, int input_len, char*& output,int& output_len);
	//------------------------------------------
	// PURPOSE: Wrap the buffer
	// REQUIRE: 
	// RETUNRS: TRUE -- success, FALSE -- falure
	//          May need more code later on
	//------------------------------------------
	
	int unwrap(char* input, int input_len, char*& output, int& output_len);
	//------------------------------------------
	// PURPOSE: Unwrap the buffer
	// REQUIRE: 
	// RETURNS: TRUE -- success, FALSE -- failure
	//------------------------------------------

	const char * getRemoteAddress();
	//----------------------------------------
	// PURPOSE: Get remote address 
	// REQUIRE: None
	// RETURNS: IP address of the remote machine
	//------------------------------------------

	void setRemoteAddress(char * address);
	//------------------------------------------
	// PURPOSE: Get romote host address
	//          If the connection is established
	//          via kerberos, this address
	//          is obtained from kerberos' auth_context
	//          For all other connections, we simply
	//          return sock's end point
	// REQUIRE: authenticate has been called
	// RETURNS: Remote machine's address
	//------------------------------------------

private:
#if !defined(SKIP_AUTHENTICATION)
	Authentication() {}; //should never be called, make private to help that!
	int handshake();

	void setAuthType( authentication_state state );
	int authenticate_claimtobe();

	void initialize(void);

#if defined(WIN32)
	static PSecurityFunctionTable pf;
	int sspi_client_auth(CredHandle& cred,CtxtHandle& cliCtx, 
		const char *tokenSource);
	int sspi_server_auth(CredHandle& cred,CtxtHandle& srvCtx);
	int authenticate_nt();
#else
	int authenticate_filesystem( int remote = 0 );
#endif
	int selectAuthenticationType( int clientCanUse );
	void setupEnv( char *hostAddr );

#if defined(GSS_AUTHENTICATION)
	
	Authentication(gss_cred_id_t,gss_ctx_id_t);
	int authenticate_gss();
	int authenticate_self_gss();
	int authenticate_client_gss();
	int authenticate_server_gss();
	int lookup_user_gss( char *username );
	int nameGssToLocal();
	int get_user_x509name(char*,char*);

	/** Check whether the security context of the scoket is valid or not 
	@return TRUE if valid FALSE if not */
	
	bool gss_is_valid();

	/** encrypts a given buffer 
	    @return GSS status of the cooresponding wrap call*/
	
	OM_uint32 gss_wrap_buffer(char*,int,char*& ,int&);
	
	/** decrypts a given GSS buffer .
	  @return GSS status of the correspoding unwrap call */
	
	OM_uint32 gss_unwrap_buffer(char*,int,char*&, int&);


   /** A specialized function that is needed for secure personal
       condor. When schedd and the user are running under the
       same userid we would still want the authentication process
       be transperant. This means that we have to maintain to
       distinct environments to processes running under the same
       id. erase\_env ensures that the X509\_USER\_PROXY which
       should \bf{NOT} be set for daemon processes is wiped out
       even if it is set, so that a daemon process does not start
       looking for a proxy which it does not need. */
	
 	void   erase_env();
	
	void  print_log(OM_uint32 ,OM_uint32 ,int , char*);

#endif

#if defined (KERBEROS_AUTHENTICATION)
	int authenticate_kerberos();
	//------------------------------------------
	// PURPOSE : Performing authentication using Kerberos
	// REQUIRE : None
	// RETURNS : TRUE -- success; FALSE failure
	//------------------------------------------
	
	int daemon_acquire_credential();
	//------------------------------------------
	// PURPOSE : Acquire service credential on 
	//           behalf of the user
	// REQUIRE : NONE
	// RETURNS: TRUE -- if success; FALSE -- if failure
	//------------------------------------------
	int client_acquire_credential();
	//------------------------------------------
	// PURPOSE : Acquire a credential for client
	// REQUIRE : None
	// RETURNS : TRUE -- success; FALSE failure
	//------------------------------------------

	int server_acquire_credential();
	//------------------------------------------
	// PURPOSE: Acquire a credential for the daemon process
	// REQUIRE: None (probably root access)
	// RETURNS: TRUE -- success; FALSE -- failure
	//------------------------------------------
	
	int authenticate_client_kerberos();
	//------------------------------------------
	// PURPOSE : Client's authentication method
	// REQUIRE : None
	// RETURNS : TRUE -- success; FALSE failure
	//------------------------------------------

	int authenticate_server_kerberos();
	//------------------------------------------
	// PURPOSE : Server's authentication method
	// REQUIRE : None
	// RETURNS : TRUE -- success; FALSE failure
	//------------------------------------------

	int init_kerberos_context();
	//------------------------------------------
	// PURPOSE: Initialize context
	// REQUIRE: NONE
	// RETURNS: TRUE -- success; FALSE -- failure
	//------------------------------------------

	int map_kerberos_name(krb5_ticket * ticket);
	//------------------------------------------
	// PURPOSE: Map kerberos realm to condor uid
	// REQUIRE: A valid kerberos principal
	// RETURNS: TRUE --success; FALSE -- failure
	//------------------------------------------

	int get_server_info();
	//------------------------------------------
	// PURPOSE: Get server info
	// REQUIRE: NONE
	// RETURNS: true -- success; false -- failure
	//------------------------------------------

	int send_request(krb5_data * request);
	//------------------------------------------
	// PURPOSE: Send an authentication request 
	// REQUIRE: None
	// RETURNS: KERBEROS_GRANT -- granted
	//          KERBEROS_DENY  -- denied
	//------------------------------------------
	
	void initialize_client_data();
	//------------------------------------------
	// PURPOSE: intialize default cache name and 
	// REQUIRE: NONE
	// RETURNS: NONE
	//------------------------------------------

	int forward_tgt_creds(krb5_auth_context auth_context, 
			      krb5_creds      * creds,
			      krb5_ccache       ccache);
	//------------------------------------------
	// PURPOSE: Forwarding tgt to another process
	// REQUIRE: krb5_creds
	// RETURNS: 1 -- unable to forward
	//          0 -- success
	//------------------------------------------
	int receive_tgt_creds(krb5_auth_context auth_context, 
			      krb5_ticket     * ticket);
	//------------------------------------------
	// PURPOSE: Receive tgt from another process
	// REQUIRE: ticket, context
	// RETURNS: 1 -- unable to receive tgt
	//          0 -- success
	//------------------------------------------

	int read_request(krb5_data * request);
	//------------------------------------------
	// PURPOSE: Read a request
	// REQUIRE: Pointer to a krb5_data object
	// RETURNS: fields in krb5_data will be filled
	//------------------------------------------

	int client_mutual_authenticate(krb5_auth_context * auth);
	//------------------------------------------
	// PURPOSE: Mututal authenticate the server
	// REQUIRE: krb5_auth_context
	// RETURNS: Server's response code
	//------------------------------------------

	void getRemoteHost(krb5_auth_context auth_context);
	//------------------------------------------
	// PURPOSE: Get remote host name 
	// REQUIRE: successful kereros authentication
	// RETURNS: NONE, set remotehost_
	//------------------------------------------

	bool kerberos_is_valid();
	//------------------------------------------
	// PURPOSE: Whether the security context supports
	//          encryption or not?
	// REQUIRE: NONE
	// RETURNS: TRUE or FALSE
	//------------------------------------------

	int kerberos_wrap(char*  input, 
			  int    input_len, 
			  char*& output,
			  int&   output_len);
	//------------------------------------------
	// PURPOSE: Wrap the buffer
	// REQUIRE: 
	// RETUNRS: TRUE -- success, FALSE -- falure
	//          May need more code later on
	//------------------------------------------
	
	int kerberos_unwrap(char*  input, 
			    int    input_len, 
			    char*& output, 
			    int&   output_len);
	//------------------------------------------
	// PURPOSE: Unwrap the buffer
	// REQUIRE: 
	// RETURNS: TRUE -- success, FALSE -- failure
	//------------------------------------------
#endif

#endif /* !SKIP_AUTHENTICATION */
	
#if defined (GSS_AUTHENTICATION)
	/// Personal credentials
	gss_cred_id_t credential_handle;
	gss_ctx_id_t     context_handle ;
	X509_Credential* my_credential;
	OM_uint32	 ret_flags ;
#endif

	//------------------------------------------
	// Kerberos related data
	//------------------------------------------
#if defined(KERBEROS_AUTHENTICATION)
	krb5_context       krb_context_;      // context_
	krb5_principal     krb_principal_;    // principal information
	krb5_principal     server_;           // server
	char *             ccname_;           // FILE:/krbcc_name
	char *             defaultCondor_;    // Default condor 
	char *             defaultStash_;     // Default stash location
	krb5_keyblock *    sessionKey_;       // Session key
	char *             keytabName_;       // keytab to use
	//  key?
#endif


	/// Track accomplished authentication state.
	authentication_state auth_status;
	char *serverShouldTry;

	ReliSock *mySock;
	GSSComms authComms;
	char * GSSClientname;
	char * claimToBe;
	char * domainName;
	int    canUseFlags;
	//int    isServer;
	char * RendezvousDirectory;
	int    token_status;
	transfer_mode t_mode;
	char * remotehost_;
	bool               isDaemon_;         // Whether it's a daemon/daemon

};

#endif /* AUTHENTICATION_H */

