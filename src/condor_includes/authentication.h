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


#ifndef AUTHENTICATION_H
#define AUTHENTICATION_H

#include "reli_sock.h"
#include "condor_auth.h"
#include "CryptKey.h"
#include "condor_ipverify.h"
#include "CondorError.h"
#include "MapFile.h"

#define MAX_USERNAMELEN 128

class Authentication {
	
    friend class ReliSock;

 public:
    /// States to track status/authentication level
    Authentication( ReliSock *sock );
    
    ~Authentication();
    
    int authenticate( char *hostAddr, const char* auth_methods, CondorError* errstack, int timeout, bool non_blocking );
    //------------------------------------------
    // PURPOSE: authenticate with the other side 
    // REQUIRE: hostAddr     -- host to authenticate
	//          auth_methods -- protocols to use
	//          timeout      -- 0 for none, o.w. seconds before timing out
	//                          -1 means use existing timeout
    // RETURNS: -1 -- failure
    //           2 -- would block, continue later
    //------------------------------------------

    int authenticate( char *hostAddr, KeyInfo *& key, const char* auth_methods, CondorError* errstack, int timeout, bool non_blocking );
    int authenticate_continue( CondorError* errstack, bool non_blocking );

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

    const char* getFQAuthenticatedName();
    //------------------------------------------
    // PURPOSE: Return the full unmapped name from the authencation
    // REQUIRE: That authentication has already happened
    // RETURNS: pointer to a string you should NOT free
    //          which is the unmapped peer name, or NULL
    //          if not currently authenticated, see next.
    //------------------------------------------

    const char* getAuthenticatedName();
    //------------------------------------------
    // PURPOSE: Return the unmapped name from the authencation
    // REQUIRE: That authentication has already happened
    // RETURNS: pointer to a string you should NOT free
    //          which is the unmapped peer name, or NULL
    //          if not currently authenticated, see next.
    //------------------------------------------

    int isAuthenticated() const;
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
    
#if !defined(SKIP_AUTHENTICATION)
	static void split_canonical_name(const std::string& can_name, std::string& user, std::string& domain );
		// This version of the function exists to avoid use of MyString
		// in ReliSock, because that gets linked into std univ jobs.
		// This function is stubbed out in cedar_no_ckpt.C.
		// The user and domain variables should be freed by the caller.
	static void split_canonical_name(char const *can_name,char **user,char **domain);
#endif

	static void reconfigMapFile();

		// True if the socket failed to authenticate with the remote
		// server but may succeed with a token request workflow.
	bool shouldTryTokenRequest() const { return m_should_try_token_request; }

		// Return the current global map file
	static MapFile* getGlobalMapFile() { return global_map_file; }

 private:
#if !defined(SKIP_AUTHENTICATION)
    Authentication() {}; //should never be called, make private to help that!
    
    int handshake(const std::string& clientCanUse, bool non_blocking);
    int handshake_continue(const std::string& clientCanUse, bool non_blocking);

    int authenticate_finish( CondorError* errstack );

    int exchangeKey(KeyInfo *& key);
    
    void setAuthType( int state );
    
    int selectAuthenticationType( const std::string& my_methods, int remote_methods );

	void map_authentication_name_to_canonical_name(int authentication_type, const char* method_string, const char* authentication_name);

#endif /* !SKIP_AUTHENTICATION */
    
    int authenticate_inner( char *hostAddr, const char* auth_methods, CondorError* errstack, int timeout, bool non_blocking);
    
    //------------------------------------------
    // Data (private)
    //------------------------------------------
    Condor_Auth_Base *   authenticator_;    // This is it.
    ReliSock         *   mySock;
    int                  auth_status;
	int         m_method_id;
    char*                method_used;
	std::string	m_method_name;
	std::string	m_methods_to_try;
	std::string	m_host_addr;
	Condor_Auth_Base *m_auth;
	KeyInfo		**m_key;
	time_t		m_auth_timeout_time;
	bool		m_continue_handshake;
	bool		m_continue_auth;
	bool		m_should_try_token_request{false};

	static MapFile* global_map_file;
	static bool global_map_file_load_attempted;

};

extern char const *UNMAPPED_DOMAIN;
extern char const *MATCHSESSION_DOMAIN;
extern char const *UNAUTHENTICATED_FQU;
extern char const *UNAUTHENTICATED_USER;

/* This is the hard-coded name of the startd (and starter) as seen by
   the schedd and shadow when using non-negotiated security sessions
   based on the claim id. */
extern char const *EXECUTE_SIDE_MATCHSESSION_FQU;

/* This is the hard-coded name of the shadow as seen by the starter
   when using non-negotiated security sessions based on the claim
   id. */
extern char const *SUBMIT_SIDE_MATCHSESSION_FQU;

/* This is the hard-coded name of the negotiator as seen by the schedd
 * when using non-negotiated security sessions based on the schedd's
 * generated capabilities. */
extern char const *NEGOTIATOR_SIDE_MATCHSESSION_FQU;

/* This is the hard-coded name of the collector as seen by the schedd
 * when using non-negotiated security sessions.
 */
extern char const *COLLECTOR_SIDE_MATCHSESSION_FQU;

extern char const *CONDOR_CHILD_FQU;
extern char const *CONDOR_PARENT_FQU;
extern char const *CONDOR_FAMILY_FQU;

extern char const *AUTH_METHOD_FAMILY;
extern char const *AUTH_METHOD_MATCH;

#endif /* AUTHENTICATION_H */


