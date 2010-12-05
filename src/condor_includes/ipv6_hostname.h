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

#ifndef IPV6_HOSTNAME_H
#define IPV6_HOSTNAME_H

#include "MyString.h"
#include <vector>
#include "condor_ipaddr.h"

void init_local_hostname();

ipaddr get_local_ipaddr();
MyString get_local_hostname();
MyString get_local_fqdn();

MyString get_hostname(const ipaddr& addr);
std::vector<MyString> get_hostname_with_alias(const ipaddr& addr);
MyString get_full_hostname(const ipaddr& addr);

std::vector<ipaddr> resolve_hostname(const MyString& hostname);
std::vector<ipaddr> resolve_hostname(const char* hostname);

// IPv6-compliant version of convert_ip_to_hostname and
// convert_hostname_to_ip.
MyString convert_ipaddr_to_hostname(const ipaddr& addr);
ipaddr convert_hostname_to_ipaddr(const MyString& fullname);

#endif // IPV6_HOSTNAME_H
