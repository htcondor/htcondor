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


#ifndef CONDOR_AUTHENTICATOR_KERBEROS
#define CONDOR_AUTHENTICATOR_KERBEROS

#if defined(HAVE_EXT_KRB5)

#include "condor_auth.h"        // Condor_Auth_Base class is defined here
#include "HashTable.h"

#ifdef WIN32
// kerb5.h in win-mac.h sets MAXHOSTNAMELEN differently than we do (incorrectly?)
// so we want to push/pop our definition and undef it to prevent a pointless warning.
#pragma push_macro("MAXHOSTNAMELEN")
#undef MAXHOSTNAMELEN
#endif

extern "C" {
#include "krb5.h"
#if defined(Darwin)
#include "com_err.h"
#endif
}
#ifdef WIN32
#pragma pop_macro("MAXHOSTNAMELEN")
#endif

class Condor_Auth_Kerberos : public Condor_Auth_Base {
 public:
    Condor_Auth_Kerberos(ReliSock * sock);
    //------------------------------------------
    // Constructor
    //------------------------------------------

    ~Condor_Auth_Kerberos();
    //------------------------------------------
    // Destructor
    //------------------------------------------

    static bool Initialize();
    // Perform one-time initialization, primarily dlopen()ing the
    // kerberos libs on linux. Returns true on success, false on failure.

    int authenticate(const char * remoteHost, CondorError* errstack, bool non_blocking);
    int authenticate_continue(CondorError*, bool);
    //------------------------------------------
    // PURPOSE: authenticate with the other side 
    // REQUIRE: hostAddr -- host to authenticate
    // RETURNS:
    //------------------------------------------

    int isValid() const;
    //------------------------------------------
    // PURPOSE: whether the authenticator is in
    //          valid state.
    // REQUIRE: None
    // RETURNS: 1 -- true; 0 -- false
    //------------------------------------------

    int endTime() const;
    //------------------------------------------
    // PURPOSE: Return the expiration time for
    //          kerberos
    // REQUIRE: None
    // RETURNS: -1 -- invalid
    //          >0 -- expiration time
    //------------------------------------------

    int wrap(const char* input, int input_len, char*& output, int& output_len);
    //------------------------------------------
    // PURPOSE: Wrap the buffer
    // REQUIRE: 
    // RETUNRS: TRUE -- success, FALSE -- falure
    //          May need more code later on
    //------------------------------------------
    
    int unwrap(const char* input, int input_len, char*& output, int& output_len);
    //------------------------------------------
    // PURPOSE: Unwrap the buffer
    // REQUIRE: 
    // RETURNS: TRUE -- success, FALSE -- failure
    //------------------------------------------
    
    static int init_realm_mapping();
    //------------------------------------------
    // PURPOSE: Initialize realm mapping
    // REQUIRE: None
    // RETURNS: TRUE -- success, FALSE -- failure
    //------------------------------------------
 private:

	static bool m_initTried;
	static bool m_initSuccess;

    // states, return vals, and drivers for non-blocking authentication
    enum CondorAuthKerberosState {
        ServerReceiveClientReadiness = 100,
        ServerAuthenticate,
        ServerReceiveClientSuccessCode
    };

    enum CondorAuthKerberosRetval {
        Fail = 0,
        Success,
        WouldBlock,
        Continue
    };

    CondorAuthKerberosState m_state;
    krb5_ticket *     ticket_;

    CondorAuthKerberosRetval doServerReceiveClientReadiness(CondorError* errstack, bool non_blocking);
    CondorAuthKerberosRetval doServerAuthenticate(CondorError* errstack, bool non_blocking);
    CondorAuthKerberosRetval doServerReceiveClientSuccessCode(CondorError* errstack, bool non_blocking);


    int init_user();
    //------------------------------------------
    // PURPOSE : initiailze user info for authentication
    // REQUIRE : None
    // RETURNS : TRUE -- success; FALSE failure
    //------------------------------------------

    int init_daemon();
    //------------------------------------------
    // PURPOSE: initialize daemon info for authentication 
    // REQUIRE: NONE
    // RETURNS: TRUE -- if success; FALSE -- if failure
    //------------------------------------------

	void dprintf_krb5_principal ( int deblevel, const char *fmt,
								  krb5_principal p );
    
    int authenticate_client_kerberos();
    //------------------------------------------
    // PURPOSE : Client's authentication method
    // REQUIRE : None
    // RETURNS : TRUE -- success; FALSE failure
    //------------------------------------------
    
    int authenticate_server_kerberos_0();
    int authenticate_server_kerberos_1();
    int authenticate_server_kerberos_2();
    //------------------------------------------
    // PURPOSE : Server's authentication method
    // REQUIRE : None
    // RETURNS : TRUE -- success; FALSE failure
    //------------------------------------------
    
    int init_kerberos_context();
    //------------------------------------------
    // PURPOSE: Initialize context
    // REQUIRE: NONE
    // RETURNS: TRUE -- success; FALSE -- failure
    //------------------------------------------
    
    int map_kerberos_name(krb5_principal * princ);
    int map_domain_name(const char * domain);
    //------------------------------------------
    // PURPOSE: Map kerberos realm to condor uid
    // REQUIRE: A valid kerberos principal
    // RETURNS: TRUE --success; FALSE -- failure
    //------------------------------------------
    
    int send_request(krb5_data * request);
    int send_request_and_receive_reply(krb5_data * request);
    //------------------------------------------
    // PURPOSE: Send an authentication request 
    // REQUIRE: None
    // RETURNS: KERBEROS_GRANT -- granted
    //          KERBEROS_DENY  -- denied
    //------------------------------------------
    
    int init_server_info();
    //------------------------------------------
    // PURPOSE: intialize default cache name and 
    // REQUIRE: NONE
    // RETURNS: 1 -- success; -1:  failure
    //------------------------------------------
    
    int read_request(krb5_data * request);
    //------------------------------------------
    // PURPOSE: Read a request
    // REQUIRE: Pointer to a krb5_data object
    // RETURNS: fields in krb5_data will be filled
    //------------------------------------------
    
    int client_mutual_authenticate();
    //------------------------------------------
    // PURPOSE: Mututal authenticate the server
    // REQUIRE: None
    // RETURNS: Server's response code
    //------------------------------------------
    
    void setRemoteAddress();
    //------------------------------------------
    // PURPOSE: Get remote host name 
    // REQUIRE: successful kereros authentication
    // RETURNS: NONE, set remotehost_
    //------------------------------------------
    
    //------------------------------------------
    // Data (private)
    //------------------------------------------
    krb5_context       krb_context_;      // context_
    krb5_auth_context  auth_context_;     // auth_context_
    krb5_principal     krb_principal_;    // principal information, local
    krb5_principal     server_;           // principal, remote
    krb5_keyblock *    sessionKey_;       // Session key
    krb5_creds    *    creds_;            // credential
    char *             ccname_;           // FILE:/krbcc_name
    char *             defaultStash_;     // Default stash location
    char *             keytabName_;       // keytab to use   
    static std::map<std::string, std::string>* RealmMap;
};

#endif

#endif

