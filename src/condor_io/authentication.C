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
#if !defined(SKIP_AUTHENTICATION)
#   include "condor_debug.h"
#   include "condor_string.h"
#   include "condor_config.h"
#   include "internet.h"
#   include "condor_uid.h"
#   include "my_username.h"
#   include "string_list.h"
#endif /* !defined(SKIP_AUTHENTICATION) */

#if defined(GSS_AUTHENTICATION)
gss_cred_id_t Authentication::credential_handle = GSS_C_NO_CREDENTIAL;
#endif defined(GSS_AUTHENTICATION)

#if !defined(SKIP_AUTHENTICATION) && defined(WIN32)
extern HINSTANCE _condor_hSecDll;	// handle to SECURITY.DLL in sock.C
PSecurityFunctionTable Authentication::pf = NULL;
#endif

Authentication::Authentication( ReliSock *sock )
{
#if !defined(SKIP_AUTHENTICATION)
	//credential_handle is static and set at top of this file//
	mySock = sock;
	auth_status = CAUTH_NONE;
	claimToBe = NULL;
	GSSClientname = NULL;
	authComms.buffer = NULL;
	authComms.size = 0;
	canUseFlags = CAUTH_NONE;
	serverShouldTry = NULL;
	RendezvousDirectory = NULL;
#endif
}

Authentication::~Authentication()
{
#if !defined(SKIP_AUTHENTICATION)
	mySock = NULL;

	if ( claimToBe ) {
		delete [] claimToBe;
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
#endif
}

int
Authentication::authenticate( char *hostAddr )
{
#if defined(SKIP_AUTHENTICATION)
	return 0;
#else

		//call setupEnv if Server or if Client doesn't know which methods to try
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
		switch ( firm ) {
#if defined(GSS_AUTHENTICATION)
		 case CAUTH_GSS:
			authenticate_self_gss();
			if( authenticate_gss() ) {
				auth_status = CAUTH_GSS;
			}
			break;
#endif /* GSS_AUTHENTICATION */

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
		delete [] claimToBe;
		claimToBe = NULL;
	}
	if ( owner ) {
		claimToBe = strnewp( owner );
	}
	return 1;
#endif /* SKIP_AUTHENTICATION */
}

const char *
Authentication::getOwner() 
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

#if !defined(SKIP_AUTHENTICATION)

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
		//if cert not in ENV, and not in config file, don't try GSS auth
	if ( getenv( "X509_USER_CERT" ) ) {
			//simply assume that if USER_CERT is specified, it's all set up.
		tryGss = 1;
	}
	else {
			//try env first (some progs might have it set), else param for it
		if ( ( pbuf = getenv( "X509_DIRECTORY" ) )
				|| ( needfree = 1, pbuf = param( "X509_DIRECTORY" ) ) )
		{
			tryGss = 1;

			//set defaults for CERT_DIR, USER_CERT, and USER_KEY if not in ENV
			sprintf( buffer, "X509_CERT_DIR=%s/certdir", pbuf );
			putenv( strdup( buffer ) );

			sprintf( buffer, "X509_USER_CERT=%s/usercert.pem", pbuf );
			putenv( strdup( buffer ) );

			sprintf(buffer,"X509_USER_KEY=%s/userkey.pem", pbuf );
			putenv( strdup( buffer ) );

			sprintf(buffer,"SSLEAY_CONF=%s/condor_ssl.cnf", pbuf );
			putenv( strdup( buffer ) );

			//don't need to do anything with X509_USER_PROXY, submit should 
			//have put that into the environment already...

			if ( needfree ) {
				free( pbuf );
			}
		}
	}

	if ( mySock->isClient() ) {
			//client needs to know name of the schedd, I stashed hostAddr in 
			//applicable ReliSock::ReliSock() or ReliSock::connect(), which 
			//should only be called by clients. 
		sockaddr_in sin;
		char *hostname;

		if ( hostAddr[0] != '<' ) { //not already sinful
			if ( strchr( hostAddr, ':' ) ) { //already has port info
				sprintf( buffer, "<%s>", hostAddr );
			}
			else {
				//add a bogus port number (0) because we just want hostname
				sprintf( buffer, "<%s:0>", hostAddr );
			}
		}
		else {
			strcpy( buffer, hostAddr );
		}
		
		if ( string_to_sin( buffer, &sin ) 
				&& ( hostname = sin_to_hostname( &sin, NULL ) ) ) 
		{
			sprintf( buffer, "CONDOR_GATEKEEPER=/CN=schedd@%s", hostname );
			putenv( strdup( buffer ) );
		}
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
		if ( tryGss ) {
			canUseFlags |= CAUTH_GSS;
		}

#endif defined WIN32
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
		if ( ( clientCanUse & CAUTH_GSS ) && !stricmp( tmp, "GSS" ) ) {
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
	int time = mySock->timeout(60 * 5);  //allow user 5 min to type passwd

	major_status = globus_gss_assist_acquire_cred(&minor_status,
		GSS_C_BOTH, &credential_handle);

	mySock->timeout(time); //put it back to what it was before

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

int 
Authentication::authenticate_client_gss()
{
	OM_uint32						  major_status = 0;
	OM_uint32						  minor_status = 0;
	int								  token_status = 0;
	OM_uint32						  ret_flags = 0;
	gss_ctx_id_t					  context_handle = GSS_C_NO_CONTEXT;

	if ( !authenticate_self_gss() ) {
		dprintf( D_ALWAYS, 
			"failure authenticating client from auth_connection_client\n" );
		return FALSE;
	}
	 
	char *gateKeeper = getenv( "CONDOR_GATEKEEPER" );

	if ( !gateKeeper ) {
		dprintf( D_ALWAYS, "env var CONDOR_GATEKEEPER not set" );
		return FALSE;
	}

	authComms.sock = mySock;
	authComms.buffer = NULL;
	authComms.size = 0;

	major_status = globus_gss_assist_init_sec_context(&minor_status,
		  credential_handle, &context_handle,
		  gateKeeper,
		  GSS_C_DELEG_FLAG|GSS_C_MUTUAL_FLAG,
		  &ret_flags, &token_status,
		  authsock_get, (void *) &authComms,
		  authsock_put, (void *) mySock
	);

	if (major_status != GSS_S_COMPLETE)
	{
		dprintf( D_ALWAYS, "failed auth connection client, gss status: 0x%x\n",
								major_status );
		delete gateKeeper;
		gateKeeper = NULL;
		return FALSE;
	}

	/* 
	 * once connection is Authenticated, don't need sec_context any more
	 * ???might need sec_context for PROXIES???
	 */
	gss_delete_sec_context( &minor_status, &context_handle, GSS_C_NO_BUFFER );

	auth_status = CAUTH_GSS;

	dprintf(D_FULLDEBUG,"valid GSS connection established to %s\n", gateKeeper);
	return TRUE;
}

int 
Authentication::authenticate_server_gss()
{
	int rc;
	OM_uint32 major_status = 0;
	OM_uint32 minor_status = 0;
	int		 token_status = 0;
	OM_uint32 ret_flags = 0;
	gss_ctx_id_t context_handle = GSS_C_NO_CONTEXT;

	if ( !authenticate_self_gss() ) {
		dprintf( D_ALWAYS, 
			"failure authenticating self in auth_connection_server\n" );
		return FALSE;
	}
	 
	authComms.sock = mySock;
	authComms.buffer = NULL;
	authComms.size = 0;

	major_status = globus_gss_assist_accept_sec_context(&minor_status,
				 &context_handle, credential_handle,
				 &GSSClientname,
				 &ret_flags, NULL, /* don't need user_to_user */
				 &token_status,
				 NULL,             /* don't delegate credential */
				 authsock_get, (void *) &authComms,
				 authsock_put, (void *) mySock
	);


	if ( (major_status != GSS_S_COMPLETE) ||
			( lookup_user_gss( GSSClientname ) < 0 ) ) 
	{
		if (major_status != GSS_S_COMPLETE) {
			dprintf(D_ALWAYS, "server: GSS failure authenticating %s 0x%x\n",
					"client, status: ", major_status );
		}
		else {
			dprintf( D_ALWAYS, "server: user lookup failure.\n" );
		}
		return FALSE;
	}

	/* 
	 * once connection is Authenticated, don't need sec_context any more
	 * ???might need sec_context for PROXIES???
	 */
	gss_delete_sec_context( &minor_status, &context_handle, GSS_C_NO_BUFFER );

	if ( !nameGssToLocal() ) {
		return( FALSE );
	}

	auth_status = CAUTH_GSS;

	dprintf(D_FULLDEBUG,"valid GSS connection established to %s\n", 
			GSSClientname);
	return TRUE;
}

int
Authentication::nameGssToLocal() {
	//this might need to change with SSLK5 stuff

	//just extract username from /CN=<username>@<domain,etc>
	if ( !GSSClientname ) {
		return 0;
	}

	char *tmp;
	tmp = strchr( GSSClientname, '=' );
	char tmp2[512];

	if ( tmp ) {
		tmp++;
		sprintf( tmp2, "%*.*s", strcspn( tmp, "@" ),
				strcspn( tmp, "@" ), tmp );
		setOwner( tmp2 );
	}
	else {
		setOwner( GSSClientname );
	}
	return 1;
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
				dprintf(D_FULLDEBUG,"about to authenticate server from client\n" );
				status = authenticate_client_gss();
				break;
			default: 
				dprintf(D_FULLDEBUG,"about to authenticate client from server\n" );
				status = authenticate_server_gss();
				break;
		}
		mySock->timeout(time); //put it back to what it was before
	}

	return( status );
}

#endif defined(GSS_AUTHENTICATION)

#endif !defined(SKIP_AUTHENTICATION)
