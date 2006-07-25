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

#ifndef CONDOR_AUTHENTICATOR_CLAIM
#define CONDOR_AUTHENTICATOR_CLAIM

#if !defined(SKIP_AUTHENTICATION)

#include "condor_auth.h"        // Condor_Auth_Base class is defined here

class Condor_Auth_Claim : public Condor_Auth_Base {
 public:
    Condor_Auth_Claim(ReliSock * sock);
    //------------------------------------------
    // Constructor
    //------------------------------------------

    ~Condor_Auth_Claim();
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

 private:

};

#endif

#endif
