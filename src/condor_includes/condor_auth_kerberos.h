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

#ifndef CONDOR_AUTHENTICATOR_KERBEROS
#define CONDOR_AUTHENTICATOR_KERBEROS

#if !defined(SKIP_AUTHENTICATION) && defined(KERBEROS_AUTHENTICATION)

#include "condor_auth.h"        // Condor_Auth_Base class is defined here
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

    int authenticate(const char * remoteHost);
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

 private:

    int daemon_acquire_credential();
    //------------------------------------------
    // PURPOSE : Acquire service credential on 
    //           behalf of the user
    // REQUIRE : NONE
    // RETURNS: TRUE -- if success; FALSE -- if failure
    //------------------------------------------

    int client_acquire_credential();
    //------------------------------------------
    // PURPOSE : Acquire a credential for client
    // REQUIRE : None
    // RETURNS : TRUE -- success; FALSE failure
    //------------------------------------------
    
    int server_acquire_credential();
    //------------------------------------------
    // PURPOSE: Acquire a credential for the daemon process
    // REQUIRE: None (probably root access)
    // RETURNS: TRUE -- success; FALSE -- failure
    //------------------------------------------
    
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
    
    int map_kerberos_name(krb5_ticket * ticket);
    //------------------------------------------
    // PURPOSE: Map kerberos realm to condor uid
    // REQUIRE: A valid kerberos principal
    // RETURNS: TRUE --success; FALSE -- failure
    //------------------------------------------
    
    int get_server_info();
    //------------------------------------------
    // PURPOSE: Get server info
    // REQUIRE: NONE
    // RETURNS: true -- success; false -- failure
    //------------------------------------------
    
    int send_request(krb5_data * request);
    //------------------------------------------
    // PURPOSE: Send an authentication request 
    // REQUIRE: None
    // RETURNS: KERBEROS_GRANT -- granted
    //          KERBEROS_DENY  -- denied
    //------------------------------------------
    
    void initialize_client_data();
    //------------------------------------------
    // PURPOSE: intialize default cache name and 
    // REQUIRE: NONE
    // RETURNS: NONE
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
    char *             ccname_;           // FILE:/krbcc_name
    char *             defaultCondor_;    // Default condor 
    char *             defaultStash_;     // Default stash location
    char *             keytabName_;       // keytab to use   
};

#endif

#endif

