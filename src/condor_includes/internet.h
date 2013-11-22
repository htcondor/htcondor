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

/*
** These are functions for generating internet addresses
** and internet names
**
**             Author : Dhrubajyoti Borthakur
**               28 July, 1994
*/

#ifndef INTERNET_H
#define INTERNET_H


#if !defined(SOCKET) && !defined(WIN32)
#define SOCKET int
#endif

/* maximum length of a machine name */
#define  MAXHOSTLEN     1024

#if defined(__cplusplus)
#include "MyString.h"
extern "C" {
#endif

/* Convert "<xx.xx.xx.xx:pppp?params>" to a sockaddr_in  TCP */
//int string_to_sin(const char *addr, struct sockaddr_in *s_in);

/* Split "<host:port?params>" into parts: host, port, and params. If
   the port or params are not in the string, the result is set to
   NULL.  Any of the result char** values may be NULL, in which case
   they are parsed but not set.  The caller is responsible for freeing
   all result strings.
*/
int split_sin( const char *addr, char **host, char **port, char **params );

//int
//address_to_sin(char const *host, char const *port, struct sockaddr_in *sa_in);

/* Like inet_aton(), but works in Windows too.  (Returns 0 on failure.) */
//int condor_inet_aton(const char *ipstr, struct in_addr *result);

//char *sin_to_string(const struct sockaddr_in *s_in);

/* Convenience function for calling inet_ntoa().
   Buffer must be big enough for address (IP_STRING_BUF_SIZE).
   Returns NULL on failure; otherwise returns buf
*/
char *sin_to_ipstring(const struct sockaddr_in *sa_in,char *buf,size_t buflen);

/* Extract the port from a string of the form "<xx.xx.xx.xx:pppp>" */
int string_to_port( const char* addr );

/* Extract the ip_addr from a string of the form "<xx.xx.xx.xx:pppp>"
   and convert it to the unsigned int version from the ASCII version */
//unsigned int string_to_ip( const char* addr );

/* Convert a hostname[:port] to sinful string */
char * hostname_to_string (const char * hostname, const int default_port );

const char *sock_to_string(SOCKET sockd);

/*
 Puts socket peer's sinful address in buf.  Returns this value or the
 specified unknown value if address could not be determined.
 Buffer should be at least SINFUL_STRING_BUF_SIZE,
 but if it is less, the value will be truncated if necessary.
*/
char const *
sock_peer_to_string( SOCKET fd, char *buf, size_t buflen, char const *unknown);

/* Return the real hostname of a machine given a sin; return NULL if it cannot
 * be found or error.  Also return aliases. */
//char *sin_to_hostname(const struct sockaddr_in *s_in, char ***aliases);

//void
//display_from( struct sockaddr_in *from );

/* Returns 1 if h1 and h2 are both hostnames which refer to the same
   host, 0 if they don't, and -1 on error. */
int same_host(const char *h1, const char *h2);

/* Returns TRUE if hostname belongs to the given domain, FALSE if not */
int host_in_domain(const char *host, const char *domain);

int is_ipv4_addr_implementation(const char *inbuf, struct in_addr *sin_addr,
			struct in_addr *mask_addr,int allow_wildcard);

int is_valid_sinful( const char *sinful );

/* returns the port integer from a given address, or -1 if there is
   none. */
int getPortFromAddr( const char* addr );

/* returns only the host information (not including '<', '>', ":2132",
   etc) from a given address.  the string returned is a newly
   allocated string which must be de-allocated with free(). */
char* getHostFromAddr( const char* addr );

/* Deprecated!  The claim id should no longer be used to get the st artd
   address, except for cases where this is required for backward
   compatibility.  The startd ClassAd should be fed into a DCDaemon
   object to get the address of the startd, because this handles
   private network addresses etc.
   returns the address from a given ClaimId.  the string returned is a
   newly allocated string which must be de-allocated with free().
*/
char* getAddrFromClaimId( const char* id );

/* Binds the given fd to any port on the correct local interface for
   this machine.   is_outgoing tells it how to param.  Returns 1 if
   successful, 0 on error. */
int _condor_local_bind( int is_outgoing, int fd );

//int is_priv_net(uint32_t ip);
//int is_loopback_net(uint32_t ip);
//int is_loopback_net_str(char const *ip);
//int in_same_net(uint32_t ipA, uint32_t ipB);
//char *ipport_to_string(const unsigned int ip, const unsigned short port);
char *prt_fds(int maxfd, fd_set *fds);

/* Get the address that a socket is bound to */
// @ret: NULL if failed; non-NULL if succeed
//struct sockaddr_in *getSockAddr(int sockfd);

// generates sinful string.
// it detects whether given ip address is IPv4 or IPv6.
int generate_sinful(char* buf, int len, const char* ip, int port);

#if defined(__cplusplus)
}
// MyString version is C++ only
MyString generate_sinful(const char* ip, int port);

/* Extract the IP address from a sinful string ("<xx.xx.xx.xx:pppp>")
   and return it in ipout as an ASCII string ("xx.xx.xx.xx") and returns
   true.  If the IP address is invalid, returns false and ipout is
   left unchanged. */
bool sinful_to_ipstr(const char * addr, MyString & ipout);
#endif

#endif /* INTERNET_H */
