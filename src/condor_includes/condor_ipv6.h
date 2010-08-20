/*
 *  condor_ipv6.h
 *  condor
 *
 *  Created by minjae on 1/22/10.
 *  Copyright 2010 University of Wisconsin, Madison. All rights reserved.
 *
 */

int condor_connect(int sockfd, const ipaddr* addr);
int condor_accept(int sockfd, ipaddr* addr);
int condor_bind(int sockfd, const ipaddr* addr);

int condor_getnameinfo (const ipaddr* addr,
				char * __host, socklen_t __hostlen, 
				char * __serv, socklen_t __servlen,
				unsigned int __flags);


int condor_getaddrinfo(const char *node,
                const char *service,
                const addrinfo *hints,
                addrinfo **res);

// The syntax of IP address
// from http://www.opengroup.org/onlinepubs/000095399/functions/inet_ntop.html

/*
If the af argument of inet_pton() is AF_INET, the src string shall be in the standard IPv4 dotted-decimal form:

ddd.ddd.ddd.ddd

where "ddd" is a one to three digit decimal number between 0 and 255 (see inet_addr()). The inet_pton() function does not accept other formats (such as the octal numbers, hexadecimal numbers, and fewer than four numbers that inet_addr() accepts).

[IP6]  If the af argument of inet_pton() is AF_INET6, the src string shall be in one of the following standard IPv6 text forms:

The preferred form is "x:x:x:x:x:x:x:x", where the 'x' s are the hexadecimal values of the eight 16-bit pieces of the address. Leading zeros in individual fields can be omitted, but there shall be at least one numeral in every field.

A string of contiguous zero fields in the preferred form can be shown as "::". The "::" can only appear once in an address. Unspecified addresses ( "0:0:0:0:0:0:0:0" ) may be represented simply as "::".

A third form that is sometimes more convenient when dealing with a mixed environment of IPv4 and IPv6 nodes is "x:x:x:x:x:x:d.d.d.d", where the 'x' s are the hexadecimal values of the six high-order 16-bit pieces of the address, and the 'd' s are the decimal values of the four low-order 8-bit pieces of the address (standard IPv4 representation).
*/


// it converts given text string to ipaddr.
// it automatically detects whether given text is ipv4 or ipv6.
// returns 0 if it is not ip address at all
// returns 1 if succeeded
int condor_inet_pton(const char* src, ipaddr* dest);

//void condor_freeaddrinfo(condor_addrinfo *res);