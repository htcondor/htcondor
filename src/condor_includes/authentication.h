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

#ifndef AUTHENTICATION_H
#define AUTHENTICATION_H

#include "reli_sock.h"
#include "condor_auth.h"
#include "CryptKey.h"
#include "../condor_daemon_core.V6/condor_ipverify.h"
#include "CondorError.h"

#define MAX_USERNAMELEN 128

enum transfer_mode {
  NORMAL = 1,
  ENCRYPT,
  ENCRYPT_HDR
};

class Authentication {
	
    friend class ReliSock;

 public:
    /// States to track status/authentication level
    Authentication( ReliSock *sock );
    
    ~Authentication();
    
    int authenticate( char *hostAddr, const char* auth_methods, CondorError* errstack);
    //------------------------------------------
    // PURPOSE: authenticate with the other side 
    // REQUIRE: hostAddr     -- host to authenticate
	//          auth_methods -- protocols to use
    // RETURNS: -1 -- failure
    //------------------------------------------

    int authenticate( char *hostAddr, KeyInfo *& key, const char* auth_methods, CondorError* errstack);
    //------------------------------------------
    // PURPOSE: To send the secret key over. this method
    //          is written to keep compatibility issues
    //          Later on, we can get rid of the first authenticate method
    // REQUIRE: hostAddr     -- host address
    //          key          -- the key to be sent over, see CryptKey.h
    //          auth_methods -- protocols to use
    // RETURNS: -1 -- failure
    //------------------------------------------
    
    char* getMethodUsed();
    //------------------------------------------
    // PURPOSE: Return the method of authentication
    // REQUIRE: That authentication has already happened
    // RETURNS: pointer to a string you should NOT free
	//          which is the name of the method, or NULL
	//          if not currently authenticated, see next.
    //------------------------------------------

    int isAuthenticated();
    //------------------------------------------
    // PURPOSE: Return the state of the authentication
    // REQUIRE: None
    // RETURNS: true -- 1; false -- 0
    //------------------------------------------
    
    void unAuthenticate();
    //------------------------------------------
    // PURPOSE:
    // REQUIRE:
    // RETURNS:
    //------------------------------------------
    
    void setAuthAny();
    
    int setOwner( const char *owner );

    const char *getFullyQualifiedUser() const;
    
    const char *getOwner() const;
    
    const char *getDomain() const;
    
    const char * getRemoteAddress() const;
    //----------------------------------------
    // PURPOSE: Get remote address 
    // REQUIRE: None
    // RETURNS: IP address of the remote machine
    //------------------------------------------
    
    bool is_valid();
    //------------------------------------------
    // PURPOSE: Whether the security context supports
    //          encryption or not?
    // REQUIRE: NONE
    // RETURNS: TRUE or FALSE
    //------------------------------------------
    
    int end_time();
    //------------------------------------------
    // PURPOSE: Return the expiration time for the
    //          authenticator
    // REQUIRE: None
    // RETURNS: -1 -- invalid, for Kerberos, X.509, etc
    //           0 -- undefined, for FS, 
    //          >0 -- for Kerberos and X.509
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
    
 private:
#if !defined(SKIP_AUTHENTICATION)
    Authentication() {}; //should never be called, make private to help that!
    
    int handshake(MyString clientCanUse);

    int exchangeKey(KeyInfo *& key);
    
    void setAuthType( int state );
    
    int selectAuthenticationType( MyString my_methods, int remote_methods );
    
#endif /* !SKIP_AUTHENTICATION */
    
    //------------------------------------------
    // Data (private)
    //------------------------------------------
    Condor_Auth_Base *   authenticator_;    // This is it.
    ReliSock         *   mySock;
    transfer_mode        t_mode;
    int                  auth_status;
    char*                method_used;
};

#endif /* AUTHENTICATION_H */


