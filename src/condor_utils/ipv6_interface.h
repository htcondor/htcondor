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

// returns scope id for the link-local address
// if NETWORK_INTERFACE was not set, it returns 0 (no scope)
//
// scope_id is only valid for IPv6 address
uint32_t ipv6_get_scope_id();

#endif //_IPV6_INTERFACE_H
