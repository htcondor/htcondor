/***************************************************************
*
* Copyright (C) 1990-2008, Condor Team, Computer Sciences Department,
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

#ifndef _IPV6_INTERFACE_H
#define _IPV6_INTERFACE_H

// Returns scope id for the link-local address of the local machine.
// If there are multiple link-local addresses and NETWORK_INTERFACE
// doesn't select one as the preferred address, then a random one
// is selected.
// The scope id should only be used if the destination of a network
// connection is an IPv6 link-local address.
// If a machine has multiple link-local addresses and the admin wants
// to use one with Condor, they should set NETWORK_INTERFACE to indicate
// which one to use.
//
// scope_id is only valid for IPv6 address
uint32_t ipv6_get_scope_id();

#endif //_IPV6_INTERFACE_H
