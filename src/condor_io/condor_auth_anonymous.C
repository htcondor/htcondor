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

#include "condor_common.h"
#include "CondorError.h"

#if !defined(SKIP_AUTHENTICATION)

const char STR_ANONYMOUS[]  = "CONDOR_ANONYMOUS_USER";

#include "condor_auth_anonymous.h"

Condor_Auth_Anonymous :: Condor_Auth_Anonymous(ReliSock * sock)
    : Condor_Auth_Claim(sock)
{
}

Condor_Auth_Anonymous :: ~Condor_Auth_Anonymous()
{
}

int Condor_Auth_Anonymous :: authenticate(const char * remoteHost, CondorError* errstack)
{
    int retval = 0;
    
    // very simple for right now, server just set the remote user to
    // be anonymous directly
    if ( mySock_->isClient() ) {
        mySock_->decode();
        mySock_->code( retval );
        mySock_->end_of_message();
    } 
    else { //server side
        setRemoteUser( STR_ANONYMOUS );
        mySock_->encode();
        retval = 1;
        mySock_->code( retval );
        mySock_->end_of_message();
    }
    
    return retval;
}

#endif
