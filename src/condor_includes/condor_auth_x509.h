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

#ifndef CONDOR_AUTHENTICATOR_GSS
#define CONDOR_AUTHENTICATOR_GSS

#if !defined(SKIP_AUTHENTICATION) && defined(GSI_AUTHENTICATION)

#include "condor_auth.h"        // Condor_Auth_Base class is defined here
#include "globus_gss_assist.h"

const char STR_GSI_DAEMON_DIRECTORY[] = "GSI_DAEMON_DIRECTORY";
const char STR_GSI_DAEMON_PROXY[]     = "GSI_DAEMON_PROXY";
const char STR_GSI_DAEMON_CERT[]      = "GSI_DAEMON_CERT";
const char STR_GSI_DAEMON_KEY[]       = "GSI_DAEMON_KEY";
const char STR_GSI_DAEMON_TRUSTED_CA_DIR[]       = "GSI_DAEMON_TRUSTED_CA_DIR";
const char STR_GSI_USER_PROXY[]       = "X509_USER_PROXY";
const char STR_GSI_CERT_DIR[]         = "X509_CERT_DIR";
const char STR_GSI_USER_CERT[]        = "X509_USER_CERT";
const char STR_GSI_USER_KEY[]         = "X509_USER_KEY";
const char STR_SSLEAY_CONF[]          = "SSLEAY_CONF";
const char STR_GSI_MAPFILE[]          = "GRIDMAP";

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

    int authenticate(const char * remoteHost, CondorError* errstack);
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

    int authenticate_self_gss(CondorError* errstack);

    int authenticate_client_gss(CondorError* errstack);

    int authenticate_server_gss(CondorError* errstack);

    char * get_server_info();

    int nameGssToLocal( char * GssClient );

#ifdef WIN32
	int ParseMapFile();
	int condor_gss_assist_gridmap ( char * GssClient, char *& local_user );
#endif

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
#ifdef WIN32
    typedef HashTable<MyString, MyString> Grid_Map_t;
    static Grid_Map_t * GridMap;
#endif
};

#endif

#endif
