/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
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
#include "../condor_daemon_core.V6/condor_ipverify.h"
#include "CondorError.h"

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
	method_used         = NULL;
	t_mode              = NORMAL;
	authenticator_      = NULL;
#endif
}

Authentication::~Authentication()
{
#if !defined(SKIP_AUTHENTICATION)
	mySock = NULL;

	delete authenticator_;

	if (method_used) {
		free (method_used);
	}

#endif
}

int Authentication::authenticate( char *hostAddr, KeyInfo *& key, const char* auth_methods, CondorError* errstack)
{
    int retval = authenticate(hostAddr, auth_methods, errstack);
    
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

int Authentication::authenticate( char *hostAddr, const char* auth_methods,
		CondorError* errstack)
{
#if defined(SKIP_AUTHENTICATION)
	dprintf(D_ALWAYS, "Skipping....\n");
	/*
	errstack->push ( "AUTHENTICATE", AUTHENTICATE_ERR_NOT_BUILT,
			"this condor was built with SKIP_AUTHENTICATION");
	*/
	return 0;
#else
Condor_Auth_Base * auth = NULL;

	if (DebugFlags & D_FULLDEBUG) {
		if (hostAddr) {
			dprintf ( D_SECURITY, "AUTHENTICATE: in authenticate( addr == '%s', "
					"methods == '%s')\n", hostAddr, auth_methods);
		} else {
			dprintf ( D_SECURITY, "AUTHENTICATE: in authenticate( addr == NULL, "
					"methods == '%s')\n", auth_methods);
		}
	}

	MyString methods_to_try = auth_methods;

	auth_status = CAUTH_NONE;
	method_used = NULL;
 
	while (auth_status == CAUTH_NONE ) {
		if (DebugFlags & D_FULLDEBUG) {
			dprintf(D_SECURITY, "AUTHENTICATE: can still try these methods: %s\n", methods_to_try.Value());
		}

		int firm = handshake(methods_to_try);

		if ( firm < 0 ) {
			dprintf(D_ALWAYS, "AUTHENTICATE: handshake failed!\n");
			errstack->push( "AUTHENTICATE", AUTHENTICATE_ERR_HANDSHAKE_FAILED, "Failure performing handshake" );
			break;
		}

		char* method_name = NULL;
		switch ( firm ) {
#if defined(GSI_AUTHENTICATION)
			case CAUTH_GSI:
				auth = new Condor_Auth_X509(mySock);
				method_name = strdup("GSI");
				break;
#endif /* GSI_AUTHENTICATION */

#if defined(KERBEROS_AUTHENTICATION) 
			case CAUTH_KERBEROS:
				auth = new Condor_Auth_Kerberos(mySock);
				method_name = strdup("KERBEROS");
				break;
#endif
 
#if defined(WIN32)
			case CAUTH_NTSSPI:
				auth = new Condor_Auth_SSPI(mySock);
				method_name = strdup("NTSSPI");
				break;
#else
			case CAUTH_FILESYSTEM:
				auth = new Condor_Auth_FS(mySock);
				method_name = strdup("FS");
				break;
			case CAUTH_FILESYSTEM_REMOTE:
				auth = new Condor_Auth_FS(mySock, 1);
				method_name = strdup("FS_REMOTE");
				break;
#endif /* !defined(WIN32) */
			case CAUTH_CLAIMTOBE:
				auth = new Condor_Auth_Claim(mySock);
				method_name = strdup("CLAIMTOBE");
				break;
 
			case CAUTH_ANONYMOUS:
				auth = new Condor_Auth_Anonymous(mySock);
				method_name = strdup("ANONYMOUS");
				break;
 
			case CAUTH_NONE:
				dprintf(D_ALWAYS,"AUTHENTICATE: no available authentication methods succeeded, "
						"failing!\n");
				errstack->push("AUTHENTICATE", AUTHENTICATE_ERR_OUT_OF_METHODS,
						"Failed to authenticate with any method");
				return 0;

			default:
				dprintf(D_ALWAYS,"AUTHENTICATE: unsupported method: %i, failing.\n", firm);
				errstack->pushf("AUTHENTICATE", AUTHENTICATE_ERR_OUT_OF_METHODS,
						"Failure.  Unsupported method: %i", firm);
				return 0;
		}


		if (DebugFlags & D_FULLDEBUG) {
			dprintf(D_SECURITY, "AUTHENTICATE: will try to use %d (%s)\n", firm,
					(method_name?method_name:"?!?") );
		}

		//------------------------------------------
		// Now authenticate
		//------------------------------------------
		if (!auth->authenticate(hostAddr, errstack) ) {
			delete auth;
			auth = NULL;

			errstack->pushf("AUTHENTICATE", AUTHENTICATE_ERR_METHOD_FAILED,
					"Failed to authenticate using %s", method_name );

			//if authentication failed, try again after removing from client tries
			if ( mySock->isClient() ) {
				// need to remove this item from the MyString!!  perhaps there is a
				// better way to do this...  anyways, 'firm' is equal to the bit value
				// of a particular method, so we'll just convert each item in the list
				// and keep it if it's not that particular bit.
				StringList meth_iter( methods_to_try.Value() );
				meth_iter.rewind();
				MyString new_list;
				char *tmp = NULL;
				while( (tmp = meth_iter.next()) ) {
					int that_bit = SecMan::getAuthBitmask( tmp );

					// keep if this isn't the failed method.
					if (firm != that_bit) {
						// and of course, keep the comma's correct.
						if (new_list.Length() > 0) {
							new_list += ",";
						}
						new_list += tmp;
					}
				}

				// trust the copy constructor. :)
				methods_to_try = new_list;
			}

			dprintf(D_SECURITY,"AUTHENTICATE: method %d (%s) failed.\n", firm,
					(method_name?method_name:"?!?"));
		} else {
			// authentication succeeded.  store the object (we may call upon
			// its wrapper functions) and set the auth_status of this sock to
			// the bitmask method we used and the method_used to the string
			// name.  (string name is obtained above because there is currently
			// no bitmask -> string map)
			authenticator_ = auth;
			auth_status = authenticator_->getMode();
			if (method_name) {
				method_used = strdup(method_name);
			} else {
				method_used = NULL;
			}
		}
		free (method_name);
	}

	//if none of the methods succeeded, we fall thru to default "none" from above
	int retval = ( auth_status != CAUTH_NONE );
	if (DebugFlags & D_FULLDEBUG) {
		dprintf(D_SECURITY, "AUTHENTICATE: auth_status == %i (%s)\n", auth_status,
				(method_used?method_used:"?!?") );
	}
	dprintf(D_SECURITY, "Authentication was a %s.\n", retval == 1 ? "Success" : "FAILURE" );

	mySock->allow_one_empty_message();
	return ( retval );
#endif /* SKIP_AUTHENTICATION */
}


int Authentication::isAuthenticated() const
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
	if (authenticator_) {
    	delete authenticator_;
    	authenticator_ = 0;
	}
	if (method_used) {
		free (method_used);
		method_used = 0;
	}
#endif
}


char* Authentication::getMethodUsed() {
#if !defined(SKIP_AUTHENTICATION)
	return method_used;
#else
	return NULL;
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
	const char *owner;
    if (authenticator_) {
        owner = authenticator_->getRemoteUser();
    }
    else {
        owner = NULL;
    }

	// If we're authenticated, we should always have a valid owner
	if ( isAuthenticated() ) {
		if ( NULL == owner ) {
			EXCEPT( "Socket is authenticated, but has no owner!!" );
		}
	}
	return owner;
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


void Authentication::setAuthType( int state ) {
    auth_status = state;
}


int Authentication::handshake(MyString my_methods) {

    int shouldUseMethod = 0;
    
    dprintf ( D_SECURITY, "HANDSHAKE: in handshake(my_methods = '%s')\n", my_methods.Value());

    if ( mySock->isClient() ) {

		// client

        dprintf (D_SECURITY, "HANDSHAKE: handshake() - i am the client\n");
        mySock->encode();
		int method_bitmask = SecMan::getAuthBitmask(my_methods.Value());
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
        
        shouldUseMethod = selectAuthenticationType( my_methods, client_methods );

        dprintf ( D_SECURITY, "HANDSHAKE: i picked (method == %i)\n", shouldUseMethod);
        
        
        mySock->encode();
        if ( !mySock->code( shouldUseMethod ) || !mySock->end_of_message() ) {
            return -1;
        }
        
		dprintf ( D_SECURITY, "HANDSHAKE: client received (method == %i)\n", shouldUseMethod);
    }

    return( shouldUseMethod );
}



int Authentication::selectAuthenticationType( MyString method_order, int remote_methods ) {

	// the first one in the list that is also in the bitmask is the one
	// that we pick.  so, iterate the list.

	StringList method_list( method_order.Value() );

	char * tmp = NULL;
	method_list.rewind();

	while (	(tmp = method_list.next()) ) {
		int that_bit = SecMan::getAuthBitmask( tmp );
		if ( remote_methods & that_bit ) {
			// we have a match.
			return that_bit;
		}
	}

	return 0;
}


#endif /* !defined(SKIP_AUTHENTICATION) */

