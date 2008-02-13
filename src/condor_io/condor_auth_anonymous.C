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

int Condor_Auth_Anonymous :: authenticate(const char * /* remoteHost */, CondorError* /* errstack */)
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
		setAuthenticatedName( STR_ANONYMOUS );
        mySock_->encode();
        retval = 1;
        mySock_->code( retval );
        mySock_->end_of_message();
    }
    
    return retval;
}

#endif
