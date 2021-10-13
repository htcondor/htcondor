/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


#include "condor_common.h"
#include "CondorError.h"

#if !defined(SKIP_AUTHENTICATION) && defined(WIN32)

#include "condor_auth_sspi.h"

extern HINSTANCE _condor_hSecDll;	// handle to SECURITY.DLL in sock.C
PSecurityFunctionTable Condor_Auth_SSPI::pf = NULL;


Condor_Auth_SSPI :: Condor_Auth_SSPI(ReliSock * sock)
    : Condor_Auth_Base(sock, CAUTH_NTSSPI)
{
	theCtx.dwLower = 0L;
	theCtx.dwUpper = 0L;
}

Condor_Auth_SSPI :: ~Condor_Auth_SSPI()
{
	if ( theCtx.dwLower != 0L || theCtx.dwUpper != 0L )
		(pf->DeleteSecurityContext)( &theCtx );
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

	char buf[256];
	char *dom = NULL;
	DWORD bufsiz = sizeof buf;

    if ( rc != SEC_E_OK ) {
        dprintf( D_ALWAYS,
                 "sspi_server_auth(): Failed to impersonate (returns %d)!\n",
					 rc );
    } else {

		// PLEASE READ: We're now running in the context of the
		// client we're impersonating. This means dprintf()'ing is
		// OFF LIMITS until we RevertSecurityContext(), since the
		// dprintf() will likely fail because the client 
		// probably will not have write access to the log file.

        GetUserName( buf, &bufsiz );
		dom = my_domainname();

		// revert as soon as possible.
	   	(pf->RevertSecurityContext)( &srvCtx );

		// Ok, safe to dprintf() now...

		it_worked = TRUE;

		setRemoteUser(buf);
		setRemoteDomain(dom);

		// set authenticated name used for mapping
		std::string auth_name;
		auth_name = buf;
		if(dom) {
			auth_name += "@";
			auth_name += dom;
		}
		setAuthenticatedName(auth_name.c_str());

		dprintf( D_FULLDEBUG, "sspi_server_auth(): user name is: \"%s\"\n", buf );
		if (dom) {
			dprintf( D_FULLDEBUG, "sspi_server_auth(): domain name is: \"%s\"\n", dom);
			free(dom);
		}
    }

    (pf->FreeContextBuffer)( secPackInfo );

    dprintf( D_FULLDEBUG,"sspi_server_auth() exiting\n" );

    // return success (1) or failure (0) 
	return it_worked;
}

int 
Condor_Auth_SSPI::wrap(const char *   input,
                             int      input_len, 
                             char*&   output, 
                             int&     output_len)
{
	if ( !input || !input_len ) {
		return FALSE;
	}

	if ( !isValid() ) {
		return FALSE;
	}

	const void* pvMessage = input;
	DWORD cbMessage = input_len;
	void** ppvSealedMessage = (void**)&output;

	// see how much extra space the SSP needs
    SecPkgContext_Sizes sizes;
    SECURITY_STATUS err = 
        (pf->QueryContextAttributes)(&theCtx,
                                        SECPKG_ATTR_SIZES,
                                        &sizes);

    // allocate a buffer and copy plaintext into it, since 
	// the encryption must happen in place
    *ppvSealedMessage = malloc(cbMessage +
                               sizes.cbSecurityTrailer);
    CopyMemory(*ppvSealedMessage, pvMessage, cbMessage);

    // describe our buffer for SSPI
    SecBuffer rgsb[] = {
        {cbMessage, SECBUFFER_DATA, *ppvSealedMessage},
        {sizes.cbSecurityTrailer, SECBUFFER_TOKEN,
            static_cast<BYTE*>(*ppvSealedMessage) +
            cbMessage},
    };
    SecBufferDesc sbd = {SECBUFFER_VERSION,
                         sizeof rgsb / sizeof *rgsb, rgsb};
    // encrypt in place
    err = (pf->EncryptMessage)(&theCtx, 0, &sbd, 0);
    bool bOk = true;
    if (err) {
        bOk = false;
        free(*ppvSealedMessage);
        *ppvSealedMessage = 0;
    }
    output_len = cbMessage + rgsb[1].cbBuffer;
	dprintf(D_SECURITY,
		"Condor_Auth_SSPI::wrap() - input_len=%d output_len=%d\n",
		input_len,output_len);
    return bOk ? TRUE : FALSE;
}

int 
Condor_Auth_SSPI::unwrap(const char *   input,
                             int      input_len, 
                             char*&   output, 
                             int&     output_len)
{
	if ( !input || !input_len ) {
		return FALSE;
	}

	if ( !isValid() ) {
		return FALSE;
	}

	const void* pvSealedMessage = input;
	DWORD cbSealedMessage = input_len;
	ASSERT(input_len > 16);
	DWORD cbMessage = input_len - 16;	// 16 should not be hard-coded!
	output_len = input_len - 16;
	void** ppvMessage = (void**)&output;

	// allocate a buffer and copy ciphertext into it
    *ppvMessage = malloc(cbSealedMessage);
    CopyMemory(*ppvMessage,
               pvSealedMessage,
               cbSealedMessage);
    const DWORD cbTrailer = cbSealedMessage - cbMessage;

    // describe our buffer to SSPI
    SecBuffer rgsb[] = {
        {cbMessage, SECBUFFER_DATA, *ppvMessage},
        {cbTrailer, SECBUFFER_TOKEN,
         static_cast<BYTE*>(*ppvMessage) + cbMessage},
    };
    SecBufferDesc sbd = {SECBUFFER_VERSION,
                         sizeof rgsb / sizeof *rgsb,
                         rgsb};
    // decrypt in place
    SECURITY_STATUS err =
       (pf->DecryptMessage)(&theCtx, &sbd, 0, 0);
    if (err) {
        free(*ppvMessage);
        *ppvMessage = 0;
		output_len = 0;
		dprintf(D_ALWAYS,"Condor_Auth_SSPI::unwrap() failed!\n");
		return FALSE;
    }
	return TRUE;
}

int
Condor_Auth_SSPI::authenticate(const char * remoteHost, CondorError* errstack, bool /*non_blocking*/)
{
	int ret_value;
	CredHandle cred;

	cred.dwLower = 0L;
	cred.dwUpper = 0L;

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

	if ( theCtx.dwLower != 0L || theCtx.dwUpper != 0L )
		(pf->DeleteSecurityContext)( &theCtx );
	theCtx.dwLower = 0L;
	theCtx.dwUpper = 0L;


	if ( mySock_->isClient() ) {
		//client authentication
		
		ret_value = sspi_client_auth(cred,theCtx,NULL);
	}
	else {
		//server authentication

		ret_value = sspi_server_auth(cred,theCtx);
	}

	// clean up
	if ( cred.dwLower != 0L || cred.dwUpper != 0L )
		(pf->FreeCredentialHandle)( &cred );

	//return 1 for success, 0 for failure. Server should send sucess/failure
	//back to client so client can know what to return.
	
	if ( ret_value == 0 ) {
		// if we failed, the security context is useless to us for wrapping, 
		// get rid of it.
		if ( theCtx.dwLower != 0L || theCtx.dwUpper != 0L )
			(pf->DeleteSecurityContext)( &theCtx );
		theCtx.dwLower = 0L;
		theCtx.dwUpper = 0L;
	}

	return ret_value;
}

int Condor_Auth_SSPI :: isValid() const
{
	if ( theCtx.dwLower != 0L || theCtx.dwUpper != 0L ) {
		return TRUE;
	} else {
		return FALSE;
	}
}

#endif	// of if defined(WIN32)
