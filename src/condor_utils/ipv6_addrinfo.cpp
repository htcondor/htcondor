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
#include "ipv6_addrinfo.h"
#include "condor_netdb.h"
#include "MyString.h"
#include "condor_ipv6.h"

addrinfo get_default_hint()
{
	addrinfo ret;
	memset(&ret, 0, sizeof(ret));
#ifndef WIN32
	ret.ai_flags = AI_ADDRCONFIG;
#endif
	ret.ai_flags |= AI_CANONNAME;

	ret.ai_socktype = SOCK_STREAM;
	ret.ai_protocol = IPPROTO_TCP;
	if(_condor_is_ipv6_mode()) {
		ret.ai_family = AF_UNSPEC;
	} else {
		ret.ai_family = AF_INET;
	}
	return ret;
}

// - m.
// addrinfo_iterator has two features.
// 1. it works as an iterator
// 2. it works as a shared pointer
//

struct shared_context
{
	shared_context() : count(0), head(NULL) {}
	int count;
	addrinfo* head;
	void add_ref() {
	    count++;
    }
	void release() {
	    count--;
	    if (!count && head) {
	        freeaddrinfo(head);
            delete this;
	    }
    }
};

addrinfo_iterator::addrinfo_iterator() : cxt_(NULL),
	current_(NULL)
{
}

addrinfo_iterator::addrinfo_iterator(const addrinfo_iterator& rhs) :
	cxt_(rhs.cxt_), current_(NULL)
{
	if (cxt_) cxt_->add_ref();
}

addrinfo_iterator::addrinfo_iterator(addrinfo* res) : cxt_(new shared_context),
	current_(NULL)
{
	cxt_->add_ref();
	cxt_->head = res;
}

addrinfo_iterator::~addrinfo_iterator()
{
	if (cxt_) {
		cxt_->release();
	}
}

addrinfo_iterator& addrinfo_iterator::operator= (const addrinfo_iterator& rhs)
{
	if (cxt_) cxt_->release();
	cxt_ = rhs.cxt_;
	cxt_->add_ref();

	current_ = NULL;
	return *this;
}

addrinfo* addrinfo_iterator::next()
{
	if (!current_)
		current_ = cxt_->head;
	else if (!current_->ai_next)
		return NULL;
	else
		current_ = current_->ai_next;
	return current_;
}

void addrinfo_iterator::reset()
{
	current_ = NULL;
}

bool find_any_ipv4(addrinfo_iterator& ai, sockaddr_in& sin)
{
	while ( addrinfo* r = ai.next() )
		if ( r->ai_family == AF_INET ) {
			memcpy(&sin, r->ai_addr, r->ai_addrlen);
			return true;
		}
	return false;
}


int ipv6_getaddrinfo(const char *node, const char *service,
		addrinfo_iterator& ai, const addrinfo& hint)
{
	addrinfo* res = NULL;
	int e = getaddrinfo(node, service, &hint, &res );
	if (e!=0) return e;
	ai = addrinfo_iterator(res);
	return 0;
}

/*
addrinfo_iterator ipv6_getaddrinfo(const char *node, const char *service)
{
	int e;
	addrinfo_iterator ret;
	e = ipv6_getaddrinfo(node, service, ret);
	return ret;
}

addrinfo_iterator ipv6_getaddrinfo2(const char *node, int port_no)
{
	MyString port_str;
	port_str.sprintf("%d", port_no);
	return ipv6_getaddrinfo(node, port_str.Value());
}
*/

/*
std::string ipv6_inet_ntop(int family, const void* addr)
{
	std::string ret;
	ret.resize(IP_STRING_BUF_SIZE);
	// it will work with most STL implementation
	// however, it would not work with some non-standard STL implementation
	// that implements copy-on-write string.
	char* buf = const_cast<char*>(ret.c_str());

	const char* result = inet_ntop(family, addr, buf, IP_STRING_BUF_SIZE);
	if (!result)
		return ""; // return empty string
	ret.resize( strlen(buf) );
	return ret;
}

std::string ipv6_inet_ntop(struct sockaddr* sa)
{
	if (sa->sa_family != AF_INET) {
		//dprintf( D_ALWAYS, "ipv6_inet_ntop() called with non-AF_INET sockaddr");
		return "";
	}
	struct sockaddr_in* sin = (struct sockaddr_in*)sa;
	return ipv6_inet_ntop(sin->sin_family, &sin->sin_addr);

}
*/
