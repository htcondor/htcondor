/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
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
extern "C" {
#endif

/* Convert a string of the form "<xx.xx.xx.xx:pppp>" to a sockaddr_in  TCP */
int string_to_sin(const char *addr, struct sockaddr_in *sin);

char *sin_to_string(const struct sockaddr_in *sin);

/* Extract the port from a string of the form "<xx.xx.xx.xx:pppp>" */
int string_to_port( const char* addr );

/* Extract the ip_addr from a string of the form "<xx.xx.xx.xx:pppp>"
   and convert it to the unsigned int version from the ASCII version */
unsigned int string_to_ip( const char* addr );

/* Extract the ip_addr from a string of the form "<xx.xx.xx.xx:pppp>"
   and return a pointer to the ASCII version */
char* string_to_ipstr( const char* addr );

/* Convert a sinful string into a hostname. */
char* string_to_hostname( const char* addr );

/* Convert a hostname[:port] to sinful string */
char * hostname_to_string (const char * hostname, const int default_port );

char *sock_to_string(SOCKET sockd);

/* Return the real hostname of a machine given a sin; return NULL if it cannot
 * be found or error.  Also return aliases. */
char *sin_to_hostname(const struct sockaddr_in *sin, char ***aliases);

void
display_from( struct sockaddr_in *from );

/* Returns 1 if h1 and h2 are both hostnames which refer to the same
   host, 0 if they don't, and -1 on error. */
int same_host(const char *h1, const char *h2);

/* Returns TRUE if hostname belongs to the given domain, FALSE if not */ 
int host_in_domain(const char *host, const char *domain);

char* calc_subnet_name( char* host );

int is_ipaddr(const char *inbuf, struct in_addr *sin_addr);

int is_valid_network( const char *network, struct in_addr *ip, struct in_addr *mask);

int is_valid_sinful( const char *sinful );

/* returns the port integer from a given address, or -1 if there is
   none. */
int getPortFromAddr( const char* addr );

/* returns only the host information (not including '<', '>', ":2132",
   etc) from a given address.  the string returned is a newly
   allocated string which must be de-allocated with free(). */ 
char* getHostFromAddr( const char* addr );

/* returns the address from a given ClaimId.  the string returned is a
   newly allocated string which must be de-allocated with free().
*/ 
char* getAddrFromClaimId( const char* id );

/* Binds the given fd to any port on the correct local interface for
   this machine.   is_outgoing tells it how to param.  Returns 1 if
   successful, 0 on error. */
int _condor_local_bind( int is_outgoing, int fd );

int is_priv_net(uint32_t ip);
int in_same_net(uint32_t ipA, uint32_t ipB);
char *ipport_to_string(const unsigned int ip, const unsigned short port);
char *prt_fds(int maxfd, fd_set *fds);

/* Get the address that a socket is bound to */
// @ret: NULL if failed; non-NULL if succeed
struct sockaddr_in *getSockAddr(int sockfd);

#if defined(__cplusplus)
}
#endif

#endif /* INTERNET_H */
