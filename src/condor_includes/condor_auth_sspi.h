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

#ifndef CONDOR_AUTH_NTSSPI
#define CONDOR_AUTH_NTSSPI

#if !defined(SKIP_AUTHENTICATION) && defined(WIN32)

#define	SECURITY_WIN32 1
#include "condor_auth.h"        // Condor_Auth_Base class is defined here
#include "sspi.NT.h"

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

 private:
    sspi_client_auth( CredHandle&    cred, 
                      CtxtHandle&    cliCtx, 
                      const char *   tokenSource );
    //------------------------------------------
    // PURPOSE: 
    // REQUIRE:
    // RETURNS:
    //------------------------------------------

    sspi_server_auth(CredHandle& cred,CtxtHandle& srvCtx);
    //------------------------------------------
    // PURPOSE: 
    // REQUIRE:
    // RETURNS:
    //------------------------------------------

    //------------------------------------------
    // Data (private)
    //------------------------------------------

    static PSecurityFunctionTable pf;
};

#endif

#endif
