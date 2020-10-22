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

//#include <string>

struct shared_context;
// an iterator and a smart pointer for addrinfo structure
class addrinfo_iterator
{
public:
	addrinfo_iterator();
	addrinfo_iterator(addrinfo* res);
	addrinfo_iterator(const addrinfo_iterator& rhs);
	~addrinfo_iterator();
	addrinfo* next();
	addrinfo_iterator& operator= (const addrinfo_iterator& rhs);
	addrinfo_iterator & operator=(addrinfo_iterator &&rhs) noexcept ;
	void reset();
protected:
	shared_context* cxt_;
	addrinfo* current_;
};

// return with AI_ADDRCONFIG
addrinfo get_default_hint();

// will use default hint
int ipv6_getaddrinfo(const char *node, const char *service,
		addrinfo_iterator& ai, const addrinfo& hint = get_default_hint());

bool find_any_ipv4(addrinfo_iterator& ai, sockaddr_in& sin);

#endif // IPV6_ADDRINFO_H
