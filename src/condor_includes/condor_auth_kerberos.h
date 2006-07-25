/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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

#ifndef CONDOR_AUTHENTICATOR_KERBEROS
#define CONDOR_AUTHENTICATOR_KERBEROS

#if !defined(SKIP_AUTHENTICATION) && defined(KERBEROS_AUTHENTICATION)

#include "condor_auth.h"        // Condor_Auth_Base class is defined here
#include "MyString.h"
#include "HashTable.h"

extern "C" {
#include "krb5.h"
#include "com_err.h"            // error_message is defined here
}

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

    int authenticate(const char * remoteHost, CondorError* errstack);
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

    int wrap(char* input, int input_len, char*& output, int& output_len);
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
    
    static int init_realm_mapping();
    //------------------------------------------
    // PURPOSE: Initialize realm mapping
    // REQUIRE: None
    // RETURNS: TRUE -- success, FALSE -- failure
    //------------------------------------------
 private:

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

	void Condor_Auth_Kerberos :: dprintf_krb5_principal ( int deblevel, char *fmt, krb5_principal p );
    
    int authenticate_client_kerberos();
    //------------------------------------------
    // PURPOSE : Client's authentication method
    // REQUIRE : None
    // RETURNS : TRUE -- success; FALSE failure
    //------------------------------------------
    
    int authenticate_server_kerberos();
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
    
    int forward_tgt_creds(krb5_creds      * creds,
                          krb5_ccache       ccache);
    //------------------------------------------
    // PURPOSE: Forwarding tgt to another process
    // REQUIRE: krb5_creds
    // RETURNS: 1 -- unable to forward
    //          0 -- success
    //------------------------------------------
    int receive_tgt_creds(krb5_ticket * ticket);
    //------------------------------------------
    // PURPOSE: Receive tgt from another process
    // REQUIRE: ticket, context
    // RETURNS: 1 -- unable to receive tgt
    //          0 -- success
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
    typedef HashTable<MyString, MyString> Realm_Map_t;
    static Realm_Map_t * RealmMap;
};

#endif

#endif

