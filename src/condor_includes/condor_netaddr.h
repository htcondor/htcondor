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
//
// Also, this is replacement for is_valid_network() in internet.cpp

class condor_netaddr
{
	condor_sockaddr base_;
	unsigned int maskbit_;
public:
	condor_netaddr(const condor_sockaddr& base, unsigned int maskbit);
	condor_netaddr();
	bool match(const condor_sockaddr& target) const;

	// it accepts following variants
	// [IPv4 form]
	// 128.105.*
	// 128.105.0.0/255.255.0.0
	// 128.105.0.0/16
	//
	// [IPv6 form] only allows IPv6 Address/#Mask
	// FE8F:1234::/60
	//
	// please refer RFC 4291 for IPv6 network representation.
	bool from_net_string(const char* net);
};

#endif // CONDOR_NETADDR_H
