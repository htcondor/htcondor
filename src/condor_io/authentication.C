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

#include "condor_common.h"
#include "authentication.h"
#include "environ.h"

#if !defined(SKIP_AUTHENTICATION)
#   include "condor_debug.h"
#   include "condor_string.h"
#   include "condor_config.h"
#   include "internet.h"
#   include "condor_uid.h"
#   include "my_username.h"
#   include "string_list.h"
#endif /* !defined(SKIP_AUTHENTICATION) */

extern DLL_IMPORT_MAGIC char **environ;

const char STR_DEFAULT_CONDOR_USER[]    = "condor";    // Default condor user
const char STR_DEFAULT_CONDOR_SPOOL[]   = "SPOOL";
const char STR_DEFAULT_CACHE_DIR[]      = "/tmp";      // Not portable!!!
const char STR_CONDOR_CACHE_DIR[]       = "CONDOR_CACHE_DIR";

//----------------------------------------------------------------------
// Kerberos stuff
//----------------------------------------------------------------------
#if defined(KERBEROS_AUTHENTICATION)
const char STR_CONDOR_KERBEROS_CACHE[]  = "CONDOR_KERBEROS_CACHE";
const char STR_CONDOR_CACHE_SHARE[]     = "CONDOR_KERBEROS_SHARE_CACHE";
const char STR_CONDOR_DAEMON_USER[]     = "CONDOR_DAEMON_USER";
const char STR_CONDOR_SERVER_KEYTAB[]   = "CONDOR_SERVER_KEYTAB";
const char STR_CONDOR_SERVER_PRINCIPAL[]= "CONDOR_SERVER_PRINCIPAL";
const char STR_CONDOR_CLIENT_KEYTAB[]   = "CONDOR_CLIENT_KEYTAB";
const char STR_CONDOR_CLIENT_PRINCIPAL[]= "CONDOR_CLIENT_PRINCIPAL";
const char STR_KRB_FORMAT[]             = "FILE:%s/krb_condor_%s.stash";
const char STR_DEFAULT_CONDOR_SERVICE[] = "host";
const char STR_CONDOR_CACHE_PREFIX[]    = "krb_condor_";

#define KERBEROS_DENY    0
#define KERBEROS_GRANT   1
#define KERBEROS_FORWARD 2
#define KERBEROS_MUTUAL  3

#endif
//----------------------------------------------------------------------

#if !defined(SKIP_AUTHENTICATION) && defined(WIN32)
extern HINSTANCE _condor_hSecDll;	// handle to SECURITY.DLL in sock.C
PSecurityFunctionTable Authentication::pf = NULL;
#endif

Authentication::Authentication( ReliSock *sock )
{
#if !defined(SKIP_AUTHENTICATION)
	mySock              = sock;
	auth_status         = CAUTH_NONE;
	claimToBe           = NULL;
	domainName          = NULL;
	GSSClientname       = NULL;
	authComms.buffer    = NULL;
	authComms.size      = 0;
	canUseFlags         = CAUTH_NONE;
	serverShouldTry     = NULL;
	RendezvousDirectory = NULL;
	// By default, it's mySock's end point
	isDaemon_           = FALSE;
	t_mode              = NORMAL;
	setRemoteAddress(sin_to_hostname(mySock->endpoint(), NULL));
        initialize();

#if defined(KERBEROS_AUTHENTICATION)
	krb_context_   = NULL;
	krb_principal_ = NULL;
	ccname_        = NULL;
	defaultCondor_ = NULL;
	defaultStash_  = NULL;
	keytabName_    = NULL;
	sessionKey_    = NULL;
	server_        = NULL;
#endif

#if defined(GSS_AUTHENTICATION)
	context_handle = GSS_C_NO_CONTEXT;
	credential_handle = GSS_C_NO_CREDENTIAL;
	ret_flags = 0;
#endif

#endif
}

Authentication::~Authentication()
{
#if !defined(SKIP_AUTHENTICATION)
	mySock = NULL;

	if ( claimToBe ) {
	  free(claimToBe);
	  claimToBe = NULL;
	}
	
	if ( serverShouldTry ) {
	  free( serverShouldTry );
	  serverShouldTry = NULL;
	}
	
	if ( RendezvousDirectory ) {
	  delete [] RendezvousDirectory;
	  RendezvousDirectory = NULL;
	}
	
	if (remotehost_) {
	  free(remotehost_);
	  remotehost_ = NULL;
	}

	if (authComms.buffer) {
	  free(authComms.buffer);
	  authComms.buffer = NULL;
	  authComms.size   = 0;
	}

	if (domainName) {
	  free(domainName);
	  domainName = NULL;
	}

#if defined(KERBEROS_AUTHENTICATION)
	if (krb_context_) {
	  if (krb_principal_) {
	    krb5_free_principal(krb_context_, krb_principal_);
	  }

	  if (sessionKey_) {
	    krb5_free_keyblock(krb_context_, sessionKey_);
          }

	  if (server_) {
	    krb5_free_principal(krb_context_, server_);
	  }

	  krb5_free_context(krb_context_);
	}

	if (defaultCondor_) {
	  free(defaultCondor_);
	  defaultCondor_ = NULL;
	}

	if (defaultStash_) {
	  free(defaultStash_);
	  defaultStash_ = NULL;
	}

	if (ccname_) {
	  free(ccname_);
	  ccname_ = NULL;
	}
#endif

#if defined(GSS_AUTHENTICATION)
	if (credential_handle) {
	  OM_uint32 major_status = 0; 
	  gss_release_cred(&major_status, &credential_handle);
	}
#endif

#endif
}

int
Authentication::authenticate( char *hostAddr )
{
#if defined(SKIP_AUTHENTICATION)
  return 0;
#else
  
  //call setupEnv if Server or if Client doesn't know which methods to try
  // if ( !mySock->isClient() && !isServer) 
  // interesting, I am not sure whether this is right or not

  if ( !mySock->isClient() || !canUseFlags ) {
    setupEnv( hostAddr );
  }
  
  auth_status = CAUTH_NONE;
  
  while (auth_status == CAUTH_NONE ) {
    int firm = handshake();
    if ( firm < 0 ) {
      dprintf(D_FULLDEBUG,"authentication failed due to network errors\n");
      break;
    }
    else 
      dprintf(D_ALWAYS, "Trying to use %d\n", firm);

    switch ( firm ) {
#if defined(GSS_AUTHENTICATION)
    case CAUTH_GSS:
      //authenticate_self_gss();
      if( authenticate_gss() ) {
	auth_status = CAUTH_GSS;
      }
      break;
#endif /* GSS_AUTHENTICATION */

#if defined(KERBEROS_AUTHENTICATION) 
    case CAUTH_KERBEROS:
      if ( authenticate_kerberos() ) {
	auth_status = CAUTH_KERBEROS;
      }
      break;
#endif
  
#if defined(WIN32)
    case CAUTH_NTSSPI:
      if ( authenticate_nt() ) {
	auth_status = CAUTH_NTSSPI;
      }
      break;
#else
    case CAUTH_FILESYSTEM:
      if ( authenticate_filesystem() ) {
	
	auth_status = CAUTH_FILESYSTEM;
      }
      break;
    case CAUTH_FILESYSTEM_REMOTE:
				//use remote filesystem authentication
      if ( authenticate_filesystem( 1 ) ) {
	
	auth_status = CAUTH_FILESYSTEM_REMOTE;
      }
      break;
#endif /* !defined(WIN32) */
    case CAUTH_CLAIMTOBE:
      if( authenticate_claimtobe() ) {
	auth_status = CAUTH_CLAIMTOBE;
      }
      break;
      
    default:
      dprintf(D_ALWAYS,"Authentication::authenticate-- bad handshake "
	      "FAILURE\n" );
      return 0;
    }
    //if authentication failed, try again after removing from client tries
    if ( auth_status == CAUTH_NONE && mySock->isClient() ) {
      canUseFlags &= ~firm;
    }
  }
  
  //if none of the methods succeeded, we fall thru to default "none" from above
  int retval = ( auth_status != CAUTH_NONE );
  dprintf(D_FULLDEBUG, "Authentication::authenticate %s\n", 
	  retval == 1 ? "Success" : "FAILURE" );
  return ( retval );
#endif /* SKIP_AUTHENTICATION */
}


int 
Authentication::isAuthenticated() 
{
#if defined(SKIP_AUTHENTICATION)
	return 0;
#else
	return( auth_status != CAUTH_NONE );
#endif
}


void
Authentication::unAuthenticate()
{
#if !defined(SKIP_AUTHENTICATION)
	auth_status = CAUTH_NONE;
	setOwner( NULL );
#endif
}


void
Authentication::setAuthAny()
{
#if !defined(SKIP_AUTHENTICATION)
	canUseFlags = CAUTH_ANY;
#endif
}

int
Authentication::setOwner( const char *owner ) 
{
#if defined(SKIP_AUTHENTICATION)
	return 0;
#else
	if ( !this ) {
	  return 0;
	}
	if ( claimToBe ) {
	  free(claimToBe);
	  claimToBe = NULL;
	}
	if ( owner ) {
	  claimToBe = strdup( owner );   // strdup uses new
	}
	return 1;
#endif /* SKIP_AUTHENTICATION */
}

//----------------------------------------------------------------------
// getRemoteAddress: after authentication, return the remote address
//----------------------------------------------------------------------
const char * Authentication :: getRemoteAddress()
{
  // If we are not using Kerberos
  return remotehost_;
}

void Authentication :: setRemoteAddress(char * address) 
{
  remotehost_ = strdup(address);
}
//----------------------------------------------------------------------
// getDomain:: return domain information
//----------------------------------------------------------------------
const char * Authentication :: getDomain()
{
#if defined(SKIP_AUTHENTICATION)
	return NULL;
#else
	return domainName;
#endif
}

const char * Authentication::getOwner() 
{
#if defined(SKIP_AUTHENTICATION)
	return NULL;
#else
		// Since we never use getOwner() like it allocates memory
		// anywhere in the code, it shouldn't actually allocate
		// memory.  We can always just return claimToBe, since it'll
		// either be NULL or the string we want, which is the old
		// semantics.  -Derek Wright 3/12/99
	return (const char*) claimToBe;
#endif
}

bool Authentication :: is_valid()
{
  bool valid = FALSE;
  switch (auth_status) {
#if !defined(SKIP_AUTHENTICATION)
  case CAUTH_GSS :
#if defined(GSS_AUTHENTICATION)
    valid = gss_is_valid();
#endif
    break;
  case CAUTH_KERBEROS:
#if defined(KERBEROS_AUTHENTICATION)
    valid = kerberos_is_valid();
#endif
    break;
#endif
  default:
    break;
  }

  return valid;
}

int Authentication :: encrypt(bool flag)
{
  int code = 0;
  if (flag == TRUE) {
    if (is_valid()){//check for flags to support shd be added 
      t_mode = ENCRYPT;
      code = 0;
    }
    else{
      t_mode = NORMAL;
      code = -1;
    }
  }
  else {
    t_mode = NORMAL;
    code = 0;
  }

  return code;
}
	
bool Authentication :: is_encrypt()
{
	if ( t_mode ==  ENCRYPT || t_mode == ENCRYPT_HDR )
		return TRUE;
	return FALSE;
}

int Authentication :: hdr_encrypt(){
		if ( t_mode == ENCRYPT){
			t_mode = ENCRYPT_HDR;
			return 0;
		}
		return -1;
}
	
bool Authentication :: is_hdr_encrypt(){
	if (t_mode  == ENCRYPT_HDR)  
		return TRUE;
	 else 
		return FALSE;
}

int Authentication :: wrap(char*  input, 
			   int    input_len, 
			   char*& output,
			   int&   output_len)
{
  int status = FALSE;

  switch (auth_status) {
#if !defined(SKIP_AUTHENTICATION)
  case CAUTH_GSS :
#if defined(GSS_AUTHENTICATION)
    status = gss_wrap_buffer(input, input_len, output, output_len);
#endif
    break;
  case CAUTH_KERBEROS:
#if defined(KERBEROS_AUTHENTICATION)
    status = kerberos_wrap(input, input_len, output, output_len);
#endif
    break;
#endif
  default:
    //output = strdup(input);
    output_len = 0;   // Nope, I did not wrap it.xb
    break;
  }
  return status;
}
	
int Authentication :: unwrap(char*  input, 
			     int    input_len, 
			     char*& output, 
			     int&   output_len)
{
  int status = FALSE;

  // one ugly piece of code
  switch (auth_status) {
#if !defined(SKIP_AUTHENTICATION)
  case CAUTH_GSS :
#if defined(GSS_AUTHENTICATION)
    status = gss_unwrap_buffer(input, input_len, output, output_len);
#endif
    break;
  case CAUTH_KERBEROS:
#if defined(KERBEROS_AUTHENTICATION)
    status = kerberos_unwrap(input, input_len, output, output_len);
#endif
    break;
#endif
  default:
    output = strdup(input);
    output_len = input_len;
    break;
  }

  return status;
}


#if !defined(SKIP_AUTHENTICATION)

void Authentication :: initialize() 
{
  char * username = my_username();

  if (mySock->isClient()) {

    //------------------------------------------
    // There are two possible cases:
    // 1. This is a user who is calling authentication
    // 2. This is a daemon who is calling authentication
    // 
    // So, we first find out whether this is a daemon or user
    //------------------------------------------

    if (strncmp(username, STR_DEFAULT_CONDOR_USER, strlen(STR_DEFAULT_CONDOR_USER)) == 0) {
      // I am a daemon! This is a daemon-daemon authentication
      isDaemon_ = TRUE;
    
      //fprintf(stderr,"This is a daemon with Condor uid:%d; uid:%d\n",
      //      get_condor_uid(), get_my_uid());
    }
    else {
      //fprintf(stderr, "This is a user: %s\n", username);
    }
  }
  
  free(username);
}

#if defined(WIN32)
int 
Authentication::sspi_client_auth( CredHandle& cred, CtxtHandle& cliCtx, 
								 const char *tokenSource )
{
	int rc;
	SecPkgInfo *secPackInfo;
	char *myTokenSource;
	
	//	myTokenSource = _strdup( tokenSource );
	myTokenSource = NULL;

	dprintf( D_FULLDEBUG,"sspi_client_auth() entered\n" );

	rc = (pf->QuerySecurityPackageInfo)( "NTLM", &secPackInfo );

	TimeStamp useBefore;

	rc = (pf->AcquireCredentialsHandle)( NULL, "NTLM", SECPKG_CRED_OUTBOUND,
		NULL, NULL, NULL, NULL, &cred, &useBefore );

	// input and output buffers
	SecBufferDesc obd, ibd;
	SecBuffer ob, ib;

	DWORD ctxReq, ctxAttr;

	ctxReq = ISC_REQ_REPLAY_DETECT | ISC_REQ_SEQUENCE_DETECT |
		ISC_REQ_CONFIDENTIALITY | ISC_REQ_DELEGATE;

	bool haveInbuffer = false;
	bool haveContext = false;
	ib.pvBuffer = NULL;

	while ( 1 )
	{
		obd.ulVersion = SECBUFFER_VERSION;
		obd.cBuffers = 1;
		obd.pBuffers = &ob; // just one buffer
		ob.BufferType = SECBUFFER_TOKEN; // preping a token here
		ob.cbBuffer = secPackInfo->cbMaxToken;
		ob.pvBuffer = LocalAlloc( 0, ob.cbBuffer );

		rc = (pf->InitializeSecurityContext)( &cred, 
			haveContext? &cliCtx: NULL,
			myTokenSource, ctxReq, 0, SECURITY_NATIVE_DREP, 
			haveInbuffer? &ibd: NULL,
			0, &cliCtx, &obd, &ctxAttr, &useBefore );

		if ( ib.pvBuffer != NULL )
		{
			LocalFree( ib.pvBuffer );
			ib.pvBuffer = NULL;
		}

		if ( rc == SEC_I_COMPLETE_AND_CONTINUE || rc == SEC_I_COMPLETE_NEEDED )
		{
			if ( pf->CompleteAuthToken != NULL ) // only if implemented
				(pf->CompleteAuthToken)( &cliCtx, &obd );
			if ( rc == SEC_I_COMPLETE_NEEDED )
				rc = SEC_E_OK;
			else if ( rc == SEC_I_COMPLETE_AND_CONTINUE )
				rc = SEC_I_CONTINUE_NEEDED;
		}

		// send the output buffer off to the server
		if ( ob.cbBuffer != 0 )
		{
			mySock->encode();

			// send the size of the blob, then the blob data itself
			if ( !mySock->code(ob.cbBuffer) || 
				 !mySock->end_of_message() ||
				 !mySock->put_bytes((const char *) ob.pvBuffer, ob.cbBuffer) ||
				 !mySock->end_of_message() ) {
				dprintf(D_ALWAYS,
					"ERROR sspi_client_auth() failed to send blob to server!\n");
				LocalFree( ob.pvBuffer );
				ob.pvBuffer = NULL;
				(pf->FreeContextBuffer)( secPackInfo );
				if ( myTokenSource ) {
					free( myTokenSource );
				}
				return 0;
			}			
		}
		LocalFree( ob.pvBuffer );
		ob.pvBuffer = NULL;

		if ( rc != SEC_I_CONTINUE_NEEDED )
			break;

		// prepare to get the server's response
		ibd.ulVersion = SECBUFFER_VERSION;
		ibd.cBuffers = 1;
		ibd.pBuffers = &ib; // just one buffer
		ib.BufferType = SECBUFFER_TOKEN; // preping a token here

		// receive the server's response
		ib.pvBuffer = NULL;
		mySock->decode();
		if ( !mySock->code(ib.cbBuffer) ||
			 !mySock->end_of_message() ||
			 !(ib.pvBuffer = LocalAlloc( 0, ib.cbBuffer )) ||
			 !mySock->get_bytes( (char *) ib.pvBuffer, ib.cbBuffer ) ||
			 !mySock->end_of_message() ) 
		{
			dprintf(D_ALWAYS,
				"ERROR sspi_client_auth() failed to receive blob from server!\n");
			if (ib.pvBuffer) {
				LocalFree(ib.pvBuffer);
				ib.pvBuffer = NULL;
			}
			(pf->FreeContextBuffer)( secPackInfo );
			if ( myTokenSource ) {
				free( myTokenSource );
			}
			return 0;
		}

		// by now we have an input buffer and a client context

		haveInbuffer = true;
		haveContext = true;

		// loop back for another round
		dprintf( D_FULLDEBUG,"sspi_client_auth() looping\n" );
	}

	// we arrive here as soon as InitializeSecurityContext()
	// returns != SEC_I_CONTINUE_NEEDED.

	if ( rc != SEC_E_OK ) {
		dprintf( D_ALWAYS,"sspi_client_auth(): Oops! ISC() returned %d!\n",
			rc );
	}

	(pf->FreeContextBuffer)( secPackInfo );
	dprintf( D_FULLDEBUG,"sspi_client_auth() exiting\n" );
	if ( myTokenSource ) {
		free( myTokenSource );
	}

	// return success
	return 1;
}

int 
Authentication::sspi_server_auth(CredHandle& cred,CtxtHandle& srvCtx)
{
	int rc;
	SecPkgInfo *secPackInfo;
	int it_worked = FALSE;		// assume failure

	dprintf(D_FULLDEBUG, "sspi_server_auth() entered\n" );

	rc = (pf->QuerySecurityPackageInfo)( "NTLM", &secPackInfo );

	TimeStamp useBefore;

	rc = (pf->AcquireCredentialsHandle)( NULL, "NTLM", SECPKG_CRED_INBOUND,
		NULL, NULL, NULL, NULL, &cred, &useBefore );

	// input and output buffers
	SecBufferDesc obd, ibd;
	SecBuffer ob, ib;

	DWORD ctxAttr;

	bool haveContext = false;

	while ( 1 )
	{
		// prepare to get the server's response
		ibd.ulVersion = SECBUFFER_VERSION;
		ibd.cBuffers = 1;
		ibd.pBuffers = &ib; // just one buffer
		ib.BufferType = SECBUFFER_TOKEN; // preping a token here

		// receive the client's POD
		ib.pvBuffer = NULL;
		mySock->decode();
		if ( !mySock->code(ib.cbBuffer) ||
			 !mySock->end_of_message() ||
			 !(ib.pvBuffer = LocalAlloc( 0, ib.cbBuffer )) ||
			 !mySock->get_bytes((char *) ib.pvBuffer, ib.cbBuffer) ||
			 !mySock->end_of_message() )
		{
			dprintf(D_ALWAYS,
				"ERROR sspi_server_auth() failed to received client POD\n");
			if ( ib.pvBuffer ) {
				LocalFree( ib.pvBuffer );
				ib.pvBuffer = NULL;
			}
			(pf->FreeContextBuffer)( secPackInfo );			
			return 0;
		}

		// by now we have an input buffer

		obd.ulVersion = SECBUFFER_VERSION;
		obd.cBuffers = 1;
		obd.pBuffers = &ob; // just one buffer
		ob.BufferType = SECBUFFER_TOKEN; // preping a token here
		ob.cbBuffer = secPackInfo->cbMaxToken;
		ob.pvBuffer = LocalAlloc( 0, ob.cbBuffer );

		rc = (pf->AcceptSecurityContext)( &cred, haveContext? &srvCtx: NULL,
			&ibd, 0, SECURITY_NATIVE_DREP, &srvCtx, &obd, &ctxAttr,
			&useBefore );

		if ( ib.pvBuffer != NULL )
		{
			LocalFree( ib.pvBuffer );
			ib.pvBuffer = NULL;
		}

		if ( rc == SEC_I_COMPLETE_AND_CONTINUE || rc == SEC_I_COMPLETE_NEEDED )
		{
			if ( pf->CompleteAuthToken != NULL ) // only if implemented
				(pf->CompleteAuthToken)( &srvCtx, &obd );
			if ( rc == SEC_I_COMPLETE_NEEDED )
				rc = SEC_E_OK;
			else if ( rc == SEC_I_COMPLETE_AND_CONTINUE )
				rc = SEC_I_CONTINUE_NEEDED;
		}

		// send the output buffer off to the server
		if ( ob.cbBuffer != 0 )
		{
			mySock->encode();
			if ( !mySock->code(ob.cbBuffer) ||
				 !mySock->end_of_message() ||
				 !mySock->put_bytes( (const char *) ob.pvBuffer, ob.cbBuffer ) ||
				 !mySock->end_of_message() ) 
			{
				dprintf(D_ALWAYS,
					"ERROR sspi_server_auth() failed to send output blob\n");
				LocalFree( ob.pvBuffer );
				ob.pvBuffer = NULL;
				(pf->FreeContextBuffer)( secPackInfo );			
				return 0;
			}
		}
		LocalFree( ob.pvBuffer );
		ob.pvBuffer = NULL;

		if ( rc != SEC_I_CONTINUE_NEEDED )
			break;

		haveContext = true;

		// loop back for another round
		dprintf(D_FULLDEBUG,"sspi_server_auth() looping\n");
	}

	// we arrive here as soon as InitializeSecurityContext()
	// returns != SEC_I_CONTINUE_NEEDED.

	if ( rc != SEC_E_OK ) {
		dprintf( D_ALWAYS,"sspi_server_auth(): Oops! ASC() returned %d!\n", 
			rc );
	}

	// now we try to use the context to Impersonate and thereby get the login
	rc = (pf->ImpersonateSecurityContext)( &srvCtx );
	if ( rc != SEC_E_OK ) {
		dprintf( D_ALWAYS,
			"sspi_server_auth(): Failed to impersonate (returns %d)!\n", rc );
	} else {
		char buf[256];
		DWORD bufsiz = sizeof buf;
		GetUserName( buf, &bufsiz );
		dprintf( D_FULLDEBUG,
			"sspi_server_auth(): user name is: \"%s\"\n", buf );
		setOwner(buf);
		it_worked = TRUE;
		(pf->RevertSecurityContext)( &srvCtx );
	}

	(pf->FreeContextBuffer)( secPackInfo );

	dprintf( D_FULLDEBUG,"sspi_server_auth() exiting\n" );

	// return success (1) or failure (0)
	return it_worked;
}

int
Authentication::authenticate_nt()
{
	int ret_value;
	CredHandle cred;
	CtxtHandle theCtx;

	cred.dwLower = 0L;
	cred.dwUpper = 0L;
	theCtx.dwLower = 0L;
	theCtx.dwUpper = 0L;

	if ( pf == NULL ) {
		PSecurityFunctionTable (*pSFT)( void );

		pSFT = (PSecurityFunctionTable (*)( void )) 
			GetProcAddress( _condor_hSecDll, "InitSecurityInterfaceA" );
		if ( pSFT )
		{
			pf = pSFT();
		}

		if ( pf == NULL )
		{
			EXCEPT("SECURITY.DLL load messed up!");
		}
	}

	if ( mySock->isClient() ) {
		//client authentication
		
		ret_value = sspi_client_auth(cred,theCtx,NULL);
	}
	else {
		//server authentication

		ret_value = sspi_server_auth(cred,theCtx);
	}

	// clean up
	if ( theCtx.dwLower != 0L || theCtx.dwUpper != 0L )
		(pf->DeleteSecurityContext)( &theCtx );
	if ( cred.dwLower != 0L || cred.dwUpper != 0L )
		(pf->FreeCredentialHandle)( &cred );

	//return 1 for success, 0 for failure. Server should send sucess/failure
	//back to client so client can know what to return.
	
	return ret_value;
}
#endif	// of if defined(WIN32)

void 
Authentication::setupEnv( char *hostAddr )
{
	char buffer[1024];
	char *pbuf;
	int tryGss = 0;
	int needfree = 0;

	memset(buffer, 0, 1024);
#if defined(GSS_AUTHENTICATION)
	
	if (!mySock->isClient()){
	  erase_env();

	  pbuf = param( STR_X509_DIRECTORY );
	  if (pbuf) {
	    sprintf( buffer, "%s=%s/certdir", STR_X509_CERT_DIR, pbuf);
	    putenv( strdup( buffer ) );
	    
	    sprintf( buffer, "%s=%s/usercert.pem", STR_X509_USER_CERT, pbuf);
	    putenv( strdup ( buffer ) );
	    
	    sprintf(buffer,"%s=%s/private/userkey.pem",STR_X509_USER_KEY,pbuf);
	    putenv( strdup ( buffer  ) );
	    
	    sprintf(buffer,"%s=%s/condor_ssl.cnf", STR_SSLEAY_CONF, pbuf);
	    putenv( strdup ( buffer ) );

	    tryGss = 1;
	    free(pbuf);
	  }
  	}	
	else{
	  pbuf = getenv(STR_X509_DIRECTORY );

	  if (pbuf) {
	    tryGss = 1;	

	    if (!getenv(STR_X509_CERT_DIR )){	
	      sprintf( buffer, "%s=%s/certdir", STR_X509_CERT_DIR, pbuf );
	      putenv(  strdup( buffer ) );
	    }
	    
	    if (!getenv( STR_X509_USER_CERT )){	
	      sprintf( buffer, "%s=%s/usercert.pem", STR_X509_USER_CERT, pbuf );
	      putenv( strdup ( buffer )  );
	    }
	    
	    if (!getenv( STR_X509_USER_KEY )){	
	      sprintf(buffer,"%s=%s/private/userkey.pem",STR_X509_USER_KEY, pbuf );
	      putenv( strdup ( buffer )  );
	    }
	    
	    if (!getenv(STR_SSLEAY_CONF)){	
	      sprintf(buffer,"%s=%s/condor_ssl.cnf", STR_SSLEAY_CONF, pbuf );
	      putenv( strdup ( buffer ) );
	    }

	    //free( pbuf );
	  }
	}
	
	pbuf =  param("CONDOR_GATEKEEPER") ; 

	if (pbuf){
	  sprintf(buffer ,"CONDOR_GATEKEEPER=%s",pbuf);
	  putenv( buffer );
	  free(pbuf);
	}
#endif
	
	if ( mySock->isClient() ) {
	  //client needs to know name of the schedd, I stashed hostAddr in 
	  //applicable ReliSock::ReliSock() or ReliSock::connect(), which 
	  //should only be called by clients. 

	  canUseFlags |= (int) CAUTH_CLAIMTOBE;
#if defined(WIN32)
	  canUseFlags |= (int) CAUTH_NTSSPI;
#else
	  canUseFlags |= (int) CAUTH_FILESYSTEM;
	  
	  //RendezvousDirectory is for use by shared-filesystem filesys auth.
	  //if user specfied RENDEZVOUS_DIRECTORY, extract it
	  if ( ( pbuf = getenv( "RENDEZVOUS_DIRECTORY" ) ) ) {
	    RendezvousDirectory = strnewp( pbuf );
	    canUseFlags |= (int) CAUTH_FILESYSTEM_REMOTE;
	  }
#if defined( GSS_AUTHENTICATION )
		if ( tryGss ) {
			canUseFlags |= CAUTH_GSS;
		}
#endif

#if defined(KERBEROS_AUTHENTICATION)
	  canUseFlags |= CAUTH_KERBEROS;
#endif
	  
#endif /* defined WIN32 */
	}
	else {   //server
	  if ( serverShouldTry ) {
	    delete serverShouldTry;
	    serverShouldTry = NULL;
	  }
	  serverShouldTry = param( "AUTHENTICATION_METHODS" );
	}
}

int
Authentication::authenticate_claimtobe() {
	int retval = 0;
	char *tmpOwner = NULL;

	if ( mySock->isClient() ) {
		mySock->encode();
		if ( tmpOwner = my_username() ) {
			//send 1 and then username
			retval = 1;
			mySock->code( retval );
			mySock->code( tmpOwner );
			setOwner( tmpOwner );
			free( tmpOwner );
			mySock->end_of_message();
			mySock->decode();
			mySock->code( retval );
		} else {
			//send 0
			mySock->code( retval );
		}
	} else { //server side
		mySock->decode();
		mySock->code( retval );
		//if 1, receive owner and send back ok
		if( retval == 1 ) {
			mySock->code( tmpOwner );
			mySock->end_of_message();
			mySock->encode();
			if( tmpOwner ) {
				retval = 1;
				setOwner( tmpOwner );
				delete [] tmpOwner;
			} else {
				retval = 0;
			}
			mySock->code( retval );
		}
	}

	mySock->end_of_message();
	return retval;
}

void 
Authentication::setAuthType( authentication_state state ) {
	auth_status = state;
}

int
Authentication::handshake()
{
	int shouldUseMethod = 0;

	int clientCanUse = 0;
	int canUse = (int) canUseFlags;

	if ( mySock->isClient() ) {
		mySock->encode();
		if ( !mySock->code( canUse ) ||
			 !mySock->end_of_message() )
		{
			return -1;
		}
		mySock->decode();
		if ( !mySock->code( shouldUseMethod ) ||
			 !mySock->end_of_message() )
		{
			return -1;
		}
	}
	else { //server
		mySock->decode();
		if ( !mySock->code( clientCanUse ) ||
			 !mySock->end_of_message() )
		{
			return -1;
		}

		shouldUseMethod = selectAuthenticationType( clientCanUse );


		mySock->encode();
		if ( !mySock->code( shouldUseMethod ) ||
			 !mySock->end_of_message() )
		{
			return -1;
		}
	}

	dprintf(D_FULLDEBUG,
		"handshake: clientCanUse=%d,shouldUseMethod=%d\n",
		clientCanUse,shouldUseMethod);

	return( shouldUseMethod );
}


#if !defined(WIN32)
int
Authentication::authenticate_filesystem( int remote = 0 )
{
	char *new_file = NULL;
	int fd = -1;
	char *owner = NULL;
	int retval = -1;

	if ( mySock->isClient() ) {
		if ( remote ) {
				//send over the directory
			mySock->encode();
			mySock->code( RendezvousDirectory );
			mySock->end_of_message();
		}
		mySock->decode();
		mySock->code( new_file );
		if ( new_file ) {
			fd = open(new_file, O_RDONLY | O_CREAT | O_TRUNC, 0666);
			::close(fd);
		}
		mySock->end_of_message();
		mySock->encode();
		//send over fd as a success/failure indicator (-1 == failure)
		mySock->code( fd );
		mySock->end_of_message();
		mySock->decode();
		mySock->code( retval );
		mySock->end_of_message();
		if ( new_file ) {
			unlink( new_file );
			free( new_file );
		}
	}
	else {  //server 
		setOwner( NULL );

		if ( remote ) {
				//get RendezvousDirectory from client
			mySock->decode();
			mySock->code( RendezvousDirectory );
			mySock->end_of_message();
			dprintf(D_FULLDEBUG,"RendezvousDirectory: %s\n", RendezvousDirectory );
			new_file = tempnam( RendezvousDirectory, "qmgr_");
		}
		else {
			new_file = tempnam("/tmp", "qmgr_");
		}
		//now, send over filename for client to create
		// man page says string returned by tempnam has been malloc()'d
		mySock->encode();
		mySock->code( new_file );
		mySock->end_of_message();
		mySock->decode();
		mySock->code( fd );
		mySock->end_of_message();
		mySock->encode();

		retval = 0;
		if ( fd > -1 ) {
			//check file to ensure that claimant is owner
			struct stat stat_buf;

			if ( lstat( new_file, &stat_buf ) < 0 ) {
				retval = -1;  
			}
			else {
			// Authentication should fail if a) owner match fails, or b) the
			// file is either a hard or soft link (no links allowed because they
			// could spoof the owner match).  -Todd 3/98
				if ( 
					(stat_buf.st_nlink > 1) ||	 // check for hard link
					(S_ISLNK(stat_buf.st_mode)) ) 
				{
					retval = -1;
				} 
				else {
					//need to lookup username from file and do setOwner()
					char *tmpOwner = my_username( stat_buf.st_uid );
					setOwner( tmpOwner );
					free( tmpOwner );
				}
			}
		} 
		else {
			retval = -1;
			dprintf(D_ALWAYS,
					"invalid state in authenticate_filesystem (file %s)\n",
					new_file ? new_file : "(null)" );
		}

		mySock->code( retval );
		mySock->end_of_message();
		free( new_file );
	}
	dprintf( D_FULLDEBUG, "authentcate_filesystem%s status: %d\n", 
			remote ? "(REMOTE)" : "()", retval == 0 );
	return( retval == 0 );
}
#endif /* !defined(WIN32) */

int
Authentication::selectAuthenticationType( int clientCanUse ) 
{
	int retval = 0;

	if ( !serverShouldTry ) {
		//wasn't specified in config file [param("AUTHENTICATION_METHODS")]
		//default to generic authentication:
#if defined(WIN32)
		serverShouldTry = strdup("NTSSPI");
#else
		serverShouldTry = strdup("FS");
#endif
	}
	StringList server( serverShouldTry );
	char *tmp = NULL;

	server.rewind();
	while ( tmp = server.next() ) {
	  if ( ( clientCanUse & CAUTH_GSS ) ) {
	    retval = CAUTH_GSS;
	    break;
	  }
#if defined(WIN32)
	  if ( ( clientCanUse & CAUTH_NTSSPI ) && !stricmp( tmp, "NTSSPI" ) ) {
	    retval = CAUTH_NTSSPI;
	    break;
	  }
#else
	  if ( ( clientCanUse & CAUTH_FILESYSTEM ) && !stricmp( tmp, "FS" ) ) {
	    retval = CAUTH_FILESYSTEM;
	    break;
	  }
	  if ( ( clientCanUse & CAUTH_FILESYSTEM_REMOTE ) 
	       && !stricmp( tmp, "FS_REMOTE" ) ) {
	    retval = CAUTH_FILESYSTEM_REMOTE;
	    break;
	  }

#if defined(KERBEROS_AUTHENTICATION)
	  if ( ( clientCanUse & CAUTH_KERBEROS) && !stricmp( tmp, "KERBEROS")){
	    retval = CAUTH_KERBEROS;
	    break;
	  }
#endif

#endif
	  if ( ( clientCanUse & CAUTH_CLAIMTOBE ) && !stricmp( tmp, "CLAIMTOBE" ) )
	    {
	      retval = CAUTH_CLAIMTOBE;
	      break;
	    }
	}
	
	return retval;
}

#if defined(GSS_AUTHENTICATION)

int 
Authentication::lookup_user_gss( char *client_name ) 
{
	char filename[MAXPATHLEN];

	sprintf( filename, "%.*s/index.txt", MAXPATHLEN-11, 
			getenv( "X509_CERT_DIR" ) );

	FILE *index;
	if ( !(index = fopen(  filename, "r" ) ) ) {
		dprintf( D_ALWAYS, "unable to open index file %s, errno %d\n", 
				filename, errno );
		return -1;
	}

	//find entry
	int found = 0,i=0;
 	char line[256],ch_temp,ch = 9,temp[2],*pos;
   	sprintf(temp,"%c",ch);
  	while ( !found){ 
    	 for (i=0;(line[i] = fgetc(index)) != '\n' && (ch_temp=line[i]) != EOF;i++);
     		line[i] = '\0';

     //Valid user entries have 'V' as first byte in their cert db entry
     	if ( line[0] == 'V'){
       		pos = line;
		    while (*pos != '/') pos++;
			pos = strtok(pos,temp);
	        if (!strcmp(pos,client_name)){
		    	found = 1;
			    dprintf(D_ALWAYS, "found the entry for client\n");
		    }
    	}
	    if (ch_temp == EOF) break;
   }
	fclose( index );
	if ( !found ) {
		dprintf( D_ALWAYS, "unable to find V entry for %s in %s\n", 
				filename, client_name );
		return( -1 );
	}

	return 0;
}

//cannot make this an AuthSock method, since gss_assist method expects
//three parms, methods have hidden "this" parm first. Couldn't figure out
//a way around this, so made AuthSock have a member of type AuthComms
//to pass in to this method to manage buffer space.  //mju
int 
authsock_get(void *arg, void **bufp, size_t *sizep)
{
	/* globus code which calls this function expects 0/-1 return vals */

	//authsock must "hold onto" GSS state, pass in struct with comms stuff
	GSSComms *comms = (GSSComms *) arg;
	ReliSock *sock = comms->sock;
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
	if ( stat ) {
	  sock->code_bytes( *bufp, *((int *)sizep) );
	}

	sock->end_of_message();

	//check to ensure comms were successful
	if ( !stat ) {
		dprintf( D_ALWAYS, "authsock_get (read from socket) failure\n" );
		return -1;
	}
	return 0;
}

int 
authsock_put(void *arg,  void *buf, size_t size)
{
	//param is just a AS*
	ReliSock *sock = (ReliSock *) arg;
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

int 
Authentication::authenticate_self_gss()
{
	OM_uint32 major_status;
	OM_uint32 minor_status;
	char comment[1024];
	if ( credential_handle != GSS_C_NO_CREDENTIAL ) { // user already auth'd 
		dprintf( D_FULLDEBUG, "This process has a valid certificate & key\n" );
		return TRUE;
	}

	// ensure all env vars are in place, acquire cred will fail otherwise 

	//use gss-assist to verify user (not connection)
	//this method will prompt for password if private key is encrypted!
	int time = mySock->timeout(60 * 5);  //allow user 5 min to type passwd

	priv_state priv;

	if (!mySock->isClient() || isDaemon_) {
	  priv = set_root_priv();
	}

	major_status = globus_gss_assist_acquire_cred(&minor_status,
		GSS_C_BOTH, &credential_handle);

	if (!mySock->isClient() || isDaemon_) {
	  set_priv(priv);
	}

	mySock->timeout(time); //put it back to what it was before

	if (major_status != GSS_S_COMPLETE)
	{
		sprintf(comment,"authenticate_self_gss: acquiring self 
				credentials failed \n");
		print_log( major_status,minor_status,0,comment); 
		credential_handle = GSS_C_NO_CREDENTIAL; 
		return FALSE;
	}
		
	dprintf( D_FULLDEBUG, "This process has a valid certificate & key\n" );
	return TRUE;
}

int 
Authentication::authenticate_client_gss()
{
  OM_uint32		 major_status = 0;
  OM_uint32		 minor_status = 0;
  char *proxy = NULL;

  priv_state priv;

  //fprintf(stderr, "GSS\n");
	if ( !authenticate_self_gss() ) {
	  dprintf( D_ALWAYS, 
		   "failure authenticating client from auth_connection_client\n" );
	  return FALSE;
	}
	 
	char *gateKeeper = param( "CONDOR_GATEKEEPER" );

	if ( !gateKeeper ) {
	  dprintf( D_ALWAYS, "env var CONDOR_GATEKEEPER not set\n" );
	  return FALSE;
	}

	authComms.sock = mySock;
	authComms.buffer = NULL;
	authComms.size = 0;

	if (isDaemon_) {
	  priv = set_root_priv();
	}

	major_status = globus_gss_assist_init_sec_context(&minor_status,
		  credential_handle, &context_handle,
		  gateKeeper,
		  GSS_C_DELEG_FLAG|GSS_C_MUTUAL_FLAG,
		  &ret_flags, &token_status,
		  authsock_get, (void *) &authComms,
		  authsock_put, (void *) mySock
	);

	if (isDaemon_) {
	  set_priv(priv);
	}

	if (major_status != GSS_S_COMPLETE)
	{
		print_log(major_status,minor_status,token_status,
				"Authentication Failure on client side");
		//free(gateKeeper);
		return FALSE;
	}



	auth_status = CAUTH_GSS;
	dprintf(D_ALWAYS,"valid GSS connection established to %s\n", gateKeeper);
	//proxy = getenv(STR_X509_USER_PROXY);
	//my_credential = new X509_Credential(USER, proxy);
	//free(proxy);
	
	//if (!my_credential -> forward_credential(mySock))
	//	dprintf(D_ALWAYS,"Successfully forwarded credentials\n");
	//delete my_credential;
	
	gss_delete_sec_context( &minor_status, &context_handle, GSS_C_NO_BUFFER );
	free(gateKeeper);

	return TRUE;
}

int 
Authentication::authenticate_server_gss()
{
	int rc;
	OM_uint32 major_status = 0;
	OM_uint32 minor_status = 0;
	int		 token_status = 0;

	if ( !authenticate_self_gss() ) {
		dprintf( D_ALWAYS, 
			"failure authenticating self in auth_connection_server\n" );
		return FALSE;
	}
	 
	authComms.sock = mySock;
	authComms.buffer = NULL;
	authComms.size = 0;
	
	priv_state priv;

	priv = set_root_priv();

	major_status = globus_gss_assist_accept_sec_context(&minor_status,
				 &context_handle, credential_handle,
				 &GSSClientname,
				 &ret_flags, NULL,/* don't need user_to_user */
				 &token_status,
				 NULL,     /* don't delegate credential */
				 authsock_get, (void *) &authComms,
				 authsock_put, (void *) mySock
	);

	set_priv(priv);

	if ( (major_status != GSS_S_COMPLETE))// ||
			//( lookup_user_gss( GSSClientname ) < 0 ) )
	{
	  if (major_status != GSS_S_COMPLETE) 
	    print_log(major_status,minor_status,token_status,
		      "GSS authentication failure on server side" );
	  else 
	    dprintf( D_ALWAYS, "server: user lookup failure.\n" );
	  return FALSE;
	}

	if ( !nameGssToLocal() ) {
		dprintf(D_ALWAYS,"Could not find local mapping \n");
		return( FALSE );
	}

	auth_status = CAUTH_GSS;
	dprintf(D_FULLDEBUG,"valid GSS connection established to %s\n", 
			GSSClientname);
	
	//my_credential = new X509_Credential(SYSTEM,NULL); 
	//if (my_credential -> 
	//		receive_credential(mySock,GSSClientname)==0){
	//	dprintf(D_ALWAYS,"successfully received credentials\n");	
	//}
	//else {
	//  dprintf(D_ALWAYS,"unable to receive credentials\n");
	//}

	//delete my_credential;

	gss_delete_sec_context( &minor_status, &context_handle, GSS_C_NO_BUFFER );
	return TRUE;
}

 int Authentication::nameGssToLocal() {
   //this might need to change with SSLK5 stuff
   //just extract username from /CN=<username>@<domain,etc>

  char filename[MAXPATHLEN];
  int found = 0,i=0;
  char line[256],ch_temp,ch = 9,temp[2];
  char *pos,*pos1, *pos_temp;
  FILE *index;
  
  sprintf( filename, "%.*s/index.txt", MAXPATHLEN-11, 
	   getenv( STR_X509_CERT_DIR ) );

  if ( !(index = fopen(  filename, "r" ) ) ) {
    dprintf( D_ALWAYS, "unable to open index file %s, errno %d\n", 
	     filename, errno );
    return -1;
  }

  sprintf(temp,"%c",ch);
  while ( !found){ 
    for (i=0;(line[i] = fgetc(index)) != '\n' && (ch_temp=line[i]) != EOF;i++);
    line[i] = '\0';

    //Valid user entries have 'V' as first byte in their cert db entry
    if ( line[0] == 'V'){
      pos = line;
      pos1 = pos+ strlen(pos) -1;
      while (*pos1 != 9) pos1--;
	  pos_temp = pos1;
      pos1++;
      while (*pos != '/') pos++;
	  while (*pos_temp	== ' ' || *pos_temp == 9 )
		  pos_temp--;
	  pos_temp++;
	  *pos_temp = '\0';
      if (!strcmp(pos,GSSClientname))
		found = 1;
    }
    if (ch_temp == EOF) break;
  }
  fclose( index );
  if (found)
    {
      setOwner((const char*)pos1);
      return 1;
    }
  else {
    dprintf(D_ALWAYS, "lookup failure\n");
    return 0;
  }
 }


int
Authentication::authenticate_gss() 
{
	int status = 1;

	//don't just return TRUE if isAuthenticated() == TRUE, since 
	//we should BALANCE calls of Authenticate() on client/server side
	//just like end_of_message() calls must balance!

	if ( !authenticate_self_gss() ) {
		dprintf( D_ALWAYS, "authenticate: user creds not established\n" );
		status = 0;
	}

	if ( status ) {
		//temporarily change timeout to 5 minutes so the user can type passwd
		//MUST do this even on server side, since client side might call
		//authenticate_self_gss() while server is waiting on authenticate!!
		int time = mySock->timeout(60 * 5); 

		switch ( mySock->isClient() ) {
			case 1: 
				status = authenticate_client_gss();
				break;
			default: 
				status = authenticate_server_gss();
				break;
		}
		mySock->timeout(time); //put it back to what it was before
	}

	return( status );
}

int Authentication::get_user_x509name(char *proxy_file, char* name)
{

	proxy_cred_desc *     pcd = NULL;
    struct stat           stx; 
	/* initialize SSLeay and the error strings */
    ERR_load_prxyerr_strings(0);
    SSLeay_add_ssl_algorithms();
    /* Load proxy */
    if (!proxy_file)
   	 proxy_get_filenames(1, NULL, NULL, &proxy_file, NULL, NULL);
    if (!proxy_file)
    {
        dprintf(D_ALWAYS,"ERROR: unable to determine proxy file name\n");
        return 1;
    }

    if (stat(proxy_file,&stx) != 0) {
    	return 1;
    }

    pcd = proxy_cred_desc_new();
    if (!pcd)
    {
       dprintf(D_ALWAYS,"ERROR: problem during internal initialization\n");
       return 1;
    }

    if (proxy_load_user_cert(pcd, proxy_file, NULL))
    {
    	dprintf(D_ALWAYS,"ERROR: unable to load proxy");
	    return 1;
	}
	name=X509_NAME_oneline(X509_get_subject_name(pcd->ucert),NULL,0);
	return 0;
}

 void  Authentication :: erase_env()
 {
   int i,j;
   char *temp=NULL,*temp1=NULL;

   for (i=0;environ[i] != NULL;i++)
     {
       
       temp1 = (char*)strdup(environ[i]);
       if (!temp1)
	 return;
       temp = (char*)strtok(temp1,"=");
       if (temp && !strcmp(temp, "X509_USER_PROXY" ))
	 break;
     }
   for (j = i;environ[j] != NULL;j++)
     environ[j] = environ[j+1];

   return;
 }

void Authentication :: print_log(OM_uint32 major_status,
				 OM_uint32 minor_status,
				 int       token_status, 
				 char *    comment)
{
	char* buffer;
	globus_gss_assist_display_status_str
		(&buffer,
		 comment,
		 major_status,
		 minor_status,
		 token_status);
	if (buffer){
	  dprintf(D_ALWAYS,"%s\n",buffer);
	  free(buffer);
	}
}

OM_uint32 Authentication :: gss_wrap_buffer(char *data_in,int length_in,
		char*& data_out,int& length_out )
{
	
	OM_uint32 major_status;
	OM_uint32 minor_status;
	
	gss_buffer_desc input_token_desc  = GSS_C_EMPTY_BUFFER;
	gss_buffer_t    input_token       = &input_token_desc;
	gss_buffer_desc output_token_desc = GSS_C_EMPTY_BUFFER;
	gss_buffer_t    output_token      = &output_token_desc;

	if (!is_valid())
		return GSS_S_FAILURE;	

	input_token->value = data_in;
	input_token->length = length_in;

	major_status = gss_wrap(&minor_status,
                          context_handle,
                          0,
                          GSS_C_QOP_DEFAULT,
                          input_token,
                          NULL,
                          output_token);
	
	data_out = (char*)output_token -> value;
	length_out = output_token -> length;
	
	return major_status;
	
}

OM_uint32 Authentication :: gss_unwrap_buffer(char* data_in,int length_in,
		char*& data_out , int& length_out)
{
	OM_uint32 major_status;
	OM_uint32 minor_status;
	
	gss_buffer_desc input_token_desc  = GSS_C_EMPTY_BUFFER;
	gss_buffer_t    input_token       = &input_token_desc;
	gss_buffer_desc output_token_desc = GSS_C_EMPTY_BUFFER;
	gss_buffer_t    output_token      = &output_token_desc;

	if (!is_valid()) {
		return GSS_S_FAILURE;
	}
	
	input_token -> value = data_in;
	input_token -> length = length_in;
	
	major_status = gss_unwrap(&minor_status,
				  context_handle,
				  input_token,
				  output_token,
				  NULL,
				  NULL);
	
	
	data_out = (char*)output_token -> value;
	length_out = output_token -> length;
	
	return major_status;
}
	
bool Authentication ::  gss_is_valid()
{
	OM_uint32 major_status;
	OM_uint32 minor_status;
	OM_uint32 time_rec;
	
	major_status = gss_context_time
	  (&minor_status ,
	   context_handle ,
	   &time_rec);
	if (major_status == GSS_S_COMPLETE) {
		return TRUE;
	}
	else {
		return FALSE;
	}
}	

#endif /* defined(GSS_AUTHENTICATION) */

//----------------------------------------------------------------------
// Kerberos Authentication Code
//----------------------------------------------------------------------
#if defined(KERBEROS_AUTHENTICATION)

//----------------------------------------------------------------------
// Authentication from client side
//----------------------------------------------------------------------
int Authentication::authenticate_client_kerberos()
{
  krb5_error_code        code;
  krb5_auth_context      auth_context = 0;
  krb5_flags             flags;
  krb5_data              request;
  krb5_error           * error = NULL;
  krb5_principal         server;
  krb5_creds             creds, * new_creds = NULL;
  krb5_ccache            ccdef = (krb5_ccache) NULL;
  int                    reply, rc = FALSE;

  //------------------------------------------
  // Set up the flags
  //------------------------------------------
  flags = AP_OPTS_MUTUAL_REQUIRED | AP_OPTS_USE_SUBKEY;
  memset(&creds, 0, sizeof(creds));

  if (krb5_cc_resolve(krb_context_, ccname_, &ccdef)) {
    goto error;
  }

  if (code = krb5_copy_principal(krb_context_,krb_principal_,&creds.client)){
    goto error;
  }

  if (code = krb5_copy_principal(krb_context_, server_, &creds.server)) {
    goto error;
  }

  if (code = krb5_get_credentials(krb_context_, 
				  0,
				  ccdef, 
				  &creds, 
				  &new_creds)) {
    goto error;
  }                                   
  
  //------------------------------------------
  // Load local addresses
  //------------------------------------------
  if (new_creds->addresses) {
    //fprintf(stderr, "addresses is already loaded\n");
  }
  else {
    if (code = krb5_os_localaddr(krb_context_, &(new_creds->addresses))) {
      goto error;
    }
  }

  //------------------------------------------
  // Let's create the KRB_AP_REQ message
  //------------------------------------------

  if (code = krb5_auth_con_init(krb_context_, &auth_context)) {
    goto error;
  }
 
  krb5_auth_con_setflags(krb_context_, auth_context, KRB5_AUTH_CONTEXT_RET_TIME); 

  if (code = krb5_auth_con_genaddrs(krb_context_, 
				    auth_context, 
				    mySock->get_file_desc(),
				    KRB5_AUTH_CONTEXT_GENERATE_LOCAL_FULL_ADDR|
				    KRB5_AUTH_CONTEXT_GENERATE_REMOTE_FULL_ADDR
				    )) {
    goto error;
  }

  if (code = krb5_mk_req_extended(krb_context_, 
				  &auth_context, 
				  flags,
				  0, 
				  new_creds, 
				  &request)) {
    dprintf(D_ALWAYS, "Unable to build request buffer\n");
    goto error;
  }

  // Send out the request
  if ((reply = send_request(&request)) != KERBEROS_MUTUAL) {
    dprintf(D_ALWAYS, "Could not mutual authenticate server!\n");
    return FALSE;
  }

  //------------------------------------------
  // Now, mutual authenticate
  //------------------------------------------
  reply = client_mutual_authenticate(&auth_context);

  switch (reply) 
    {
    case KERBEROS_DENY:
      dprintf(D_ALWAYS, "Authentication failed\n");
      return FALSE;
      break; // unreachable
    case KERBEROS_FORWARD:
      // We need to forward the credentials
      // We could do a fast forwarding (i.e stashing, if client/server
      // are located on the same machine. However, I want to keep the
      // forwarding mechanism clean, so, we use krb5_fwd_tgt_creds
      // regardless of where client/server are located
      
      // This is an implict GRANT
      if (forward_tgt_creds(auth_context, new_creds, ccdef)) {
	dprintf(D_ALWAYS, "Unable to forward credentials\n");
	return FALSE;  
      }
    default:
      dprintf(D_ALWAYS, "Response is invalid\n");
      break;
    }

  //------------------------------------------
  // Success, do some cleanup
  //------------------------------------------
  getRemoteHost(auth_context);

  rc = TRUE;

  //------------------------------------------
  // Store the session key for encryption
  //------------------------------------------
  if (code = krb5_copy_keyblock(krb_context_, 
				&(new_creds->keyblock), 
				&sessionKey_)) {
    goto error;			  
  }

  goto cleanup;

 error:

  dprintf(D_ALWAYS, "%s\n", error_message(code));

  rc = FALSE;

 cleanup:

  if (auth_context) {    
    krb5_auth_con_free(krb_context_, auth_context);
  }

  krb5_free_cred_contents(krb_context_, &creds);

  if (new_creds) {
    krb5_free_creds(krb_context_, new_creds);
  }

  if (request.data) {
    free(request.data);
  }

  krb5_cc_close(krb_context_, ccdef);

  return rc;

}

//----------------------------------------------------------------------
// authenticate_server_kerberos: authenticating from the server side
//----------------------------------------------------------------------
int Authentication::authenticate_server_kerberos()
{
  krb5_error_code   code;
  krb5_auth_context auth_context = 0;
  krb5_flags        flags = 0;
  krb5_data         request, reply;
  priv_state        priv;
  int               time, message, rc = FALSE;
  krb5_ticket *     ticket = NULL;
  //------------------------------------------
  // First, acquire credential
  //------------------------------------------
  if ( !server_acquire_credential() ) {
    return rc;
  }
  
  //------------------------------------------
  // Now, accept the connection
  //------------------------------------------
  dprintf(D_ALWAYS, "Accepting connection\n");
  
  //------------------------------------------
  // First, initialize auth_context
  //------------------------------------------
  if (code = krb5_auth_con_init(krb_context_, &auth_context)) {
    dprintf(D_ALWAYS, "Server is unable to initialize auth_context\n");
    return rc;
  }

  if (code = krb5_auth_con_genaddrs(krb_context_, 
				    auth_context, 
				    mySock->get_file_desc(),
				    KRB5_AUTH_CONTEXT_GENERATE_LOCAL_FULL_ADDR|
				    KRB5_AUTH_CONTEXT_GENERATE_REMOTE_FULL_ADDR
				    )) {
    return rc;
  }

  //------------------------------------------
  // Get te KRB_AP_REQ message
  //------------------------------------------
  if(read_request(&request) == FALSE) {
    dprintf(D_ALWAYS, "Server is unable to read request\n");
    goto error;
  }

  dprintf(D_ALWAYS, "Reading request object\n");

  priv = set_root_priv();   // Get the old privilige
  
  if (code = krb5_rd_req(krb_context_,
			 &auth_context,
			 &request,
			 krb_principal_,
			 NULL,
			 &flags,
			 &ticket)) {
    set_priv(priv);   // Reset
    goto error;
  }
  set_priv(priv);   // Reset

  //------------------------------------------
  // See if mutual authentication is required
  //------------------------------------------
  if (flags & AP_OPTS_MUTUAL_REQUIRED) {
    if (code = krb5_mk_rep(krb_context_, auth_context, &reply)) {
      goto error;
    }

    message = KERBEROS_MUTUAL;
    mySock->encode();
    if (!(mySock->code(message)) || !(mySock->end_of_message())) {
      goto cleanup;
    }

    // send the message
    if (send_request(&reply) != KERBEROS_GRANT) {
      goto cleanup;
    }
  }

  //------------------------------------------
  // extract client addresses
  //------------------------------------------
  if (ticket->enc_part2->caddrs) {
    struct in_addr in;
    memcpy(&(in.s_addr), 
	   ticket->enc_part2->caddrs[0]->contents, 
	   sizeof(in_addr));

    setRemoteAddress(inet_ntoa(in));

    dprintf(D_ALWAYS, "Client address is %s\n", remotehost_);
  }

  // First, map the name, this has to take place before receive_tgt_creds!
  if (!map_kerberos_name(ticket)) {
    dprintf(D_ALWAYS, "Unable to map name ");
    goto cleanup;
  }

  // Next, see if we need client to forward the credential as well
  if (receive_tgt_creds(auth_context, ticket)) {
    goto cleanup;
  }

  // copy the session key
  if (code = krb5_copy_keyblock(krb_context_, 
				ticket->enc_part2->session, 
				&sessionKey_)){
    goto error;
  }

  //------------------------------------------
  // We are now authenticated!
  //------------------------------------------
  dprintf(D_ALWAYS, "User %s is now authenticated!\n", claimToBe);

  rc = TRUE;

  goto cleanup;

 error:
  message = KERBEROS_DENY;

  mySock->encode();
  if ((!mySock->code(message)) || (!mySock->end_of_message())) {
    dprintf(D_ALWAYS, "Failed to send response message!\n");
  }

  dprintf(D_ALWAYS, "%s\n", error_message(code));
  
 cleanup:

  //------------------------------------------
  // Free up some stuff
  //------------------------------------------
  if (auth_context) {
    krb5_auth_con_free(krb_context_, auth_context);
  }

  if (ticket) {
    krb5_free_ticket(krb_context_, ticket);
  }

  //------------------------------------------
  // Free it for now, in the future, we might 
  // need this for future secure transctions.
  //------------------------------------------
  if (request.data) {
    free(request.data);
  }

  if (reply.data) {
    free(reply.data);
  }

  return rc;
}

//----------------------------------------------------------------------
// client_mutual_authentication
//----------------------------------------------------------------------
int Authentication :: client_mutual_authenticate(krb5_auth_context * auth)
{
  krb5_ap_rep_enc_part * rep = NULL;
  krb5_error_code        code;
  krb5_data              request;
  int reply            = KERBEROS_DENY;
  int message;

  if (read_request(&request) == FALSE) {
    return KERBEROS_DENY;
  }
  
  if (code = krb5_rd_rep(krb_context_, *auth, &request, &rep)) {
    goto error;
  }

  if (rep) {
    krb5_free_ap_rep_enc_part(krb_context_, rep);
  }

  message = KERBEROS_GRANT;
  mySock->encode();
  if (!(mySock->code(message)) || !(mySock->end_of_message())) {
    return KERBEROS_DENY;
  }

  mySock->decode();
  if (!(mySock->code(reply)) || !(mySock->end_of_message())) {
    return KERBEROS_DENY;
  }

  free(request.data);

  return reply;
 error:
  free(request.data);

  dprintf(D_ALWAYS, "%s\n", error_message(code));
  return KERBEROS_DENY;
}

//----------------------------------------------------------------------
// get_server_info: retrieve server information
//----------------------------------------------------------------------
int Authentication :: get_server_info(void)
{
  struct hostent * hp;
  krb5_error_code  code;

  dprintf(D_ALWAYS, "Getting server info\n");
  //------------------------------------------
  // Form the info for server
  //------------------------------------------
  
  hp = gethostbyaddr((char *) &(mySock->endpoint())->sin_addr, 
		     sizeof (struct in_addr),
		     mySock->endpoint()->sin_family);

  if (hp == NULL) {
    dprintf(D_ALWAYS, "Unable to resolve server host name");
    goto error;
  }

  //------------------------------------------
  // Setup the host server information
  //------------------------------------------
  if ((code = krb5_sname_to_principal(krb_context_, 
				      hp->h_name, 
				      STR_DEFAULT_CONDOR_SERVICE,
				      KRB5_NT_SRV_HST, 
				      &server_))) {
    goto error;
  }

  return TRUE;

 error:
  dprintf(D_ALWAYS, "%s\n", error_message(code));
  return FALSE;
}
//----------------------------------------------------------------------
// server_acquire_cred: Acquire service credential for server
//----------------------------------------------------------------------
int Authentication :: server_acquire_credential()
{
  // Note, this could be combinded with get_server_info
  int code;

  dprintf(D_ALWAYS, "Acquiring credential for server\n");
  //------------------------------------------
  // First, find out the principal mapping
  //------------------------------------------
  if ((code = krb5_sname_to_principal(krb_context_, 
				      NULL, 
				      STR_DEFAULT_CONDOR_SERVICE,
				      KRB5_NT_SRV_HST, 
				      &krb_principal_))) {
    dprintf(D_ALWAYS, "Failed to acquire server principal\n");
    goto error;
  }

  dprintf(D_ALWAYS, "Successfully acquired server principal\n");

  return TRUE;
error:
  dprintf(D_ALWAYS, "%s\n", error_message(code));
  return FALSE;
}

//----------------------------------------------------------------------
// daemon_acquire_credential:
//----------------------------------------------------------------------
int Authentication :: daemon_acquire_credential()
{
  int            code, rc = FALSE;
  priv_state     priv;
  krb5_ccache    ccache;
  krb5_creds     creds;
  krb5_keytab    keytab = 0;
  krb5_flags     flags;
  krb5_principal server;

  flags = AP_OPTS_MUTUAL_REQUIRED;
  memset(&creds, 0, sizeof(creds));

  dprintf(D_ALWAYS, "Acquiring credential for daemon\n");

  if (krb_principal_ == NULL) {
    //------------------------------------------
    // First, assuming that I am the default condor service
    //------------------------------------------
    if ((code = krb5_sname_to_principal(krb_context_, 
					NULL, 
					defaultCondor_,
					KRB5_NT_SRV_HST, 
					&krb_principal_)))
      goto error;
  }

  //----------------------------------------
  // Now, build TGT server information
  //----------------------------------------
  if (code = krb5_build_principal_ext(krb_context_, 
				      &server,
				      krb5_princ_realm(krb_context_, 
						       krb_principal_)->length,
				      krb5_princ_realm(krb_context_, 
						       krb_principal_)->data,
				      KRB5_TGS_NAME_SIZE, 
				      KRB5_TGS_NAME,
				      krb5_princ_realm(krb_context_, 
						       krb_principal_)->length,
				      krb5_princ_realm(krb_context_, 
						       krb_principal_)->data,
				      0)) {
    goto error;
  }


  //------------------------------------------
  // Copy the principal information 
  //------------------------------------------
  krb5_copy_principal(krb_context_, krb_principal_, &(creds.client));
  krb5_copy_principal(krb_context_, server_, &(creds.server));

  dprintf(D_ALWAYS, "Trying to get credential\n");

  if (keytabName_) {
    code = krb5_kt_resolve(krb_context_, keytabName_, &keytab);
  }
  else {
    code = krb5_kt_default(krb_context_, &keytab);
  }

  priv = set_root_priv();   // Get the old privilige
  
  if (code = krb5_get_in_tkt_with_keytab(krb_context_,
					 0,
					 NULL,      //adderss
					 NULL,
					 NULL,
					 keytab,
					 0,
					 &creds,
					 0)) {
    set_priv(priv);
    dprintf(D_ALWAYS, "Daemon failed to retrieve credential\n");
    goto error;
  }
  
  set_priv(priv);
  
  dprintf(D_ALWAYS, "Success..........................\n");
  
  if (krb5_cc_resolve(krb_context_, ccname_, &ccache)) {
    dprintf(D_ALWAYS, "Daemon failed to get default cache\n");
    return FALSE;
  }

  if (code = krb5_cc_initialize(krb_context_, ccache, krb_principal_)) {
    goto error;
  }
 
  if (code = krb5_cc_store_cred(krb_context_, ccache, &creds)) {
    goto error;
  }       
  rc = TRUE;
  goto cleanup;

 error:

  dprintf(D_ALWAYS, "%s\n", error_message(code));
  rc = FALSE;

 cleanup:

  krb5_cc_close(krb_context_, ccache);
  
  krb5_free_cred_contents(krb_context_, &creds);

  krb5_free_principal(krb_context_, server);

  if (keytab) {
    krb5_kt_close(krb_context_, keytab);   
  }

  return rc;
}
//----------------------------------------------------------------------
// client_acquire_credential
//----------------------------------------------------------------------
int Authentication::client_acquire_credential()
{
  int             rc = TRUE;
  krb5_error_code code;
  krb5_ccache     ccache = (krb5_ccache) NULL;

  dprintf(D_ALWAYS, "Acquiring credential for user\n");

  //------------------------------------------
  // First, try the default credential cache
  //------------------------------------------
  ccname_ = strdup(krb5_cc_default_name(krb_context_));

  if (code = krb5_cc_resolve(krb_context_, ccname_, &ccache)) {
    dprintf(D_ALWAYS, "Kerberos: Could not find default cache.");
    rc = FALSE;
    goto error;
  }

  //------------------------------------------
  // Get principal info
  //------------------------------------------
  if (code = krb5_cc_get_principal(krb_context_, ccache, &krb_principal_)) {
    dprintf(D_ALWAYS,"Kerberos : failure to get principal info");
    goto error;
  }
  
  krb5_cc_close(krb_context_, ccache);
  dprintf(D_ALWAYS, "Successfully located credential cache\n");

  return TRUE;
  
 error:
  dprintf(D_ALWAYS, "%s\n", error_message(code));
  if (ccache) {  // maybe should destroy this
    krb5_cc_close(krb_context_, ccache);
  }
  return FALSE;
}


//----------------------------------------------------------------------
// authenticate_kerberos
//----------------------------------------------------------------------
int Authentication::authenticate_kerberos() 
{
  //temporarily change timeout to 5 minutes so the user can type passwd
  //MUST do this even on server side, since client side might call
  
  int time = mySock->timeout(60 * 5); 
  int status = 0;          // Default is false
  char * principal = NULL;

  //------------------------------------------
  // First, initialize the context if necessary
  //------------------------------------------
  init_kerberos_context();

  if ( mySock->isClient() ) {
    initialize_client_data();
    //----------------------------------------
    // Check to see if client needs to use
    // special keytab instead of TGT
    //----------------------------------------
    keytabName_ = param(STR_CONDOR_CLIENT_KEYTAB);

    if (keytabName_) {
      if ((principal = param(STR_CONDOR_CLIENT_PRINCIPAL)) == NULL) {
	dprintf(D_ALWAYS, "Principal is not specified!\n");
	return 0;
      }

      // Convert from short name to Kerberos name
      krb5_sname_to_principal(krb_context_, 
			      NULL, 
			      principal, 
			      KRB5_NT_SRV_HST, 
			      &krb_principal_);

      ccname_  = (char *) malloc(MAXPATHLEN);
      sprintf(ccname_, STR_KRB_FORMAT, STR_DEFAULT_CACHE_DIR, principal);

      dprintf(D_ALWAYS, "About to authenticate using Keytab.\n");

      status = daemon_acquire_credential();
    }
    else {
      // Case 1: This is a daemon - daemon authentication
      if (isDaemon_ == TRUE) {
	ccname_  = (char *) malloc(MAXPATHLEN);
	sprintf(ccname_, STR_KRB_FORMAT, STR_DEFAULT_CACHE_DIR, STR_DEFAULT_CONDOR_USER);
	dprintf(D_ALWAYS, "About to authenticate daemon to daemon\n");
	status = daemon_acquire_credential();
      }
      else {
	// This is a user - daemon authentication
	dprintf(D_ALWAYS,"about to authenticate server using Kerberos\n" );
	
	status = client_acquire_credential();
      }
    }

    if (status) {
      status = authenticate_client_kerberos();
    }
  }
  else {
    keytabName_ = param(STR_CONDOR_SERVER_KEYTAB);
    dprintf(D_ALWAYS,"About to authenticate client using Kerberos\n" );
    status = authenticate_server_kerberos();
  }

  mySock->timeout(time); //put it back to what it was before
  
  return( status );
}


//----------------------------------------------------------------------
// map_kerberos_name
//----------------------------------------------------------------------
int Authentication::map_kerberos_name(krb5_ticket * ticket)
{
  krb5_error_code code;
  char *          client = NULL;
  int             rc = FALSE;

  //------------------------------------------
  // Decode the client name
  //------------------------------------------

  if (code = krb5_unparse_name(krb_context_, 
			       ticket->enc_part2->client, 
			       &client)){
    goto error;
  } 
  else {
    // We need to parse it right now. from userid@domain
    // to just user id
    char *tmp;
    tmp = strchr( client, '@' );
    int len = strlen(tmp);
    if (claimToBe) {
      free(claimToBe);
      claimToBe = NULL;
    }
    int size = strlen(client) - len + 1;
    claimToBe = (char *) malloc(size);
    memset(claimToBe, '\0', size);
    memcpy(claimToBe, client, size -1);
    
    //------------------------------------------
    // Now, map domain name
    //------------------------------------------
    if (code = krb5_get_default_realm(krb_context_, &domainName)) {
      goto error;
    }
    dprintf(D_ALWAYS, "Client is %s@%s\n", claimToBe, domainName);
  }

  rc = TRUE;
  goto cleanup;
  
 error:
  dprintf(D_ALWAYS, "%s\n", error_message(code));

 cleanup:
  if (client) {
    free(client);
  }
  return rc;
}

//----------------------------------------------------------------------
// init_kerberos_context
//----------------------------------------------------------------------
int Authentication :: init_kerberos_context()
{
  int retval = TRUE;

  // kerberos context_
  if (krb_context_ == NULL) {
    retval = (krb5_init_context(&krb_context_) == 0);
  }

  // stash location
  defaultStash_ = param(STR_CONDOR_CACHE_DIR);

  if (defaultStash_ == NULL) {
    defaultStash_ = param(STR_DEFAULT_CONDOR_SPOOL);
  }

  return retval;
}

//----------------------------------------------------------------------
// initialize_client_data
//----------------------------------------------------------------------
void Authentication :: initialize_client_data()
{
  // First, where to find cache
  //if (defaultStash_) {
  //  ccname_  = new char[MAXPATHLEN];
  //  sprintf(ccname_, STR_KRB_FORMAT, defaultStash_, my_username());
  //}
  //else {
  // Exception!
  //  dprintf(D_ALWAYS, "Unable to stash ticket -- STASH directory is not defined!\n");
  //}

  get_server_info();

  // Now, lets find out the default condor daemon user
  defaultCondor_ = param(STR_CONDOR_DAEMON_USER);

  // If default is not set in configuration file
  if (defaultCondor_ == NULL) {
    //defaultCondor_ = strdup(STR_DEFAULT_CONDOR_USER);
    defaultCondor_ = strdup(STR_DEFAULT_CONDOR_SERVICE);
  }
}

//----------------------------------------------------------------------0
// Forwarding tgt credentials
//----------------------------------------------------------------------
int Authentication :: forward_tgt_creds(krb5_auth_context auth_context, 
					krb5_creds *      cred,
					krb5_ccache       ccache)
{
  krb5_error_code  code;
  krb5_data        request;
  int              message, rc = 1;
  struct hostent * hp;

  hp = gethostbyaddr((char *) &(mySock->endpoint())->sin_addr, 		  
		     sizeof (struct in_addr), 
		     mySock->endpoint()->sin_family);

  if (code = krb5_fwd_tgt_creds(krb_context_, 
				auth_context,
				hp->h_name,
				cred->client, 
				cred->server,
				ccache, 
				KDC_OPT_FORWARDABLE,
				&request)) {
    dprintf(D_ALWAYS, "Error getting forwarded creds\n");
    goto error;
  }

  // Now, send it

  message = KERBEROS_FORWARD;
  mySock->encode();
  if ((!mySock->code(message)) || (!mySock->end_of_message())) {
    dprintf(D_ALWAYS, "Failed to send response\n");
    goto error;
  }

  rc = !(send_request(&request) == KERBEROS_GRANT);

  goto cleanup;

 error:
  dprintf(D_ALWAYS, "%s\n", error_message(code));

 cleanup:

  free(request.data);

  return rc;
}

//----------------------------------------------------------------------
// Receiving tgt credentials
//----------------------------------------------------------------------
int Authentication :: receive_tgt_creds(krb5_auth_context auth_context, 
					krb5_ticket     * ticket)
{
  krb5_error_code  code;
  krb5_ccache      ccache;
  krb5_principal   client;
  krb5_data        request;
  krb5_creds **    creds;
  char             defaultCCName[MAXPATHLEN+1];
  int              message;
  // First find out who we are talking to.
  // In case of host or condor, we do not need to receive credential

  // This is really ugly
  if ((strncmp(claimToBe, "host/"  , strlen("host/")) == 0) || 
      (strncmp(claimToBe, "condor/", strlen("condor/")) == 0)) {
    // Make the user condor, this is a hack for now. We need to 
    // create a condor service!
    free(claimToBe);
    claimToBe = strdup(STR_DEFAULT_CONDOR_USER);
  }
  else {
    if (defaultStash_) {
      sprintf(defaultCCName, STR_KRB_FORMAT, defaultStash_, claimToBe);
      dprintf(D_ALWAYS, defaultCCName);
      
      // First, check to see if we have a stash ticket
      if (code = krb5_cc_resolve(krb_context_, defaultCCName, &ccache)) {
	goto error;
      }
      
      // A very weak assumption that client == ticket->enc_part2->client
      // But this is what I am going to do right now
      if (code = krb5_cc_get_principal(krb_context_, ccache, &client)) {
	// We need use to forward credential
	message = KERBEROS_FORWARD;
	
	mySock->encode();
	if ((!mySock->code(message)) || (!mySock->end_of_message())) {
	  dprintf(D_ALWAYS, "Failed to send response\n");
	  return 1;
	}
	
	// Expecting KERBEROS_FORWARD as well
	mySock->decode();
	
	if ((!mySock->code(message)) || (!mySock->end_of_message())) {
	  dprintf(D_ALWAYS, "Failed to receive response\n");
	  return 1;
	}
	
	if (message != KERBEROS_FORWARD) {
	  dprintf(D_ALWAYS, "Client did not send forwarding message!\n");
	  return 1;
	}
	else {
	  if (!mySock->code(request.length)) {
	    dprintf(D_ALWAYS, "Credential length is invalid!\n");
	    return 1;
	  }
	  
	  request.data = (char *) malloc(request.length);
	  
	  if ((!mySock->get_bytes(request.data, request.length)) ||
	      (!mySock->end_of_message())) {
	    dprintf(D_ALWAYS, "Credential is invalid!\n");
	    return 1;
	  }
	  
	// Technically spearking, we have the credential now
	  if (code = krb5_rd_cred(krb_context_, 
				  auth_context, 
				  &request, 
				  &creds, 
				  NULL)) {
	    goto error;
	  }
	  
	  // Now, try to store it
	  if (code = krb5_cc_initialize(krb_context_, 
					ccache, 
					ticket->enc_part2->client)) {
	    goto error;
	  }
	  
	  if (code = krb5_cc_store_cred(krb_context_, ccache, *creds)) {
	    goto error;
	  }
	  
	  // free the stuff
	  krb5_free_creds(krb_context_, *creds);
	  
	  free(request.data);
	}
      }
      
      krb5_cc_close(krb_context_, ccache);
      
    }
    else {
      dprintf(D_ALWAYS, "Stash location is not set!\n");
      goto error;
    }
  }

  //------------------------------------------
  // Tell the other side the good news
  //------------------------------------------
  message = KERBEROS_GRANT;
  
  mySock->encode();
  if ((!mySock->code(message)) || (!mySock->end_of_message())) {
    dprintf(D_ALWAYS, "Failed to send response\n");
    return 1;
  }
  
  return 0;  // Everything is fine
  
 error:
  dprintf(D_ALWAYS, "%s\n", error_message(code));
  //if (ccache) {
  //  krb5_cc_destroy(krb_context_, ccache);
  //}

  return 1;
}

//----------------------------------------------------------------------
// Authentication :: read_request
//----------------------------------------------------------------------
int Authentication :: read_request(krb5_data * request) 
{
  int code = TRUE;

  mySock->decode();

  if (!mySock->code(request->length)) {
    dprintf(D_ALWAYS, "Incorrect message 1!\n");
    code = FALSE;
  }
  else {
    request->data = (char *) malloc(request->length);

    if ((!mySock->get_bytes(request->data, request->length)) ||
	(!mySock->end_of_message())) {
      dprintf(D_ALWAYS, "Incorrect message 2!\n");
      code = FALSE;
    }
  }

  return code;
}

//----------------------------------------------------------------------
// Authentication :: send_request
//----------------------------------------------------------------------
int Authentication :: send_request(krb5_data * request)
{
  int reply = KERBEROS_DENY;
  //------------------------------------------
  // Send the AP_REQ object
  //------------------------------------------
  mySock->encode();

  //fprintf(stderr, "size of the message is %d:\n", request->length);

  if (!mySock->code(request->length)) {
    dprintf(D_ALWAYS, "Faile to send request length\n");
    return reply;
  }

  if (!(mySock->put_bytes(request->data, request->length)) ||
      !(mySock->end_of_message())) {
    dprintf(D_ALWAYS, "Faile to send request data\n");
    return reply;
  }

  //------------------------------------------
  // Next, wait for response
  //------------------------------------------
  mySock->decode();

  if ((!mySock->code(reply)) || (!mySock->end_of_message())) {
    dprintf(D_ALWAYS, "Failed to receive response from server\n");
    return KERBEROS_DENY;
  }// Resturn buffer size

  return reply;
}

//----------------------------------------------------------------------
// getRemoteHost -- retrieve remote host's address
//----------------------------------------------------------------------
void Authentication :: getRemoteHost(krb5_auth_context auth) 
{
  krb5_address  ** localAddr  = NULL;
  krb5_address  ** remoteAddr = NULL;

  // Get remote host's address first

  krb5_auth_con_getaddrs(krb_context_, auth, localAddr, remoteAddr);

  if (remoteAddr) {
    struct in_addr in;
    memcpy(&(in.s_addr), (*remoteAddr)[0].contents, sizeof(in_addr));
  }

  if (localAddr) {
    krb5_free_addresses(krb_context_, localAddr);
  }

  if (remoteAddr) {
    krb5_free_addresses(krb_context_, remoteAddr);
  }

  dprintf(D_ALWAYS, "Remote host is %s\n", remotehost_);
}

//----------------------------------------------------------------------
// No idea what this method is for yet
//----------------------------------------------------------------------
bool Authentication :: kerberos_is_valid()
{
  return (auth_status == CAUTH_KERBEROS);
}

//----------------------------------------------------------------------
// kerberos_wrap: encryption function using Kerberos
//----------------------------------------------------------------------
int Authentication :: kerberos_wrap(char*  input, 
				    int    input_len, 
				    char*& output,
				    int&   output_len)
{
  krb5_error_code code;
  krb5_data       in_data, test;
  krb5_enc_data   out_data;
  krb5_pointer    ivec;
  int             index;
  
  //fprintf(stderr, "Encrypting using Kerberos\n");
  // Make the input buffer
  in_data.length = input_len;
  in_data.data = (char *) malloc(input_len);
  memcpy(in_data.data, input, input_len);
  
  if (code = krb5_encrypt_data(krb_context_, 
			       sessionKey_, 
			       0, 
			       &in_data, 
			       &out_data)) {
    output_len = 0;
    goto error;
  }

  output_len = sizeof(out_data.enctype) +
               sizeof(out_data.kvno)    + 
               sizeof(out_data.ciphertext.length) +
               out_data.ciphertext.length;
  
  output = (char *) malloc(output_len);
  index = 0;
  memcpy(output + index, &(out_data.enctype), sizeof(out_data.enctype));
  index += sizeof(out_data.enctype);
  memcpy(output + index, &(out_data.kvno), sizeof(out_data.kvno));
  index += sizeof(out_data.kvno);
  memcpy(output + index, &(out_data.ciphertext.length), sizeof(out_data.ciphertext.length));
  index += sizeof(out_data.ciphertext.length);
  memcpy(output + index, out_data.ciphertext.data, out_data.ciphertext.length);

  //fprintf(stderr, "Origional text: %s, encrypted text: %24s\n", input, out_data.ciphertext.data);

  if (in_data.data) {
    free(in_data.data);
  }

  return TRUE;
 error:
  dprintf(D_ALWAYS, "%s\n", error_message(code));
  return FALSE;
}

//----------------------------------------------------------------------
// kerberos_unwrap: decryption function using Kerberos
//----------------------------------------------------------------------
int Authentication :: kerberos_unwrap(char*  input, 
				      int    input_len, 
				      char*& output, 
				      int&   output_len)
{
  int code;
  krb5_data     out_data;
  krb5_enc_data enc_data;

  int index = 0;
  memcpy(&(enc_data.enctype), input, sizeof(enc_data.enctype));
  index += sizeof(enc_data.enctype);
  memcpy(&(enc_data.kvno), input + index, sizeof(enc_data.kvno));
  index += sizeof(enc_data.kvno);
  memcpy(&(enc_data.ciphertext.length), input + index, sizeof(enc_data.ciphertext.length));
  index += sizeof(enc_data.ciphertext.length);

  enc_data.ciphertext.data = (char *) malloc(enc_data.ciphertext.length);

  memcpy(enc_data.ciphertext.data, input + index, enc_data.ciphertext.length);

  //fprintf(stderr, "About to decrypt %s\n", enc_data.ciphertext.data);

  if (code = krb5_decrypt_data(krb_context_, 
			       sessionKey_,
			       0, 
			       &enc_data, 
			       &out_data)) {
    output_len = 0;
    goto error;
  }
  //fprintf(stderr, "Decrypted\n");

  output_len = out_data.length;
  output = (char *) malloc(output_len);
  memcpy(output, out_data.data, output_len);
  //fprintf(stderr, "Encrypted text: %s, decrypted text: %s\n", input, output);

  if (enc_data.ciphertext.data) {
    free(enc_data.ciphertext.data);
  }

  return TRUE;
 error:
  dprintf(D_ALWAYS, "%s\n", error_message(code));
  return FALSE;
}

#endif /* defined(KERBEROS_AUTHENTICATION) */

#endif /* !defined(SKIP_AUTHENTICATION) */










