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

#include "reli_sock.h"
#include "condor_auth.h"
#include "CryptKey.h"

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
    
    int authenticate( char *hostAddr, int clientFlags = 0 );
    //------------------------------------------
    // PURPOSE: authenticate with the other side 
    // REQUIRE: hostAddr    -- host to authenticate
    //          clientFlags -- what protocols
    //                         does client support
    // RETURNS: -1 -- failure
    //------------------------------------------

    int authenticate( char *hostAddr, KeyInfo *& key , int clientFlags = 0 );
    //------------------------------------------
    // PURPOSE: To send the secret key over. this method
    //          is written to keep compatibility issues
    //          Later on, we can get rid of the first authenticate method
    // REQUIRE: hostAddr  -- host address
    //          key       -- the key to be sent over, see CryptKey.h
    //          clientFlags -- protocols client supports
    // RETURNS: -1 -- failure
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
    
    int handshake(int clientCanUse);

    int exchangeKey(KeyInfo *& key);
    
    void setAuthType( int state );
    
    int selectAuthenticationType( int methods );
    
    int default_auth_methods();
    
#endif /* !SKIP_AUTHENTICATION */
    
    //------------------------------------------
    // Data (private)
    //------------------------------------------
    Condor_Auth_Base *   authenticator_;    // This is it.
    ReliSock         *   mySock;
    transfer_mode        t_mode;
    int                  auth_status;
    // int                  canUseFlags;
    // char             *   serverShouldTry;
};

#endif /* AUTHENTICATION_H */


