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

#include <string>
#include <vector>
#include "condor_sockaddr.h"

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

int condor_gethostname(char *name, size_t namelen);

void reset_local_hostname();

condor_sockaddr get_local_ipaddr(condor_protocol proto);
std::string get_local_hostname();
std::string get_local_fqdn();

// returns fully-qualified-domain-name from given hostname.
// this is replacement of previous get_full_hostname()
//
// it will
// 1) lookup DNS by calling getaddrinfo()
// 2) if FQDN was not found in canonname of addrinfo,
//    it will look into h_alias from gethostbyname().
// 3) if FQDN still not be found, it will add DEFAULT_DOMAIN_NAME.
// 4) if DEFAULT_DOMAIN_NAME is not defined, it will return an empty string.
std::string get_fqdn_from_hostname(const std::string& hostname);

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
int get_fqdn_and_ip_from_hostname(const std::string & hostname,
		std::string & fqdn, condor_sockaddr & addr );

// returns just hostname for given addr
std::string get_hostname(const condor_sockaddr& addr);

// returns a set of hostname for given addr
std::vector<std::string> get_hostname_with_alias(const condor_sockaddr& addr);

// returns a fully-qualified domain name for given addr
//
// IT IS DIFFERENT FROM PREVIOUS get_full_hostname()
std::string get_full_hostname(const condor_sockaddr& addr);

// DNS-lookup for given hostname
std::vector<condor_sockaddr> resolve_hostname(const std::string& hostname, std::string* canonical = nullptr);

// _raw function directly calls getaddrinfo, does not do any of NO_DNS
// related handlings.
std::vector<condor_sockaddr> resolve_hostname_raw(const std::string& hostname, std::string* canonical = nullptr);

// NODNS functions
//
// Construct fake hostnames based on an underlying IP address,
// avoiding any use of DNS.
std::string convert_ipaddr_to_fake_hostname(const condor_sockaddr& addr);
condor_sockaddr convert_fake_hostname_to_ipaddr(const std::string& fullname);

#endif // IPV6_HOSTNAME_H
