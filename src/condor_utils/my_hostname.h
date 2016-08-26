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

#ifndef MY_HOSTNAME_H
#define MY_HOSTNAME_H

// use get_local_ipaddr().to_ip_string() instead
extern	const char*	my_ip_string( void );

#include "stream.h"
#include <string>
#include <set>

class CondorError;
bool init_network_interfaces( CondorError * errorStack );

// If the specified attribute name is recognized as an attribute used
// to publish a daemon IP address, this function replaces any
// reference to the default host IP with the actual connection IP in
// the attribute's expression string.

// You might consider this a dirty hack (and it is), but of the
// methods that were considered, this was the one with the lowest
// maintainance, least overhead, and least likelihood to have
// unintended side-effects.

void ConvertDefaultIPToSocketIP(char const *attr_name,std::string &expr_string,Stream& s);


void ConfigConvertDefaultIPToSocketIP();

// This interface to ConvertDefaultIPToSocketIP() takes a std::string
// and modifies its contents.

/* Find local addresses that match a given NETWORK_INTERFACE
 *
 * Prefers public addresses.  Strongly prefers "up" addresses.
 *
 * interface_param_name - Optional, but recommended, exclusively used 
 *        in log messages. Probably "NETWORK_INTERFACE" or "PRIVATE_NETWORK_INTERFACE"
 * interface_pattern - Required. The value of the param.  Something like
 *        "*" or "192.168.0.23"
 * ipv4 - best ipv4 address found. May be empty if no ipv4 addresses are found
 *        that match the interface pattern.
 * ipv6 - best ipv6 address found. May be empty if no ipv6 addresses are found
 *        that match the interface pattern.
 * ipbest - If you absolutely need a single result, this is our best bet.
 *        But really, just don't do that. Should be one of ipv4 or ipv6.
 * network_interface_ips - Optional. A list of every IP address, v4 or v6,
 *        that matches the interface pattern.
 */
bool network_interface_to_ip(
	char const *interface_param_name,
	char const *interface_pattern,
	std::string & ipv4,
	std::string & ipv6,
	std::string & ipbest,
	std::set< std::string > *network_interface_ips = NULL);

#endif /* MY_HOSTNAME_H */
