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

#ifndef CONDOR_AUTHENTICATOR_GSS
#define CONDOR_AUTHENTICATOR_GSS

#if !defined(SKIP_AUTHENTICATION) && defined(X509_AUTHENTICATION)

#include "condor_auth.h"        // Condor_Auth_Base class is defined here
#include "globus_gss_assist.h"

const char STR_X509_USER_PROXY[]       = "X509_USER_PROXY";
const char STR_X509_DIRECTORY[]        = "X509_DIRECTORY";
const char STR_X509_CERT_DIR[]         = "X509_CERT_DIR";
const char STR_X509_USER_CERT[]        = "X509_USER_CERT";
const char STR_X509_USER_KEY[]         = "X509_USER_KEY";
const char STR_SSLEAY_CONF[]           = "SSLEAY_CONF";

class GSSComms {
public:
   ReliSock *sock;
   void *buffer;
   int size;
};


class Condor_Auth_X509 : public Condor_Auth_Base {
 public:
    Condor_Auth_X509(ReliSock * sock);
    //------------------------------------------
    // Constructor
    //------------------------------------------

    ~Condor_Auth_X509();
    //------------------------------------------
    // Destructor
    //------------------------------------------

    int authenticate(const char * remoteHost);
    //------------------------------------------
    // PURPOSE: authenticate with the other side 
    // REQUIRE: hostAddr -- host to authenticate
    // RETURNS:
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

    int isValid() const;
    //------------------------------------------
    // PURPOSE: whether the authenticator is in
    //          valid state.
    // REQUIRE: None
    // RETURNS: true or false
    //------------------------------------------

     int endTime() const;
    //------------------------------------------
    // PURPOSE: Return the expiration time for
    //          kerberos
    // REQUIRE: None
    // RETURNS: -1 -- invalid
    //          >0 -- expiration time
    //------------------------------------------
 private:

    int authenticate_self_gss();

    int authenticate_client_gss();

    int authenticate_server_gss();

    int nameGssToLocal( char * GssClient );

    int get_user_x509name(char*,char*);
    
    /** Check whether the security context of the scoket is valid or not 
	@return TRUE if valid FALSE if not */
    bool gss_is_valid();
    
    /** A specialized function that is needed for secure personal
        condor. When schedd and the user are running under the
        same userid we would still want the authentication process
        be transperant. This means that we have to maintain to
        distinct environments to processes running under the same
        id. erase\_env ensures that the X509\_USER\_PROXY which
        should \bf{NOT} be set for daemon processes is wiped out
        even if it is set, so that a daemon process does not start
        looking for a proxy which it does not need. */
    void  erase_env();
    
    void  print_log(OM_uint32 ,OM_uint32 ,int , char*);

    //------------------------------------------
    // Data (private)
    //------------------------------------------
    gss_cred_id_t       credential_handle;
    gss_ctx_id_t        context_handle ;
    int                 token_status;
    //X509_Credential *   my_credential;
    OM_uint32	        ret_flags ;
};

#endif

#endif
