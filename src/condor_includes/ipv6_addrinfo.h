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


#ifndef IPV6_ADDRINFO_H
#define IPV6_ADDRINFO_H

// return with AI_ADDRCONFIG
addrinfo get_default_hint();

// will use default hint
int ipv6_getaddrinfo(const char *node, const char *service,
		addrinfo*& ai, const addrinfo& hint = get_default_hint());

#endif // IPV6_ADDRINFO_H
