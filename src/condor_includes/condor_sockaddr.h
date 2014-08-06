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


#ifndef CONDOR_SOCKADDR_H
#define CONDOR_SOCKADDR_H

#include "MyString.h"

enum condor_protocol { CP_INVALID_MIN, CP_IPV4, CP_IPV6, CP_INVALID_MAX };
// Return a human friendly(ish) name for a protocol. Suitable for
// use in log messages.
MyString condor_protocol_to_str(condor_protocol p);

class condor_sockaddr 
{
	union {
		// sockaddr_in6 and sockaddr_in structure differs from OS to OS.
		// Mac OS X has a length field while Linux usually does not have 
		// the length field.
		//
		// However,
		// sin_family and sin6_family should be located at same offset.
		// after *_family field it may diverges
		sockaddr_in6 v6;
		sockaddr_in v4;
		sockaddr_storage storage;
	};
	
public:
	condor_sockaddr();
	
	// int represented ip should be network-byte order.
	// however, port is always host-byte order.
	//
	// the reason is that in Condor source code, it does not convert
	// network-byte order to host-byte order in IP address
	// but convert so in port number.
	/*condor_sockaddr(int ip, unsigned short port = 0);*/
	condor_sockaddr(in_addr ip, unsigned short port = 0);
	condor_sockaddr(const in6_addr& ipv6, unsigned short port = 0);
	condor_sockaddr(const sockaddr* saddr);
	condor_sockaddr(const sockaddr_in* sin) ;
	condor_sockaddr(const sockaddr_in6* sin6);
	condor_sockaddr(const sockaddr_storage* sin);

private:
	void init(uint32_t ip, unsigned port);
public:

	// the caller is responsible for checking is_ipv4().
	sockaddr_in to_sin() const;
	sockaddr_in6 to_sin6() const;
	bool is_ipv4() const;
	bool is_ipv6() const;

	// returns true if the ip address is link local.
	// for IPv4, it denotes 169.254.0.0/16
	// for IPv6, it denotes fe80::/10
	bool is_link_local() const;

	// set ip version when you want to bind the address to a socket
	void set_protocol(condor_protocol proto);
	void set_ipv4();
	void set_ipv6();

	// addr_any and loopback involve protocol dependent constant
	// like INADDR_ANY, IN6ADDR_ANY, ...
	//
	// so, it would be desirable to hide protocol dependent constant
	// but expose general concept like addr_any or loopback

	bool is_addr_any() const;
	void set_addr_any();
	bool is_loopback() const;
	void set_loopback();
	void set_port(unsigned short port);
	unsigned short get_port() const;

	// sets the sin6_scope_id field in sockaddr_in6
	// ipv6 only
	void set_scope_id(uint32_t scope_id);

	bool from_ip_string(const MyString& ip_string);
	bool from_ip_string(const char* ip_string);

		// sinful string could contain either IP address or hostname.
		// from_sinful() calls gethostbyname to resolve DNS name to IP addr.
	bool from_sinful(const MyString& ip_string);
	bool from_sinful(const char* sinful);
	MyString to_sinful() const;
	const char* to_sinful(char* buf, int len) const;

		// returns IP address string as it is. (i.e. not returning local ip
		// address when inaddr_any)
		
		// if it fails on inet_ntop(), returns blank string.
	MyString to_ip_string() const;
		// it it fails on inet_ntop(), returns NULL and given buf
		// will not be modified.
	const char* to_ip_string(char* buf, int len) const;

		// if it contains loopback address, it will return
		// local ip address.
	MyString to_ip_string_ex() const; 
	const char* to_ip_string_ex(char* buf, int len) const;

	// if the address contained is ipv4, it converts to 
	// IPv6-V4MAPPED address. caller must check is_ipv4() first.
	void convert_to_ipv6();

	void clear();

	// returns as sockaddr_storage. 
	sockaddr_storage to_storage() const;

	// check sockaddr.sa_family before use (i.e. type-cast to sockaddr_in)
	const sockaddr* to_sockaddr() const;
	socklen_t get_socklen() const;

	bool is_valid() const;

		// returns true if the ip address is private. (e.g. 10.0.0.0/24)
	bool is_private_network() const;

		// returns ipv6 address.
		// if stored address is IPv4, it returns IPv4-mapped IPv6 address.
		// e.x. 137.10.0.1 --> ::FFFF:137.10.0.1
	in6_addr to_ipv6_address() const;

		// returns raw pointer to the address array.
		// if the stored address is IPv4, returned address is single uint32_t.
		// if it is IPv6, it returns uint32_t [4]
	const uint32_t* get_address() const;

		// returns length of the address array
		// if the stored address is IPv4, returns 1
		// if it is IPv6, returns 4
	int get_address_len() const;

		// returns AF_INET if ipv4, AF_INET6 if ipv6, AF_UNSPEC if unknown.
	int get_aftype() const;

		// only compares address, ignores port number.
		// returns true if same.
	bool compare_address(const condor_sockaddr& addr) const;

		// use it when you want a null place holder.
	static condor_sockaddr null;

	bool operator<(const condor_sockaddr& rhs) const;
	bool operator==(const condor_sockaddr& lhs) const;
	bool operator!=(const condor_sockaddr& rhs) const {
		return !operator ==(rhs);
	}
};

#endif // CONDOR_SOCKADDR_H
