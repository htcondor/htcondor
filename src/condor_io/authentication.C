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

#include "condor_auth.h"
#include "condor_auth_claim.h"
#include "condor_auth_fs.h"
#include "condor_auth_sspi.h"
#include "condor_auth_x509.h"
#include "condor_auth_kerberos.h"

#if !defined(SKIP_AUTHENTICATION)
#   include "condor_debug.h"
#   include "condor_config.h"
#   include "string_list.h"
#endif /* !defined(SKIP_AUTHENTICATION) */

//----------------------------------------------------------------------

Authentication::Authentication( ReliSock *sock )
{
#if !defined(SKIP_AUTHENTICATION)
	mySock              = sock;
	auth_status         = CAUTH_NONE;
	canUseFlags         = CAUTH_NONE;
	serverShouldTry     = NULL;
	t_mode              = NORMAL;
        authenticator_      = NULL;
#endif
}

Authentication::~Authentication()
{
#if !defined(SKIP_AUTHENTICATION)
	mySock = NULL;

	if ( serverShouldTry ) {
	  free( serverShouldTry );
	  serverShouldTry = NULL;
	}

        delete authenticator_;
#endif
}

int Authentication::authenticate( char *hostAddr, KeyInfo *& key, int clientFlags)
{
    int retval = authenticate(hostAddr, clientFlags);
    
#if !defined(SKIP_AUTHENTICATION)
    if (retval) {        // will always try to exchange key!
        // This is a hack for now, when we have only one authenticate method
        // this will be gone
        mySock->allow_empty_message_flag = FALSE;
        retval = exchangeKey(key);
        mySock->allow_one_empty_message();
    }
#endif
    return retval;
}

int Authentication::authenticate( char *hostAddr, int clientFlags )
{
#if defined(SKIP_AUTHENTICATION)
    dprintf(D_ALWAYS, "Skipping....\n");
  return 0;
#else
  Condor_Auth_Base * auth = NULL;

  if (hostAddr) {
	dprintf ( D_SECURITY, "AUTHENTICATE: in authenticate( char *addr == '%s', int flags == %i)\n",
		  hostAddr, clientFlags);
  } else {
	dprintf ( D_SECURITY, "AUTHENTICATE: in authenticate( char *addr == NULL, int flags == %i)\n",
		  clientFlags);
  }
  
  int clientCanUse = clientFlags;
  //call setupEnv if Server or if Client doesn't know which methods to try
  // if ( !mySock->isClient() && !isServer) 
  // interesting, I am not sure whether this is right or not

  if ( !mySock->isClient() || !canUseFlags ) {
      setupEnv( hostAddr );
  }
  
  auth_status = CAUTH_NONE;
  
  while (auth_status == CAUTH_NONE ) {
    int firm = handshake(clientCanUse);
    if ( firm < 0 ) {
      dprintf(D_ALWAYS, "handshake failed!\n");
      break;
    } else {
      dprintf(D_SECURITY, "Trying to use %d\n", firm);
    }

    switch ( firm ) {
#if defined(GSS_AUTHENTICATION)
    case CAUTH_GSS:
        auth = new Condor_Auth_X509(mySock);
      break;
#endif /* GSS_AUTHENTICATION */

#if defined(KERBEROS_AUTHENTICATION) 
    case CAUTH_KERBEROS:
        auth = new Condor_Auth_Kerberos(mySock);
      break;
#endif
  
#if defined(WIN32)
    case CAUTH_NTSSPI:
        auth = new Condor_Auth_SSPI(mySock);
      break;
#else
    case CAUTH_FILESYSTEM:
        auth = new Condor_Auth_FS(mySock);
      break;
    case CAUTH_FILESYSTEM_REMOTE:
        auth = new Condor_Auth_FS(mySock, 1);
      break;
#endif /* !defined(WIN32) */
    case CAUTH_CLAIMTOBE:
        auth = new Condor_Auth_Claim(mySock);
      break;
      
    default:
      dprintf(D_ALWAYS,"Authentication::authenticate-- bad handshake FAILURE\n" );
      return 0;
    }

    //------------------------------------------
    // Now authenticate
    //------------------------------------------
    if (!auth->authenticate(hostAddr) ) {
        delete auth;

        //if authentication failed, try again after removing from client tries
        if ( mySock->isClient() ) {
            canUseFlags &= ~firm;
        }
    }
    else {
        authenticator_ = auth;
        auth_status = authenticator_->getMode();
    }
    clientCanUse = 0;
  }

  //if none of the methods succeeded, we fall thru to default "none" from above
  int retval = ( auth_status != CAUTH_NONE );
  dprintf(D_FULLDEBUG, "Authentication::authenticate %s\n", 
	  retval == 1 ? "Success" : "FAILURE" );

  mySock->allow_one_empty_message();
  return ( retval );
#endif /* SKIP_AUTHENTICATION */
}


int Authentication::isAuthenticated() 
{
#if defined(SKIP_AUTHENTICATION)
    return 0;
#else
    return( auth_status != CAUTH_NONE );
#endif
}


void Authentication::unAuthenticate()
{
#if !defined(SKIP_AUTHENTICATION)
    auth_status = CAUTH_NONE;
#endif
}


void Authentication::setAuthAny()
{
#if !defined(SKIP_AUTHENTICATION)
    canUseFlags = CAUTH_ANY;
#endif
}

int Authentication::setOwner( const char *owner ) 
{
#if defined(SKIP_AUTHENTICATION)
	return 0;
#else
	if ( !this ) {
            return 0;
	}
	if ( authenticator_ ) {
            authenticator_->setRemoteUser(owner);
            return 1;
	}
        else {
            return 0;
        }
#endif /* SKIP_AUTHENTICATION */
}

//----------------------------------------------------------------------
// getRemoteAddress: after authentication, return the remote address
//----------------------------------------------------------------------
const char * Authentication :: getRemoteAddress() const
{
#if defined(SKIP_AUTHENTICATION)
    return NULL;
#else
    // If we are not using Kerberos
    if (authenticator_) {
        return authenticator_->getRemoteHost();
    }
    else {
        return NULL;
    }
#endif
}

//----------------------------------------------------------------------
// getDomain:: return domain information
//----------------------------------------------------------------------
const char * Authentication :: getDomain() const
{
#if defined(SKIP_AUTHENTICATION)
    return NULL;
#else
    if (authenticator_) {
        return authenticator_->getRemoteDomain();
    }
    else {
        return NULL;
    }
#endif
}

const char * Authentication::getOwner() const
{
#if defined(SKIP_AUTHENTICATION)
    return NULL;
#else
    // Since we never use getOwner() like it allocates memory
    // anywhere in the code, it shouldn't actually allocate
    // memory.  We can always just return claimToBe, since it'll
    // either be NULL or the string we want, which is the old
    // semantics.  -Derek Wright 3/12/99
    if (authenticator_) {
        return authenticator_->getRemoteUser();
    }
    else {
        return NULL;
    }
#endif  
}               

bool Authentication :: is_valid()
{
    bool valid = FALSE;
#if defined(SKIP_AUTHENTICATION)
    return valid;
#else
    if (authenticator_) {
        valid = authenticator_->isValid();
    }

    return valid;
#endif
}

int Authentication :: encrypt(bool flag)
{
    int code = 0;
#if defined(SKIP_AUTHENTICATION)
    return code;
#else
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
#endif
}

bool Authentication :: is_encrypt()
{
#if defined(SKIP_AUTHENTICATION)
    return FALSE;
#else
    if ( t_mode ==  ENCRYPT || t_mode == ENCRYPT_HDR )
        return TRUE;
    return FALSE;
#endif
}

int Authentication :: hdr_encrypt()
{
#if defined(SKIP_AUTHENTICATION)
    return FALSE;
#else
    if ( t_mode == ENCRYPT){
        t_mode = ENCRYPT_HDR;
    }
    return TRUE;
#endif
}

bool Authentication :: is_hdr_encrypt(){
#if defined(SKIP_AUTHENTICATION)
    return FALSE;
#else
    if (t_mode  == ENCRYPT_HDR)  
        return TRUE;
    else 
        return FALSE;
#endif
}

int Authentication :: wrap(char*  input, 
			   int    input_len, 
			   char*& output,
			   int&   output_len)
{
#if defined(SKIP_AUTHENTICATION)
    return FALSE;
#else
    // Shouldn't we check the flag first?
    if (authenticator_) {
        return authenticator_->wrap(input, input_len, output, output_len);
    }
    else {
        return FALSE;
    }
#endif
}
	
int Authentication :: unwrap(char*  input, 
			     int    input_len, 
			     char*& output, 
			     int&   output_len)
{
#if defined(SKIP_AUTHENTICATION)
    return FALSE;
#else
    // Shouldn't we check the flag first?
    if (authenticator_) {
        return authenticator_->unwrap(input, input_len, output, output_len);
    }
    else {
        return FALSE;
    }
#endif
}


#if !defined(SKIP_AUTHENTICATION)

int Authentication::exchangeKey(KeyInfo *& key)
{
    int retval = 1;
    int hasKey, keyLength, protocol, duration;
    int outputLen, inputLen;
    char * encryptedKey = 0, * decryptedKey = 0;

    if (mySock->isClient()) {
        mySock->decode();
        mySock->code(hasKey);
        mySock->end_of_message();
        if (hasKey) {
            mySock->code(keyLength);// this is not good
            mySock->code(protocol);
            mySock->code(duration);
            mySock->code(inputLen);
            encryptedKey = (char *) malloc(inputLen);
            mySock->get_bytes(encryptedKey, inputLen);
            mySock->end_of_message();

            // Now, unwrap it
            authenticator_->unwrap(encryptedKey,  inputLen, decryptedKey, outputLen);
            key = new KeyInfo((unsigned char *)decryptedKey, keyLength,(Protocol) protocol,duration);
        }
    }
    else {  // server sends the key!

        mySock->encode();
        if (key == 0) {
            hasKey = 0;
            mySock->code(hasKey);
            mySock->end_of_message();
            return 1;
        }
        else { // First, wrap it
            hasKey = 1;
            mySock->code(hasKey);
            mySock->end_of_message();
            keyLength = key->getKeyLength();
            protocol  = (int) key->getProtocol();
            duration  = key->getDuration();

            authenticator_->wrap((char *)key->getKeyData(), keyLength, encryptedKey, outputLen);
            mySock->code(keyLength);       // this is not good
            mySock->code(protocol);
            mySock->code(duration);
            mySock->code(outputLen);
            mySock->put_bytes(encryptedKey, outputLen);
            mySock->end_of_message();
        }
    }

    if (encryptedKey) {
        free(encryptedKey);
    }

    if (decryptedKey) {
        free(decryptedKey);
    }

    return retval;
}

void Authentication::setupEnv( char *hostAddr )
{
    int needfree = 0;
    
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
        if ( getenv( "RENDEZVOUS_DIRECTORY" )) {
	    canUseFlags |= (int) CAUTH_FILESYSTEM_REMOTE;
        }
#if defined( GSS_AUTHENTICATION )
        canUseFlags |= CAUTH_GSS;
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

void Authentication::setAuthType( int state ) {
    auth_status = state;
}

int Authentication::handshake(int clientFlags)
{
    int shouldUseMethod = 0;
    
    int clientCanUse = 0;
    int canUse = (int) canUseFlags;
    
    dprintf ( D_SECURITY, "HANDSHAKE: in handshake(int flags == %i)\n", clientFlags);

    if ( mySock->isClient() ) {
        dprintf (D_SECURITY, "HANDSHAKE: handshake() - i am the client\n");
        if (!clientFlags) {
            mySock->encode();
            dprintf ( D_SECURITY, "HANDSHAKE: sending methods (canUse == %i) to server\n",
                      canUse);
            if ( !mySock->code( canUse ) ||
                 !mySock->end_of_message() )
                {
                    return -1;
                }
        }

        mySock->decode();
        if ( !mySock->code( shouldUseMethod ) ||
             !mySock->end_of_message() )  
            {
                return -1;
            }
        dprintf ( D_SECURITY, "HANDSHAKE: server replied (shouldUseMethod == %i)\n",
                  shouldUseMethod);
        clientCanUse = canUse;
    }
    else { //server
        dprintf (D_SECURITY, "HANDSHAKE: handshake() - i am the server\n");
        if (!clientFlags) {
            mySock->decode();
            if ( !mySock->code( clientCanUse ) ||
                 !mySock->end_of_message() )
                {
                    return -1;
                }
            dprintf ( D_SECURITY, "HANDSHAKE: client sent methods (clientCanUse == %i)\n",
                      clientCanUse);
        } else {
            clientCanUse = clientFlags;
        }
        
        shouldUseMethod = selectAuthenticationType( clientCanUse );
        dprintf ( D_SECURITY, "HANDSHAKE: i picked a method (shouldUseMethod == %i)\n",
                  shouldUseMethod);
        
        
        mySock->encode();
        if ( !mySock->code( shouldUseMethod ) ||
             !mySock->end_of_message() )
            {
                return -1;
            }
        
        dprintf ( D_SECURITY, "HANDSHAKE: client received method (shouldUseMethod == %i)\n",
                  shouldUseMethod);
    }

    dprintf(D_SECURITY, "HANDSHAKE: clientCanUse=%d,shouldUseMethod=%d\n",
            clientCanUse,shouldUseMethod);
    
    return( shouldUseMethod );
}

int Authentication::selectAuthenticationType( int clientCanUse ) 
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
        dprintf ( D_SECURITY, "SELECTAUTHENTICATIONTYPE: evaluating '%s'\n", tmp);
#if defined(GSS_AUTHENTICATION)
        if ( ( clientCanUse & CAUTH_GSS )  
             && !stricmp( tmp, "GSS_AUTHENTICATION" ) ) {
	    dprintf ( D_SECURITY, "SELECTAUTHENTICATIONTYPE: picking '%s'\n", tmp);
	    retval = CAUTH_GSS;
	    break;
        }
#endif
        
#if defined(WIN32)
        if ( ( clientCanUse & CAUTH_NTSSPI ) && !stricmp( tmp, "NTSSPI" ) ) {
	    dprintf ( D_SECURITY, "SELECTAUTHENTICATIONTYPE: picking '%s'\n", tmp);
	    retval = CAUTH_NTSSPI;
	    break;
        }
#else
        if ( ( clientCanUse & CAUTH_FILESYSTEM ) && !stricmp( tmp, "FS" ) ) {
	    dprintf ( D_SECURITY, "SELECTAUTHENTICATIONTYPE: picking '%s'\n", tmp);
	    retval = CAUTH_FILESYSTEM;
	    break;
        }
        if ( ( clientCanUse & CAUTH_FILESYSTEM_REMOTE ) 
             && !stricmp( tmp, "FS_REMOTE" ) ) {
	    dprintf ( D_SECURITY, "SELECTAUTHENTICATIONTYPE: picking '%s'\n", tmp);
            retval = CAUTH_FILESYSTEM_REMOTE;
	    break;
        }
        
#if defined(KERBEROS_AUTHENTICATION)
	  if ( ( clientCanUse & CAUTH_KERBEROS) && !stricmp( tmp, "KERBEROS")){
	    dprintf ( D_SECURITY, "SELECTAUTHENTICATIONTYPE: picking '%s'\n", tmp);
            retval = CAUTH_KERBEROS;
	    break;
        }
#endif
        
#endif
        if ( ( clientCanUse & CAUTH_CLAIMTOBE ) && !stricmp( tmp, "CLAIMTOBE" ) )
	    {
	      dprintf ( D_SECURITY, "SELECTAUTHENTICATIONTYPE: picking '%s'\n", tmp);
	      retval = CAUTH_CLAIMTOBE;
	      break;
	    }
    }
    
    return retval;
}

#endif /* !defined(SKIP_AUTHENTICATION) */










