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


#ifndef CONDOR_AUTHENTICATOR_FILESYSTEM
#define CONDOR_AUTHENTICATOR_FILESYSTEM

#include "condor_auth.h"        // Condor_Auth_Base class is defined here

class Condor_Auth_FS : public Condor_Auth_Base {
 public:
    Condor_Auth_FS(ReliSock * sock, int remote = 0);
    //------------------------------------------
    // Constructor
    //------------------------------------------

    ~Condor_Auth_FS();
    //------------------------------------------
    // Destructor
    //------------------------------------------

    int authenticate(const char * remoteHost, CondorError* errstack, bool non_blocking);
    int authenticate_continue(CondorError* /*errstack*/, bool /*non_blocking*/);

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

	std::string m_new_dir;
    int    remote_;
};

#endif
