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
		addrinfo*& ai, const addrinfo& hint)
{
	double begin = _condor_debug_get_time_double();
	int e = getaddrinfo(node, service, &hint, &ai );
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
	return 0;
}

