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


#ifndef CONDOR_AUTHENTICATOR
#define CONDOR_AUTHENTICATOR

#include "reli_sock.h"             // ReliSock
#include "condor_debug.h"
#include "internet.h"
#include "condor_uid.h"
#include "my_username.h"
#include "string_list.h"

const int CAUTH_NONE                    = 0;
const int CAUTH_ANY                     = 1;
const int CAUTH_CLAIMTOBE               = 2;
const int CAUTH_FILESYSTEM              = 4;
const int CAUTH_FILESYSTEM_REMOTE       = 8;
const int CAUTH_NTSSPI                  = 16;
const int CAUTH_GSI                     = 32;
const int CAUTH_KERBEROS                = 64;
const int CAUTH_ANONYMOUS               = 128;
const int CAUTH_SSL                     = 256;
const int CAUTH_PASSWORD                = 512;
const int CAUTH_MUNGE                   = 1024;
const int CAUTH_TOKEN                   = 2048;
const int CAUTH_SCITOKENS               = 4096;

const char STR_DEFAULT_CONDOR_USER[]    = "condor";    // Default condor user
const char STR_DEFAULT_CONDOR_SPOOL[]   = "SPOOL";
const char STR_DEFAULT_CACHE_DIR[]      = "/tmp";      // Not portable!!!
const char STR_CONDOR_CACHE_DIR[]       = "CONDOR_CACHE_DIR";

//----------------------------------------------------------------------
// This is the base class for authenticators. All supported 
// authentication methods should derive from this class
//----------------------------------------------------------------------

class Condor_Auth_Base {
 public:

    Condor_Auth_Base(ReliSock * sock, int mode);
    //------------------------------------------
    // PURPOSE: Constructor for Condor_Auth_Base
    // REQUIRE: mode indicates the authenticator
    //          type. i.e. Kerberos, X.509, etc.
    // RETURNS: A authenticator object
    //------------------------------------------

    virtual ~Condor_Auth_Base();
    //------------------------------------------
    // PURPOSE: Destructor
    // REQUIRE: None
    // RETURNS: None
    //------------------------------------------

    virtual int authenticate(const char * remoteHost, CondorError* errstack, bool non_blocking) = 0;
    virtual int authenticate_continue(CondorError* /*errstack*/, bool /*non_blocking*/) {return -1;};
    //------------------------------------------
    // PURPOSE: authenticate with the other side 
    // REQUIRE: 1. Remote host
    //          2. Call setRemoteHost after successful authentication
    // RETURNS: TRUE -- success; FALSE -- failure
    //------------------------------------------

    virtual int isValid() const = 0;
    //------------------------------------------
    // PURPOSE: whether the authenticator is in
    //          valid state.
    // REQUIRE: None
    // RETURNS: TRUE or FALSE
    //------------------------------------------

    virtual int endTime() const;
    //------------------------------------------
    // PURPOSE: Return the expiration time
    // REQUIRE: None
    // RETURNS: -1 (default), unless overwritten
    //          by derived class such as Kerberos
    //------------------------------------------

    virtual int wrap(const char*  input,
                     int    input_len, 
                     char*& output,
                     int&   output_len);
    //------------------------------------------
    // PURPOSE: Wrap the buffer. The base class
    //          will provide some default
    //          encryption method
    // REQUIRE: A successful authentication
    // RETUNRS: TRUE -- success, FALSE -- falure
    //          May need more code later on
    //------------------------------------------
    
    virtual int unwrap(const char*  input,
                       int    input_len, 
                       char*& output, 
                       int&   output_len);
    //------------------------------------------
    // PURPOSE: Unwrap the buffer
    // REQUIRE: A successful authentication
    // RETURNS: TRUE -- success, FALSE -- failure
    //------------------------------------------

    int isAuthenticated() const;
    //------------------------------------------
    // PURPOSE: Check out the status 
    // REQUIRE: None
    // RETURNS: Authentication status
    //          >= 1 -- authenticated
    //          0    -- not yet authenticated
    //------------------------------------------

    int getMode() const;
    //------------------------------------------
    // PURPOSE: retrieve the mode of the authenticator
    // REQUIRE: None
    // RETURNS: mode -- see the enumeration above
    //------------------------------------------

    const char * getAuthenticatedName() const;

    const char * getRemoteFQU();
    //------------------------------------------
    // PURPOSE: get the user in fully qualifed form
    //          i.e. who@somewhere.edu
    // REQUIRE: authenticated
    // RETURNS: pointer to fqu
    //------------------------------------------

    const char * getRemoteUser() const;
    //------------------------------------------
    // PURPOSE: Return user name of the remote client
    //          For example, userA
    // REQUIRE: successful authentication 
    // RETURSN: null -- if not yet authenticated
    //          const char * -- name of the remote user
    //------------------------------------------

    const char * getRemoteHost() const;
    //------------------------------------------
    // PURPOSE: Find out what remote host is 
    // REQUIRE: A successful authentication
    // RETURNS: A pointer to the remote peer
    //------------------------------------------

    const char * getRemoteDomain() const;
    const char * getLocalDomain() const;
    //------------------------------------------
    // PURPOSE: getRemoteDomain returns domain of 
    //          the remote client For example, cs.wisc.edu. 
    //          getLocalDomain returns domain for 
    //          itself
    // REQUIRE: successful authentication 
    // RETURSN: null -- if not yet authenticated
    //          const char * -- name of the remote domain
    //------------------------------------------

    Condor_Auth_Base& setAuthenticated(int authenticated);
    //------------------------------------------
    // PURPOSE: Set the state of authentication
    // REQUIRE: whether it's authenticated or not
    // RETUNRS: None (this)
    //------------------------------------------

    Condor_Auth_Base& setRemoteUser(const char * user);
    //------------------------------------------
    // PURPOSE: Set the remote user name
    // REQUIRE: Name of the remote user
    // RETUNRS: None (this)
    //------------------------------------------
    
    Condor_Auth_Base& setRemoteDomain(const char * domain);
    //------------------------------------------
    // PURPOSE: Set the remote domain
    // REQUIRE: Name of the remote domain
    // RETUNRS: None (this)
    //------------------------------------------

 protected:

    Condor_Auth_Base& setRemoteHost(const char * hostAddr);
    //------------------------------------------
    // PURPOSE: Set the remote address
    // REQUIRE: address of the remote host
    // RETUNRS: None (this)
    //------------------------------------------

    Condor_Auth_Base& setAuthenticatedName(const char * auth_name);

    Condor_Auth_Base& setFullyQualifiedUser(const char * fqu);

    bool isDaemon() const;
    //------------------------------------------
    // PURPOSE: Whether it's a deamon or not
    // REQUIRE: None
    // RETUNRS: 1 -- daemon; 0 -- no
    //------------------------------------------

    ReliSock *      mySock_;         // Sock to communicate

 private:
    //------------------------------------------
    // Data (private)
    //------------------------------------------
    int             authenticated_;  // status
    int             mode_;           // Which mode are we in
    bool            isDaemon_;       // Whether it's a daemon or not
    char *          remoteUser_;     // Remote user
    char *          remoteDomain_;   // Remote domain
    char *          remoteHost_;     // Remote host
    char *          localDomain_;    // Local user domain
    char *          fqu_;            // Fully qualified
    char *          authenticatedName_;   // Different for each method
};

#endif



