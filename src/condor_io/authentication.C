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
#include "condor_auth_anonymous.h"
#include "condor_auth_fs.h"
#include "condor_auth_sspi.h"
#include "condor_auth_x509.h"
#include "condor_auth_kerberos.h"
#include "condor_secman.h"
#include "condor_environ.h"

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
	t_mode              = NORMAL;
        authenticator_      = NULL;
#endif
}

Authentication::~Authentication()
{
#if !defined(SKIP_AUTHENTICATION)
	mySock = NULL;

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

int Authentication::authenticate( char *hostAddr, int auth_method )
{
#if defined(SKIP_AUTHENTICATION)
    dprintf(D_ALWAYS, "Skipping....\n");
  return 0;
#else
  Condor_Auth_Base * auth = NULL;

  if (hostAddr) {
	dprintf ( D_SECURITY, "AUTHENTICATE: in authenticate( char *addr == '%s', int method == %i)\n",
		  hostAddr, auth_method);
  } else {
	dprintf ( D_SECURITY, "AUTHENTICATE: in authenticate( char *addr == NULL, int method == %i)\n",
		  auth_method);
  }
  
  int methods_to_try = 0;

  if (auth_method) {
	  methods_to_try = auth_method;
  } else {
	  methods_to_try = default_auth_methods();
  }

  
  auth_status = CAUTH_NONE;
  
  while (auth_status == CAUTH_NONE ) {
    dprintf(D_SECURITY, "AUTHENTICATE: can still try these methods: %d\n", methods_to_try);

    int firm = handshake(methods_to_try);
    if ( firm < 0 ) {
      dprintf(D_ALWAYS, "AUTHENTICATE: handshake failed!\n");
      break;
    }


    dprintf(D_SECURITY, "AUTHENTICATE: will try to use %d\n", firm);

    switch ( firm ) {
#if defined(GSI_AUTHENTICATION)
    case CAUTH_GSI:
        auth = new Condor_Auth_X509(mySock);
      break;
#endif /* GSI_AUTHENTICATION */

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
      
    case CAUTH_ANONYMOUS:
        auth = new Condor_Auth_Anonymous(mySock);
      break;
      
    default:
      dprintf(D_ALWAYS,"AUTHENTICATE: unsupported method: %i, failing.\n", firm);
      return 0;
    }

    //------------------------------------------
    // Now authenticate
    //------------------------------------------
    if (!auth->authenticate(hostAddr) ) {
        delete auth;
		auth = NULL;

        //if authentication failed, try again after removing from client tries
        if ( mySock->isClient() ) {
            methods_to_try &= ~firm;
        }

      	dprintf(D_SECURITY,"AUTHENTICATE: method %i failed.\n", firm);
		return 0;
    }
    else {
        authenticator_ = auth;
        auth_status = authenticator_->getMode();
    }
  }

  //if none of the methods succeeded, we fall thru to default "none" from above
  int retval = ( auth_status != CAUTH_NONE );
  dprintf(D_SECURITY, "Authentication::authenticate %s\n", 
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
    delete authenticator_;
    authenticator_ = 0;
#endif
}


/*
void Authentication::setAuthAny()
{
#if !defined(SKIP_AUTHENTICATION)
    canUseFlags = CAUTH_ANY;
#endif
}
*/

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

const char * Authentication::getFullyQualifiedUser() const
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
        return authenticator_->getRemoteFQU();
    }
    else {
        return NULL;
    }
#endif  
}           

int Authentication :: end_time()
{
    int endtime = 0;
#if !defined(SKIP_AUTHENTICATION)
    if (authenticator_) {
        endtime = authenticator_->endTime();
    }
#endif
    return endtime;
}

bool Authentication :: is_valid()
{
    bool valid = FALSE;
#if !defined(SKIP_AUTHENTICATION)
    if (authenticator_) {
        valid = authenticator_->isValid();
    }
#endif
    return valid;
}

int Authentication :: encrypt(bool flag)
{
    int code = 0;
#if !defined(SKIP_AUTHENTICATION)
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
#endif
    return code;
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
            if (!mySock->code(keyLength) ||
                !mySock->code(protocol)  ||
                !mySock->code(duration)  ||
                !mySock->code(inputLen)) {
                return 0;
            }
            encryptedKey = (char *) malloc(inputLen);
            mySock->get_bytes(encryptedKey, inputLen);
            mySock->end_of_message();

            // Now, unwrap it
            authenticator_->unwrap(encryptedKey,  inputLen, decryptedKey, outputLen);
            key = new KeyInfo((unsigned char *)decryptedKey, keyLength,(Protocol) protocol,duration);
        }
        else {
            key = NULL;
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
            if (!mySock->code(hasKey) || !mySock->end_of_message()) {
                return 0;
            }
            keyLength = key->getKeyLength();
            protocol  = (int) key->getProtocol();
            duration  = key->getDuration();

            authenticator_->wrap((char *)key->getKeyData(), keyLength, encryptedKey, outputLen);

            if (!mySock->code(keyLength) || 
                !mySock->code(protocol)  ||
                !mySock->code(duration)  ||
                !mySock->code(outputLen) ||
                !mySock->put_bytes(encryptedKey, outputLen) ||
                !mySock->end_of_message()) {
                free(encryptedKey);
                return 0;
            }
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

int Authentication::default_auth_methods() {

	// get the methods from config file
	char * methods = param("SEC_DEFAULT_AUTHENTICATION_METHODS");

	int bitmask = 0;
	if (methods) {
		// instantiating a SecMan isn't a big deal... all the
		// data in it is static anyways.  i guess the functions
		// really should be too.  oh well, deal with it.
		SecMan s;

		bitmask = s.getAuthBitmask(methods);
		free( methods );
	} else {

		// clear the mask
		bitmask = 0;

		// these do _not_ go in the default
		// bitmask |= (int) CAUTH_CLAIMTOBE;
		// bitmask |= (int) CAUTH_ANONYMOUS;

#if defined(WIN32)
        bitmask |= (int) CAUTH_NTSSPI;
#else
        bitmask |= (int) CAUTH_FILESYSTEM;
        
        //RendezvousDirectory is for use by shared-filesystem filesys auth.
        //if user specfied RENDEZVOUS_DIRECTORY, extract it
        if ( getenv( EnvGetName( ENV_RENDEZVOUS ) )) {
	    	bitmask |= (int) CAUTH_FILESYSTEM_REMOTE;
        }
#endif
	}

	return bitmask;
}


void Authentication::setAuthType( int state ) {
    auth_status = state;
}


int Authentication::handshake(int method_bitmask) {

    int shouldUseMethod = 0;
    
    dprintf ( D_SECURITY, "HANDSHAKE: in handshake(int methods == %i)\n", method_bitmask);

    if ( mySock->isClient() ) {

		// client

        dprintf (D_SECURITY, "HANDSHAKE: handshake() - i am the client\n");
        mySock->encode();
        dprintf ( D_SECURITY, "HANDSHAKE: sending (methods == %i) to server\n", method_bitmask);
        if ( !mySock->code( method_bitmask ) || !mySock->end_of_message() ) {
            return -1;
        }

    	mySock->decode();
    	if ( !mySock->code( shouldUseMethod ) || !mySock->end_of_message() )  {
        	return -1;
    	}
    	dprintf ( D_SECURITY, "HANDSHAKE: server replied (method = %i)\n", shouldUseMethod);

    } else {

		//server

		int client_methods = 0;
        dprintf (D_SECURITY, "HANDSHAKE: handshake() - i am the server\n");
        mySock->decode();
        if ( !mySock->code( client_methods ) || !mySock->end_of_message() ) {
            return -1;
        }
        dprintf ( D_SECURITY, "HANDSHAKE: client sent (methods == %i)\n", client_methods);
        
		// take our supported method bitmasks and bitwise AND them together.  this yields
		// the methods we have in common.  selectAuthenticationType then picks one from a
		// hardcoded order
        shouldUseMethod = selectAuthenticationType( method_bitmask & client_methods );
        dprintf ( D_SECURITY, "HANDSHAKE: i picked (method == %i)\n", shouldUseMethod);
        
        
        mySock->encode();
        if ( !mySock->code( shouldUseMethod ) || !mySock->end_of_message() ) {
            return -1;
        }
        
		dprintf ( D_SECURITY, "HANDSHAKE: client received (method == %i)\n", shouldUseMethod);
    }

    return( shouldUseMethod );
}



int Authentication::selectAuthenticationType( int methods ) {

#if defined(GSI_AUTHENTICATION)
	if ( methods & CAUTH_GSI )  {
		return CAUTH_GSI;
	}
#endif
	
#if defined(WIN32)
	if ( methods & CAUTH_NTSSPI ) {
		return CAUTH_NTSSPI;
	}
#else
	if ( methods & CAUTH_FILESYSTEM ) {
		return CAUTH_FILESYSTEM;
	}
	if ( methods & CAUTH_FILESYSTEM_REMOTE ) {
		return CAUTH_FILESYSTEM_REMOTE;
	}
#endif
	

#if defined(KERBEROS_AUTHENTICATION)
	if ( methods & CAUTH_KERBEROS ) {
		return CAUTH_KERBEROS;
	}
#endif
	if ( methods & CAUTH_CLAIMTOBE ) {
		return CAUTH_CLAIMTOBE;
	}

	if ( methods & CAUTH_ANONYMOUS ) {
		return CAUTH_ANONYMOUS;
	}

    return CAUTH_NONE;
}


#endif /* !defined(SKIP_AUTHENTICATION) */

