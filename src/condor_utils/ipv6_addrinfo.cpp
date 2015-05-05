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
#include "condor_config.h"
#include "ipv6_addrinfo.h"
#include "condor_netdb.h"
#include "MyString.h"
#include "condor_classad.h" // generic stats needs these definitions.
#include "generic_stats.h"

addrinfo get_default_hint()
{
	addrinfo ret;
	memset(&ret, 0, sizeof(ret));
#ifndef WIN32
	// Unfortunately, Ubuntu 10 and 12 disagree with everyone else about
	// what AI_ADDRCONFIG means, so we need to ask for all addresses and
	// filter on our end.
	// ret.ai_flags = AI_ADDRCONFIG;
#endif
	ret.ai_flags |= AI_CANONNAME;

	ret.ai_socktype = SOCK_STREAM;
	ret.ai_protocol = IPPROTO_TCP;
#if defined( WORKING_GETADDRINFO )
	if(param_boolean("ENABLE_IPV6", false)) {
		ret.ai_family = AF_UNSPEC;
	} else {
		ret.ai_family = AF_INET;
	}
#else
	ret.ai_family = AF_UNSPEC;
#endif
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
	ipv6 = param_boolean( "ENABLE_IPV6", false );
}

addrinfo_iterator::addrinfo_iterator(const addrinfo_iterator& rhs) :
	cxt_(rhs.cxt_), current_(NULL), ipv6(rhs.ipv6)
{
	if (cxt_) cxt_->add_ref();
}

addrinfo_iterator::addrinfo_iterator(addrinfo* res) : cxt_(new shared_context),
	current_(NULL)
{
	ipv6 = param_boolean( "ENABLE_IPV6", false );
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
	ipv6 = rhs.ipv6;

	current_ = NULL;
	return *this;
}

addrinfo* addrinfo_iterator::next()
{
	if( ! current_ ) {
		current_ = cxt_->head;
	} else if( ! current_->ai_next ) {
		return NULL;
	} else {
		current_ = current_->ai_next;
	}

#if defined( WORKING_GETADDRINFO )
	return current_;
#else
	switch( current_->ai_family ) {
		// Switch must match condor_sockaddr constructor.
		case AF_UNIX:
			// condor_sockaddr knows what to do with the address family,
			// so we should let it pass.  On the other hand, the idea
			// of getaddrinfo() returning a domain socket bothers me.
			return current_;
		case AF_INET:
			return current_;
		case AF_INET6:
			if( ipv6 ) { return current_; }
			// This fall-through is deliberate.
		default:
			//
			// ai_canonname is only ever non-NULL in the first struct addrinfo
			// returned by getaddrinfo.  If we skip that (because it's IPv6),
			// make sure the rest of the code still sees ai_canonname.  (If
			// if doesn't, it falls back on gethostbyname(), which is even
			// more broken for host with IPv6 addresses.)  We can't just
			// copy the pointer because then it will be double-free()d.
			//
			if( current_ == cxt_->head && cxt_->head->ai_canonname ) {
				addrinfo * hack = next();
				if( hack ) {
					hack->ai_canonname = cxt_->head->ai_canonname;
					cxt_->head->ai_canonname = NULL;
				}
				return hack;
			}
			return next();
	}
#endif
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

// these stats keep track of the cost of DNS lookups
//
#if 1
//PRAGMA_REMIND("temporarily!! publish recent windowed values for DNS lookup runtime...")
stats_entry_recent<Probe> getaddrinfo_runtime; // count & runtime of all lookups, success and fail
stats_entry_recent<Probe> getaddrinfo_fast_runtime; // count & runtime of successful lookups that were faster than getaddrinfo_slow_limit
stats_entry_recent<Probe> getaddrinfo_slow_runtime; // count & runtime of successful lookups that were slower than getaddrinfo_slow_limit
stats_entry_recent<Probe> getaddrinfo_fail_runtime; // count & runtime of failed lookups
#else
stats_entry_probe<double> getaddrinfo_runtime; // count & runtime of all lookups, success and fail
stats_entry_probe<double> getaddrinfo_fast_runtime; // count & runtime of successful lookups that were faster than getaddrinfo_slow_limit
stats_entry_probe<double> getaddrinfo_slow_runtime; // count & runtime of successful lookups that were slower than getaddrinfo_slow_limit
stats_entry_probe<double> getaddrinfo_fail_runtime; // count & runtime of failed lookups
#endif

double getaddrinfo_slow_limit = 2.0;
void (*getaddrinfo_slow_callback)(const char *node, const char *service, double timediff) = NULL;

int ipv6_getaddrinfo(const char *node, const char *service,
		addrinfo_iterator& ai, const addrinfo& hint)
{
	addrinfo* res = NULL;
	double begin = _condor_debug_get_time_double();
	int e = getaddrinfo(node, service, &hint, &res );
	double timediff = _condor_debug_get_time_double() - begin;
	getaddrinfo_runtime += timediff;
	if (e!=0) {
		getaddrinfo_fail_runtime += timediff;
		return e;
	}
	if (timediff > getaddrinfo_slow_limit) {
		getaddrinfo_slow_runtime += timediff;
		if (getaddrinfo_slow_callback) getaddrinfo_slow_callback(node, service, timediff);
	} else {
		getaddrinfo_fast_runtime += timediff;
	}
	ai = addrinfo_iterator(res);
	return 0;
}

