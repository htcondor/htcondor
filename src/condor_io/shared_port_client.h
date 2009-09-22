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

#ifndef __SHARED_PORT_CLIENT_H__
#define __SHARED_PORT_CLIENT_H__

#include "MyString.h"

class SharedPortClient {
 public:
	bool sendSharedPortID(char const *shared_port_id,Sock *sock);
	bool PassSocket(Sock *sock_to_pass,char const *shared_port_id,char const *requested_by=NULL);
 private:
	MyString myName();
	bool SharedPortIdIsValid(char const *name);

};

#endif
