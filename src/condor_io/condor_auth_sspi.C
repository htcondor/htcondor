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

#if !defined(SKIP_AUTHENTICATION) && defined(WIN32)

#include "condor_auth_sspi.h"

extern HINSTANCE _condor_hSecDll;	// handle to SECURITY.DLL in sock.C
PSecurityFunctionTable Condor_Auth_SSPI::pf = NULL;


Condor_Auth_SSPI :: Condor_Auth_SSPI(ReliSock * sock)
    : Condor_Auth_Base(sock, CAUTH_NTSSPI)
{
}

Condor_Auth_SSPI :: ~Condor_Auth_SSPI()
{
}

int 
Condor_Auth_SSPI :: sspi_client_auth( CredHandle&    cred, 
                                      CtxtHandle&    cliCtx, 
                                      const char *   tokenSource )
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
    
    while ( 1 )	{

        obd.ulVersion = SECBUFFER_VERSION;
        obd.cBuffers = 1;
        obd.pBuffers = &ob; // just one buffer
        ob.BufferType = SECBUFFER_TOKEN; // preping a token here
        ob.cbBuffer = secPackInfo->cbMaxToken;
        ob.pvBuffer = LocalAlloc( 0, ob.cbBuffer );
            
        rc = (pf->InitializeSecurityContext)( &cred, 
                                              haveContext? &cliCtx: NULL,
                                              myTokenSource, 
                                              ctxReq, 
                                              0, 
                                              SECURITY_NATIVE_DREP, 
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
                mySock_->encode();
                
                // send the size of the blob, then the blob data itself
                if ( !mySock_->code(ob.cbBuffer) || 
                     !mySock_->end_of_message() ||
                     !mySock_->put_bytes((const char *) ob.pvBuffer, ob.cbBuffer) ||
                     !mySock_->end_of_message() ) {
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
        mySock_->decode();
        if ( !mySock_->code(ib.cbBuffer) ||
             !mySock_->end_of_message() ||
             !(ib.pvBuffer = LocalAlloc( 0, ib.cbBuffer )) ||
             !mySock_->get_bytes( (char *) ib.pvBuffer, ib.cbBuffer ) ||
             !mySock_->end_of_message() ) 
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
Condor_Auth_SSPI::sspi_server_auth(CredHandle& cred,CtxtHandle& srvCtx)
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
            mySock_->decode();
            if ( !mySock_->code(ib.cbBuffer) ||
                 !mySock_->end_of_message() ||
                 !(ib.pvBuffer = LocalAlloc( 0, ib.cbBuffer )) ||
                 !mySock_->get_bytes((char *) ib.pvBuffer, ib.cbBuffer) ||
                 !mySock_->end_of_message() )
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
            
            rc = (pf->AcceptSecurityContext)( &cred, 
                                              haveContext? &srvCtx: NULL,
                                              &ibd, 
                                              0, 
                                              SECURITY_NATIVE_DREP, 
                                              &srvCtx, &obd, &ctxAttr,
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
                    mySock_->encode();
                    if ( !mySock_->code(ob.cbBuffer) ||
                         !mySock_->end_of_message() ||
                         !mySock_->put_bytes( (const char *) ob.pvBuffer, ob.cbBuffer ) ||
                         !mySock_->end_of_message() ) 
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
        setRemoteUser(buf);
        it_worked = TRUE;
        (pf->RevertSecurityContext)( &srvCtx );
    }
    
    (pf->FreeContextBuffer)( secPackInfo );
    
    dprintf( D_FULLDEBUG,"sspi_server_auth() exiting\n" );
    
    // return success (1) or failure (0)
    return it_worked;
}

int
Condor_Auth_SSPI::authenticate(const char * remoteHost)
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

	if ( mySock_->isClient() ) {
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

int Condor_Auth_SSPI :: isValid() const
{
    return TRUE;
}

#endif	// of if defined(WIN32)
