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
	if ( param_false( "ENABLE_IPV6" ) ) {
		ret.ai_family = AF_INET;
	} else if ( param_false( "ENABLE_IPV4" ) ) {
		ret.ai_family = AF_INET6;
	} else {
		ret.ai_family = AF_UNSPEC;
	}
	ret.ai_socktype = SOCK_STREAM;
	ret.ai_protocol = IPPROTO_TCP;
	return ret;
}

// - m.
// addrinfo_iterator has two features.
// 1. it works as an iterator
// 2. it works as a shared pointer
//

struct shared_context
{
	shared_context() : count(0), head(NULL), was_duplicated(false) {}
	int count;
	addrinfo* head;
	bool was_duplicated;
	void add_ref() {
	    count++;
    }
	void release() {
	    count--;
	    if (!count && head) {
	    	if(! was_duplicated) {
	        	freeaddrinfo(head);
	        } else {
	        	addrinfo * next = NULL;
	        	addrinfo * current = head;
	        	while( current != NULL ) {
	        		next = current->ai_next;
	        		if( current->ai_addr ) { free( current->ai_addr ); }
	        		if( current->ai_canonname ) { free( current->ai_canonname ); }
	        		free( current );
	        		current = next;
	        	}
	        }
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

addrinfo * aidup( addrinfo * ai ) {
	if( ai == NULL ) { return NULL; }
	addrinfo * rv = (addrinfo *)malloc( sizeof( addrinfo ) );
	ASSERT( rv );
	* rv = * ai;
	if( rv->ai_addr ) {
		rv->ai_addr = (sockaddr *)malloc( rv->ai_addrlen );
		ASSERT( rv->ai_addr );
		memcpy( rv->ai_addr, ai->ai_addr, rv->ai_addrlen );
	}
	if( rv->ai_canonname ) {
		rv->ai_canonname = strdup( ai->ai_canonname );
		ASSERT( rv->ai_canonname );
	}
	rv->ai_next = NULL;
	return rv;
}

#include "condor_sockaddr.h"

addrinfo * deepCopyAndSort( addrinfo * res, bool preferIPv4 ) {
	addrinfo * r = NULL;
	addrinfo * v4 = NULL;
	addrinfo * v6 = NULL;
	addrinfo * v4head = NULL;
	addrinfo * v6head = NULL;

	for( addrinfo * current = res; current != NULL; current = current->ai_next ) {
		if( current->ai_family == AF_INET ) {
			if( v4 == NULL ) { v4head = v4 = aidup( current ); }
			else { v4->ai_next = aidup( current ); v4 = v4->ai_next; }
		} else if( current->ai_family == AF_INET6 ) {
			if( v6 == NULL ) { v6head = v6 = aidup( current ); }
			else { v6->ai_next = aidup( current ); v6 = v6->ai_next; }
		} else {
			dprintf( D_NETWORK, "Ignoring address with family %d, which is neither IPv4 nor IPv6.\n", current->ai_family );
		}
	}

	if( preferIPv4 ) {
		if( v4head ) {
			r = v4head;
			v4->ai_next = v6head;
		} else {
			r = v6head;
		}
	} else {
		if( v6head ) {
			r = v6head;
			v6->ai_next = v4head;
		} else {
			r = v4head;
		}
	}

	// getaddrinfo() promises that ai_canonname will be set in the
	// first addrinfo structure (and implicitly, nowhere else).
	for( addrinfo * c = r; c != NULL; c = c->ai_next ) {
		if( c->ai_canonname ) {
			// Need a temporary in case we get only one result.
			char * canonname = c->ai_canonname;
			c->ai_canonname = NULL;
			r->ai_canonname = canonname;
			break;
		}
	}

	return r;
}

addrinfo_iterator::addrinfo_iterator(addrinfo* res) : cxt_(new shared_context),
	current_(NULL)
{
	cxt_->add_ref();
	cxt_->head = res;

	if( param_boolean( "IGNORE_DNS_PROTOCOL_PREFERENCE", true ) ) {
		dprintf( D_HOSTNAME, "DNS returned:\n" );
		for( addrinfo * c = res; c != NULL; c = c->ai_next ) {
			condor_sockaddr sa( c->ai_addr );
			dprintf( D_HOSTNAME, "\t%s\n", sa.to_ip_string().c_str() );
		}

		// It seems dangerous to reorder the linked list, so instead
		// make a (deep) copy of each element as we sort it onto one
		// of the lists above.  When we're done, staple the two lists
		// together according to the PREFER_OUTBOUND_IPV4 setting.
		//
		// Then free the original list and set the destructor to
		// free our list, instead.
		cxt_->head = deepCopyAndSort( res, param_boolean( "PREFER_OUTBOUND_IPV4", true ) );
		cxt_->was_duplicated = true;
		freeaddrinfo( res );

		dprintf( D_HOSTNAME, "We returned:\n" );
		for( addrinfo * c = cxt_->head; c != NULL; c = c->ai_next ) {
			condor_sockaddr sa( c->ai_addr );
			dprintf( D_HOSTNAME, "\t%s\n", sa.to_ip_string().c_str() );
		}
	}
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

addrinfo_iterator &
addrinfo_iterator::operator=(addrinfo_iterator &&rhs)  noexcept {
	// First, destroy my current self
	if (cxt_) cxt_->release();

	this->cxt_ = rhs.cxt_;
	rhs.cxt_ = nullptr;

	this->current_ = rhs.current_;
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
			return current_;
		default:
			//
			// ai_canonname is only ever non-NULL in the first struct addrinfo
			// returned by getaddrinfo.  If we skip that (because it's IPv6),
			// make sure the rest of the code still sees ai_canonname.  (If
			// if doesn't, it falls back on gethostbyname(), which is even
			// more broken for host with IPv6 addresses.)  We can't just
			// copy the pointer because then it will be double-free()d.
			//
			// TODO: Since we no longer filter out IPv6 entries in the
			// results of getaddrinfo() (we now do so via the hints
			// parameter), this juggling of ai_canonname is probably no
			// longer necessary.
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
	if (timediff > getaddrinfo_slow_limit) {
		dprintf(D_ALWAYS, "WARNING: Saw slow DNS query, which may impact entire system: getaddrinfo(%s) took %f seconds.\n", node, timediff);
	}
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

