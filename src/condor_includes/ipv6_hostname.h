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
#include "condor_sockaddr.h"

void init_local_hostname();

condor_sockaddr get_local_ipaddr();
MyString get_local_hostname();
MyString get_local_fqdn();

// returns fully-qualified-domain-name from given hostname.
// this is replacement of previous get_full_hostname()
//
// it will
// 1) lookup DNS by calling getaddrinfo()
// 2) if FQDN was not found in canonname of addrinfo,
//    it will look into h_alias from gethostbyname().
// 3) if FQDN still not be found, it will add DEFAULT_DOMAIN_NAME.
// 4) if DEFAULT_DOMAIN_NAME is not defined, it will return an empty string.
MyString get_fqdn_from_hostname(const MyString& hostname);

// returns 'best' IP address and fully-qualified-domain-name for given hostname
// the criteria for picking best IP address is
// 1) public IP address
// 2) private IP address
// 3) loopback address
//
// the algorithm for getting FQDN is same as get_fqdn_from_hostname()
//
// Return value:
// 0 - if failed
// 1 - if succeeded
int get_fqdn_and_ip_from_hostname(const MyString& hostname,
		MyString& fqdn, condor_sockaddr& addr);

// returns just hostname for given addr
MyString get_hostname(const condor_sockaddr& addr);

// returns a set of hostname for given addr
std::vector<MyString> get_hostname_with_alias(const condor_sockaddr& addr);

// returns a fully-qualified domain name for given addr
//
// IT IS DIFFERENT FROM PREVIOUS get_full_hostname()
MyString get_full_hostname(const condor_sockaddr& addr);

// DNS-lookup for given hostname
std::vector<condor_sockaddr> resolve_hostname(const MyString& hostname);
std::vector<condor_sockaddr> resolve_hostname(const char* hostname);

// _raw function directly calls getaddrinfo, does not do any of NO_DNS
// related handlings.
std::vector<condor_sockaddr> resolve_hostname_raw(const MyString& hostname);

// NODNS functions
//
// IPv6-compliant version of convert_ip_to_hostname and
// convert_hostname_to_ip.
MyString convert_ipaddr_to_hostname(const condor_sockaddr& addr);
condor_sockaddr convert_hostname_to_ipaddr(const MyString& fullname);

#endif // IPV6_HOSTNAME_H
