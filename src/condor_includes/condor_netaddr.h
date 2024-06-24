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

#ifndef CONDOR_NETADDR_H
#define CONDOR_NETADDR_H

#include "condor_sockaddr.h"

// condor_netaddr represents single network in single class.
// for example, it can represent 128.105.0.0/24 network.
//
// the purpose of the class is to have all the relevant functions in
// single place.

class condor_netaddr
{
	condor_sockaddr base_;
	condor_sockaddr mask_;
	unsigned int maskbit_;
	bool matchesEverything;

	void set_mask();

public:
	condor_netaddr();
	condor_netaddr(const condor_sockaddr& base, unsigned int maskbit);
	bool match(const condor_sockaddr& target) const;

	//
	// This function is (primarily?) used by the ipverify infrastructure.
	// As such, it -- and this class -- must support IP literals.  An IP
	// literal may be an IPv4 literal or an IPv6 literal.  An IPv4 network
	// may be specified as an incompleted dotted quad with a single asterisk
	// (*) in place of a the rightmost quad; as a dotted quad followed by a
	// slash follwed by the dotted quad of the netmask; or as a dotted quad
	// followed by a slash followed by an integer specifying the number of
	// mask bits.  An IPv6 network may be specified as an IPv6 literal
	// followed by a slash followed by an integer specifying the number of
	// mask bits; or as an IPv6 literal with the second of its trailing
	// colons replaced by a star. When a full IPv6 literal is used (with
	// or without a slash and mask bits), the use of square brackets is
	// allowed but optional. Examples:
	//
	// 128.104.100.22
	//
	// 128.104.*
	// 128.104.0.0/255.255.0.0
	// 128.104.0.0/16
	//
	// 2607:f388:107c:501:1b:21ff:feca:51f0
	// [2607:f388:107c:501:1b:21ff:feca:51f0]
	// 2607:f388:107c:501::/60
	// [2607:f388:107c:501::]/60
	// 2607:f388:107c:501:*
	//
	bool from_net_string(const char* net);
};

#endif // CONDOR_NETADDR_H
