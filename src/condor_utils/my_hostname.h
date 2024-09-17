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

#include "stream.h"
#include <string>
#include <set>
#include "condor_sockaddr.h"

class CondorError;

/* Check whether our networking configuration can work with the network
 * interfaces present on the local system.
 */
bool validate_network_interfaces( CondorError * errorStack );

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
 */
bool network_interface_to_sockaddr(
	char const *interface_param_name,
	char const *interface_pattern,
	condor_sockaddr & ipv4,
	condor_sockaddr & ipv6,
	condor_sockaddr & ipbest);

#endif /* MY_HOSTNAME_H */
