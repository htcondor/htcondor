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


#ifndef CONDOR_AUTH_NTSSPI
#define CONDOR_AUTH_NTSSPI

#if !defined(SKIP_AUTHENTICATION) && defined(WIN32)

#define	SECURITY_WIN32 1
#include "condor_auth.h"        // Condor_Auth_Base class is defined here

/* no longer needed under the new SDK 
#include "sspi.NT.h" */

#include <sspi.h>

class Condor_Auth_SSPI : public Condor_Auth_Base {
 public:
    Condor_Auth_SSPI(ReliSock * sock);
    //------------------------------------------
    // Constructor
    //------------------------------------------

    ~Condor_Auth_SSPI();
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

	int wrap(	char *   input, 
				int      input_len, 
				char* &  output, 
				int &    output_len);

	int unwrap(	char *   input, 
				int      input_len, 
				char* &  output, 
				int &    output_len);

 private:
    int sspi_client_auth( CredHandle&    cred, 
						  CtxtHandle&    cliCtx, 
						  const char *   tokenSource );
    //------------------------------------------
    // PURPOSE: 
    // REQUIRE:
    // RETURNS:
    //------------------------------------------

    int sspi_server_auth(CredHandle& cred,CtxtHandle& srvCtx);
    //------------------------------------------
    // PURPOSE: 
    // REQUIRE:
    // RETURNS:
    //------------------------------------------

    //------------------------------------------
    // Data (private)
    //------------------------------------------

    static PSecurityFunctionTable pf;
	CtxtHandle theCtx;
};

#endif

#endif
