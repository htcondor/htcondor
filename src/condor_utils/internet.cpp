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

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_uid.h"
#include "internet.h"
#include "my_hostname.h"
#include "condor_config.h"
#include "get_port_range.h"
#include "condor_socket_types.h"
#include "condor_netdb.h"
#if defined ( WIN32 )
#include <iphlpapi.h>
#endif

#include "condor_sockaddr.h"
#include "ipv6_hostname.h"
#include "condor_sockfunc.h"

#if defined(__cplusplus)
extern "C" {
#endif

int bindWithin(const int fd, const int low_port, const int high_port);


/* Split "<host:port?params>" into parts: host, port, and params. If
   the port or params are not in the string, the result is set to
   NULL.  Any of the result char** values may be NULL, in which case
   they are parsed but not set.  The caller is responsible for freeing
   all result strings.
*/
int
split_sin( const char *addr, char **host, char **port, char **params )
{
	int len;

	if( host ) *host = NULL;
	if( port ) *port = NULL;
	if( params ) *params = NULL;

	if( !addr || *addr != '<' ) {
		return 0;
	}
	addr++;

	if (*addr == '[') {
		addr++;
		// ipv6 address
		const char* pos = strchr(addr, ']');
		if (!pos) {
			// mis-match bracket
			return 0;
		}
		if ( host ) {
			*host = (char*)malloc(pos - addr + 1);
			ASSERT( *host );
			memcpy(*host, addr, pos - addr);
			(*host)[pos - addr] = '\0';
		}
		addr = pos + 1;
	} else {
		// everything else
		len = strcspn(addr,":?>");
		if( host ) {
			*host = (char *)malloc(len+1);
			ASSERT( *host );
			memcpy(*host,addr,len);
			(*host)[len] = '\0';
		}
		addr += len;
	}

	if( *addr == ':' ) {
		addr++;
		len = strspn(addr,"0123456789");
		if( port ) {
			*port = (char *)malloc(len+1);
			memcpy(*port,addr,len);
			(*port)[len] = '\0';
		}
		addr += len;
	}

	if( *addr == '?' ) {
		addr++;
		len = strcspn(addr,">");
		if( params ) {
			*params = (char *)malloc(len+1);
			memcpy(*params,addr,len);
			(*params)[len] = '\0';
		}
		addr += len;
	}

	if( addr[0] != '>' || addr[1] != '\0' ) {
		if( host ) {
			free( *host );
			*host = NULL;
		}
		if( port ) {
			free( *port );
			*port = NULL;
		}
		if( params ) {
			free( *params );
			*params = NULL;
		}
		return 0;
	}
	return 1;
}

//int
//address_to_sin(char const *host, char const *port, struct sockaddr_in *sa_in)
//{
//	struct  hostent *hostptr;
//	if( !host || !port ) {
//		return 0;
//	}
//	if ( !is_ipaddr(host,NULL) &&
//		((hostptr=condor_gethostbyname(host)) != NULL &&
//		 hostptr->h_addrtype==AF_INET) )
//	{
//			sa_in->sin_addr = *(struct in_addr *)(hostptr->h_addr_list[0]);
//	}
//	else if( !condor_inet_aton(host, &sa_in->sin_addr) ) {
//		return 0;
//	}
//
//	sa_in->sin_port = htons((short)atoi(port));
//	sa_in->sin_family = AF_INET;
//
//	return 1;
//}

/* Convert a string of the form "<xx.xx.xx.xx:pppp?params>" to a
  sockaddr_in TCP (Also allow strings of the form "<hostname:pppp?params>")
  The ?params part is optional.  Use string_to_sin_params() to get the value
  of params.

  This function has a unit test.
*/

//int
//string_to_sin( const char *addr, struct sockaddr_in *sa_in )
//{
//	char *host=NULL;
//	char *port=NULL;
//	int result;
//
//	result = split_sin(addr,&host,&port,NULL);
//
//	if( result ) {
//		result = address_to_sin(host,port,sa_in);
//	}
//
//	free( host );
//	free( port );
//
//	return result;
//}

//char *
//sin_to_ipstring(const struct sockaddr_in *sa_in,char *buf,size_t buflen)
//{
//	char const *ipstr = inet_ntoa(sa_in->sin_addr);
//	if( !ipstr || strlen(ipstr)>=buflen ) {
//		if( buflen > 0 ) {
//			buf[0] = '\0';
//		}
//		return NULL;
//	}
//	strcpy(buf,ipstr);
//	return buf;
//}

/* This function has a unit test. */
//char *
//sin_to_string(const struct sockaddr_in *sa_in)
//{
//	static  char    buf[SINFUL_STRING_BUF_SIZE];
//
//	buf[0] = '\0';
//	if (!sa_in) return buf;
//	buf[0] = '<';
//	buf[1] = '\0';
//    if (sa_in->sin_addr.s_addr == INADDR_ANY) {
//        strcat(buf, my_ip_string());
//    } else {
//        strcat(buf, inet_ntoa(sa_in->sin_addr));
//    }
//    sprintf(&buf[strlen(buf)], ":%d>", ntohs(sa_in->sin_port));
//    return buf;
//}


const char *
sock_to_string(SOCKET sockd)
{
	static char sinful[SINFUL_STRING_BUF_SIZE];
	sinful[0] = '\0';
	condor_sockaddr addr;
	if (condor_getsockname(sockd, addr) <0)
		return sinful;

	addr.to_sinful(sinful, sizeof(sinful));
	return sinful;
}

char const *
sock_peer_to_string( SOCKET fd, char *buf, size_t buflen, char const *unknown )
{
	condor_sockaddr addr;
	if (condor_getpeername(fd, addr) <0)
		return unknown;

	addr.to_sinful(buf, buflen);
	return unknown;

	/* This value is never used.
	char const *sinful = sock_to_string( fd );
	*/

//	struct sockaddr_in who;
//	SOCKET_LENGTH_TYPE addr_len;
//
//	addr_len = sizeof(who);
//	memset(buf,0,buflen);
//	if( getpeername(fd, (struct sockaddr *)&who, (socklen_t*)&addr_len) == 0) {
//		if( who.sin_family == AF_INET ) {
//			char const *sinful = sin_to_string( &who );
//			if( sinful ) {
//				strncpy(buf, sinful, buflen );
//				if(buflen) {
//					buf[buflen-1] = '\0'; /* ensure null termination */
//				}
//				return buf;
//			}
//		}
//	}
//	return unknown;
}

//char *
//sin_to_hostname( const struct sockaddr_in *from, char ***aliases)
//{
//    struct hostent  *hp;
//    struct sockaddr_in caddr;
//
//	if( !from ) {
//		// make certain from is not NULL before derefencing it
//		return NULL;
//	}
//    memcpy(&caddr, from, sizeof(caddr));
//
//    // If 'from' is INADDR_ANY, then we use the canonical IP address
//    // instead of "0.0.0.0" as the input to name query
//    if (caddr.sin_addr.s_addr == INADDR_ANY) {
//        caddr.sin_addr.s_addr = my_ip_addr();
//    }
//
//    if( (hp=condor_gethostbyaddr((char *)&caddr.sin_addr,
//                sizeof(struct in_addr), AF_INET)) == NULL ) {
//		// could not find a name for this address
//        return NULL;
//    } else {
//		// CAREFULL: we are returning a staic buffer from gethostbyaddr.
//		// The caller had better use the result immediately or copy it.
//		// Also note this is not thread safe.  (as are lots of things in internet.c).
//		if( aliases ) {
//			*aliases = hp->h_aliases;
//		}
//		return hp->h_name;
//    }
//}


//void
//display_from( struct sockaddr_in *from )
//{
//    struct hostent  *hp;
//    struct sockaddr_in caddr;
//
//	if( !from ) {
//		dprintf( D_ALWAYS, "from NULL source\n" );
//		return;
//	}
//    memcpy(&caddr, from, sizeof(caddr));
//
//    // If 'from' is INADDR_ANY, then we use the canonical IP address
//    // instead of "0.0.0.0"
//    if (caddr.sin_addr.s_addr == INADDR_ANY) {
//        caddr.sin_addr.s_addr = my_ip_addr();
//    }
//
//    if( (hp=condor_gethostbyaddr((char *)&caddr.sin_addr,
//                sizeof(struct in_addr), AF_INET)) == NULL ) {
//        dprintf( D_ALWAYS, "from (%s), port %d\n",
//            inet_ntoa(caddr.sin_addr), ntohs(caddr.sin_port) );
//    } else {
//        dprintf( D_ALWAYS, "from %s, port %d\n",
//                                        hp->h_name, ntohs(caddr.sin_port) );
//    }
//}

#if 0
char *
calc_subnet_name(void)
{
//	char			subnetname[MAXHOSTNAMELEN];
//	char			*subnet_ptr;
//	char			*host_addr_string;
//	int				subnet_length;
//	struct			in_addr	in;
//	unsigned int	host_ordered_addr;
//	unsigned int		net_ordered_addr;
//
//	if ( !(host_ordered_addr = my_ip_addr()) ) {
//		return strdup("");
//	}
//
//	net_ordered_addr = htonl(host_ordered_addr);
//	memcpy((char *) &in,(char *)&net_ordered_addr, sizeof(host_ordered_addr));
//	host_addr_string = inet_ntoa( in );
//	if( host_addr_string ) {
//		subnet_ptr = (char *) strrchr(host_addr_string, '.');
//		if(subnet_ptr == NULL) {
//			return strdup("");
//		}
//		subnet_length = subnet_ptr - host_addr_string;
//		strncpy(subnetname, host_addr_string, subnet_length);
//		subnetname[subnet_length] = '\0';
//		return (strdup(subnetname));
//	}
//	return strdup("");
}
#endif

int
same_host(const char *h1, const char *h2)
{
	struct hostent *he1, *he2;
	char cn1[MAXHOSTNAMELEN];

	if( h1 == NULL || h2 == NULL) {
		dprintf( D_ALWAYS, "Warning: attempting to compare null hostnames in same_host.\n");
		return FALSE;
	}

	// if NO_DNS were used, it is same host if and only if h1 equals to h2.
	if (strcmp(h1, h2) == MATCH) {
		return TRUE;
	}

	// call directly getaddrinfo to check that both has same name
	if ((he1 = gethostbyname(h1)) == NULL) {
		return -1;
	}

	// stash h_name before our next call to gethostbyname
	strncpy(cn1, he1->h_name, MAXHOSTNAMELEN);
	cn1[MAXHOSTNAMELEN-1] = '\0';

	if ((he2 = gethostbyname(h2)) == NULL) {
		return -1;
	}

	return (strcmp(cn1, he2->h_name) == MATCH);
}


/*
  Return TRUE if the given domain contains the given hostname, FALSE
  if not.  Origionally taken from condor_starter.V5/starter_common.C.
  Moved here on 1/18/02 by Derek Wright.
*/
int
host_in_domain( const char *host, const char *domain )
{
    int skip;

    skip = strlen(host) - strlen(domain);
    if( skip < 0 ) {
        return FALSE;
    }

    if( strcasecmp(host+skip,domain) == MATCH ) {
        if(skip==0 || host[skip-1] == '.' || domain[0] == '.' ) {
            return TRUE;
        }
    }
    return FALSE;
}

// struct in_addr should not be used unless there is "IPv4" specified at the
// function name
int
is_ipv4_addr_implementation(const char *inbuf, struct in_addr *sin_addr,
		struct in_addr *mask_addr,int allow_wildcard)
{
	int len;
	char buf[17];
	int part = 0;
	int i,j,x;
	char save_char;
	unsigned char *cur_byte = NULL;
	unsigned char *cur_mask_byte = NULL;
	if( sin_addr ) {
		cur_byte = (unsigned char *) sin_addr;
	}
	if( mask_addr ) {
		cur_mask_byte = (unsigned char *) mask_addr;
	}

	len = strlen(inbuf);
	if ( len < 1 || len > 15 )
		return FALSE;
		// shortest possible IP addr is "*" - 1 char
		// longest possible IP addr is "123.123.123.123" - 15 chars

	// copy to our local buf
	strncpy( buf, inbuf, 16 );

	// strip off any trailing wild card or '.'
	if ( buf[len-1] == '*' || buf[len-1] == '.' ) {
		if ( len>1 && buf[len-2] == '.' )
			buf[len-2] = '\0';
		else
			buf[len-1] = '\0';
	}

	// Make certain we have a valid IP address, and count the parts,
	// and fill in sin_addr
	i = 0;
	for(;buf[i];) {

		j = i;
		while (buf[i] >= '0' && buf[i] <= '9') i++;
		// make certain a number was here
		if ( i == j )
			return FALSE;
		// now that we know there was a number, check it is between 0 & 255
		save_char = buf[i];
		buf[i] = '\0';
		x = atoi( &(buf[j]) );
		if( x < 0 || x > 255 ) {
			return FALSE;
		}
		if( cur_byte ) {
			*cur_byte = x;	/* save into sin_addr */
			cur_byte++;
		}
		if( cur_mask_byte ) {
			*(cur_mask_byte++) = 255;
		}
		buf[i] = save_char;

		part++;

		if ( buf[i] == '\0' )
			break;

		if ( buf[i] == '.' )
			i++;
		else
			return FALSE;

		if ( part >= 4 )
			return FALSE;
	}

	if( !allow_wildcard && part != 4 ) {
		return FALSE;
	}

	if( cur_byte ) {
		for (i=0; i < 4 - part; i++) {
			*cur_byte = (unsigned char) 255;
			cur_byte++;
		}
	}
	if( cur_mask_byte ) {
		for (i=0; i < 4 - part; i++) {
			*(cur_mask_byte++) = 0;
		}
	}
	return TRUE;
}


/*
  is_ipaddr() returns TRUE if buf is an ascii IP address (like
  "144.11.11.11") and false if not (like "cs.wisc.edu").  Allow
  wildcard "*".  If we return TRUE, and we were passed in a non-NULL
  sin_addr, it's filled in with the integer version of the ip address.
NOTE: it looks like sin_addr may be modified even if the return
  value is FALSE.  -zmiller
*/
/* XXX:  Known problems:  This function succeeds even if something with less
 * than four octets is passed in without a wildcard.  Also, strings with
 * multiple wildcards (such as 192.168.*.*) are not allowed, perhaps those
 * should be considered the same as 192.168.*  ~tristan 8/16/07
 */
/* This function has a unit test. */
//int
//is_ipaddr(const char *inbuf, struct in_addr *sin_addr)
//{
//		// In keeping with the historical definition of this function,
//		// we allow wildcards, even though the caller did not explicitly
//		// ask for them.  This usage needs to be reviewed.
//	return is_ipaddr_implementation(inbuf,sin_addr,NULL,1);
//}
//
//int
//is_ipaddr_no_wildcard(const char *inbuf, struct in_addr *sin_addr)
//{
//	return is_ipaddr_implementation(inbuf,sin_addr,NULL,0);
//}
//
//int
//is_ipaddr_wildcard(const char *inbuf, struct in_addr *sin_addr, struct in_addr *mask_addr)
//{
//	return is_ipaddr_implementation(inbuf,sin_addr,mask_addr,1);
//}


// checks to see if 'network' is a valid ip/netmask.  if given pointers to
// ip_addr structs, they will be filled in.
/* XXX:  Known Problems: The netmask doesn't have to be a valid
   netmask, as long as it looks something like an IP address.  ~tristan 8/16/07
 */
/* This function has a unit test. */
//int
//is_valid_network( const char *network, struct in_addr *ip, struct in_addr *mask)
//{
//	// copy the string, only 32 is necessary since the lonest
//	// legitimate one is 123.567.901.345/789.123.567.901
//	//                            1         2         3
//	// 31 characters.
//	//
//	// we make a copy because we then find the slash and
//	// overwrite it with a null to create two separate strings.
//	// those are then validated and parsed into the structures
//	// that were optionally passed in.
//	char nmcopy[32];
//	char *tmp;
//	int  numbits;
//	strncpy( nmcopy, network, 31 );
//	nmcopy[31] = '\0';
//
//	// find a slash and make sure both sides are valid
//	tmp = strchr(nmcopy, '/');
//	if( !tmp ) {
//		if( is_ipaddr_wildcard(nmcopy,ip,mask) ) {
//				// this is just a plain IP or IP.*
//			return TRUE;
//		}
//	}
//	else {
//		// separate by overwriting the slash with a null, and moving tmp
//		// to point to the begining of the second string.
//		*tmp++ = 0;
//
//		// now validate
//		if (is_ipaddr_no_wildcard(nmcopy, ip)) {
//			// first part is a valid ip, now validate the netmask.  two
//			// different formats are valid, we check for both.
//			if (is_ipaddr_no_wildcard(tmp, mask)) {
//				// format is a.b.c.d/m.a.s.k
//				// is_ipaddr fills in the value for both ip and mask,
//				// so we are done!
//				return TRUE;
//			} else {
//				// try format a.b.c.d/num
//				char *end = NULL;
//				numbits = strtol(tmp,&end,10);
//				if (end && *end == '\0') {
//					if (mask) {
//						// fill in the structure
//					    mask->s_addr = 0;
//					    mask->s_addr = htonl(~(~(mask->s_addr) >> numbits));
//					}
//					return TRUE;
//				} else {
//					dprintf (D_SECURITY, "ISVALIDNETWORK: malformed netmask: %s\n", network);
//				}
//			}
//		}
//	}
//
//	return FALSE;
//}

/*
	XXX:  known problems:  This function allows anything after the :, it
	doesn't have to be a port number.  ~tristan 8/20/07
*/
/* This function has a unit test. */
int
is_valid_sinful( const char *sinful )
{
	dprintf(D_HOSTNAME, "validate %s\n", sinful);
	char addrbuf[INET6_ADDRSTRLEN];
	const char* acc = sinful;
	const char* tmp;
	const char* addr_begin;
	const char* addr_end;
	if (!acc)
		return FALSE;

	if (*acc != '<') {
		dprintf(D_HOSTNAME, "is not begin with <\n");
		return FALSE;
	}
	acc++;
	if (*acc == '[') {
		dprintf(D_HOSTNAME, "ipv6 address\n");
		tmp = strchr(acc, ']');
		if (!tmp) {
			dprintf(D_HOSTNAME, "could not find ]\n");
			return FALSE;
		}
		addr_begin = acc + 1;
		addr_end = tmp;
		// too long
		if (addr_end - addr_begin > INET6_ADDRSTRLEN) {
			dprintf(D_HOSTNAME, "addr too long %d\n", (int)(addr_end - addr_begin));
			return FALSE;
		}

		strncpy(addrbuf, addr_begin, addr_end - addr_begin);
		addrbuf[addr_end - addr_begin] = '\0';
		dprintf(D_HOSTNAME, "try to convert using inet_pton, %s\n", addrbuf);
		in6_addr tmp_addr;
		if (inet_pton(AF_INET6, addrbuf, &tmp_addr) <= 0) {
			dprintf(D_HOSTNAME, "inet_pton failed\n");
			return FALSE;
		}
		acc = tmp + 1;
	} else {
		MyString ipaddr = acc;
		int colon_pos = ipaddr.FindChar(':');
		if(colon_pos == -1) { return false; }
		ipaddr.setChar(colon_pos, 0);
		if( ! is_ipv4_addr_implementation(ipaddr.Value(),NULL,NULL,false) ) {
			return FALSE;
		}
		acc = acc + colon_pos;
	}
	if (*acc != ':') {
		dprintf(D_HOSTNAME, "no colon found\n");
		return FALSE;
	}

	tmp = strchr(acc, '>');
	if (!tmp) {
		dprintf(D_HOSTNAME, "no > found\n");
		return FALSE;
	}
	dprintf(D_HOSTNAME, "success\n");
	return TRUE;
}

/*
	XXX:  known problems:  is_valid_sinful() doesn't do error checking on the
	port number, this function doesn't, and neither does atoi().  Possible fix
	would be to migrate all uses of this function to getPortFromAddr(), which
	does at least basic error checking.
		  ~tristan 8/20/07
*/
/* This function has a unit test. */
int
string_to_port( const char* addr )
{
	const char* acc = addr;
	const char* tmp;
	int port = 0;

	if( ! (addr && is_valid_sinful(addr)) ) {
		return 0;
	}

	if (*acc != '<')
		return 0;

	acc++;
	if (*acc == '[') {
		tmp = strchr(acc, ']');
		if (!tmp)
			return 0;
		acc = tmp + 1;
	}

	tmp = strchr(acc, ':');
	if (!tmp)
		return 0;

	tmp++;
	port = atoi(tmp);
	return port;
}

/* XXX:  known problems:  string_to_ip() uses is_ipaddr(), and so has the same
 * flaws.  Mainly, input like 66.199 is assumed to be followed by ".*" even
 * though that's a badly formed IP address.  ~tristan 8/22/07
 */
/* This function has a unit test. */
//unsigned int
//string_to_ip( const char* addr )
//{
//	char *sinful, *tmp;
//	unsigned int ip = 0;
//	struct in_addr sin_addr;
//
//	if( ! (addr && is_valid_sinful(addr)) ) {
//		return 0;
//	}
//
//	sinful = strdup( addr );
//	if( (tmp = strchr(sinful, ':')) ) {
//		*tmp = '\0';
//		if( is_ipaddr(&sinful[1], &sin_addr) ) {
//			ip = sin_addr.s_addr;
//		}
//	} else {
//		EXCEPT( "is_valid_sinful(\"%s\") is true, but can't find ':'", addr );
//	}
//	free( sinful );
//	return ip;
//}

#if 0
char*
string_to_ipstr( const char* addr )
{
//	char *tmp;
//	static char result[MAXHOSTNAMELEN];
//	char sinful[MAXHOSTNAMELEN];
//
//	if( ! (addr && is_valid_sinful(addr)) ) {
//		return NULL;
//	}
//
//	strncpy( sinful, addr, MAXHOSTNAMELEN-1 );
//	tmp = strchr( sinful, ':' );
//	if( tmp ) {
//		*tmp = '\0';
//	} else {
//		return NULL;
//	}
//	if( is_ipaddr(&sinful[1], NULL) ) {
//		strncpy( result, &sinful[1], MAXHOSTNAMELEN-1 );
//		return result;
//	}
//	return NULL;
}

#endif

// This union allow us to avoid casts, which cause
// gcc to warn about type punning pointers, which
// may or may not cause it to generate invalid code
typedef union {
	struct sockaddr sa;
	struct sockaddr_in v4;
	struct sockaddr_in6 v6;
	struct sockaddr_storage all;
} ipv4or6_storage;

/* Bind the given fd to the correct local interface. */

// _condor_local_bind(), get_port_range() and bindWithin()
// should be ported to IPv6. However, it will be delayed until
// internet.c becomes internet.cpp.

// [IPV6] Ported
int
_condor_local_bind( int is_outgoing, int fd )
{
	/* Note: this function is completely WinNT screwed.  However,
	 * only non-Cedar components call this function (ckpt-server,
	 * old shadow) --- and these components are not destined for NT
	 * anyhow.  So on NT, just pass back success so log file is not
	 * full of scary looking error messages.
	 *
	 * This function should go away when everything uses CEDAR.
	 */
#ifndef WIN32
	int lowPort, highPort;
	if ( get_port_range(is_outgoing, &lowPort, &highPort) == TRUE ) {
		if ( bindWithin(fd, lowPort, highPort) == TRUE )
            return TRUE;
        else
			return FALSE;
	} else {
		//struct sockaddr_storage ss;
		ipv4or6_storage ss;
		socklen_t len = sizeof(ss);
		int r = getsockname(fd, &(ss.sa), &len);
		if (r != 0) {
			dprintf(D_ALWAYS, "ERROR: getsockname fialed, errno: %d\n",
					errno);
			return FALSE;
		}

			// this implementation should be changed to the one
			// using condor_sockaddr class
		if (ss.all.ss_family == AF_INET) {
			struct sockaddr_in* sa_in = &(ss.v4);
			memset( (char *)sa_in, 0, sizeof(struct sockaddr_in) );
			sa_in->sin_family = AF_INET;
			sa_in->sin_port = 0;
			/*
			  we don't want to honor BIND_ALL_INTERFACES == true in
			  here.  this code is only used in the ckpt server (which
			  isn't daemoncore, so the hostallow localhost stuff
			  doesn't matter) and for bind()ing outbound connections
			  inside do_connect().  so, there's no harm in always
			  binding to all interfaces in here...
			  Derek <wright@cs.wisc.edu> 2005-09-20
			*/
			sa_in->sin_addr.s_addr = INADDR_ANY;
		} else if (ss.all.ss_family == AF_INET6) {
			struct sockaddr_in6* sin6 = &(ss.v6);
			sin6->sin6_addr = in6addr_any;
			sin6->sin6_port = 0;
		} else {
			dprintf(D_ALWAYS, "ERROR: getsockname returned with unknown "
					"socket type %d\n", ss.all.ss_family);
			return FALSE;
		}

		if( bind(fd, &(ss.sa), len) < 0 ) {
			dprintf( D_ALWAYS, "ERROR: bind failed, errno: %d\n",
					 errno );
			return FALSE;
		}
	}
#endif  /* of ifndef WIN32 */
	return TRUE;
}


int bindWithin(const int fd, const int low_port, const int high_port)
{
	int start_trial, this_trial;
	int pid, range;

	// Use hash function with pid to get the starting point
    pid = (int) getpid();
    range = high_port - low_port + 1;
    // this line must be changed to use the hash function of condor
    start_trial = low_port + (pid * 173/*some prime number*/ % range);

    this_trial = start_trial;
	do {
		struct sockaddr_in sa_in;
		priv_state old_priv;
		int bind_return_value;

		memset(&sa_in, 0, sizeof(sa_in));
		sa_in.sin_family = AF_INET;
		sa_in.sin_addr.s_addr = htonl(INADDR_ANY);
		sa_in.sin_port = htons((u_short)this_trial++);

// windows doesn't have privileged ports.
#ifndef WIN32
		if (this_trial <= 1024) {
			// use root priv for the call to bind to allow privileged ports
			old_priv = PRIV_UNKNOWN;
			old_priv = set_root_priv();
		}
#endif
		bind_return_value = bind(fd, (struct sockaddr *)&sa_in, sizeof(sa_in));
#ifndef WIN32
		if (this_trial <= 1024) {
			set_priv (old_priv);
		}
#endif
		if (bind_return_value == 0) { // success
			dprintf(D_NETWORK, "_condor_local_bind - bound to %d...\n", this_trial-1);
			return TRUE;
		} else {
            dprintf(D_NETWORK, "_condor_local_bind - failed to bind: %s\n", strerror(errno));
        }
		if ( this_trial > high_port )
			this_trial = low_port;
    } while(this_trial != start_trial);

	dprintf(D_ALWAYS, "_condor_local_bind::bindWithin - failed to bind any port within (%d ~ %d)\n",
	        low_port, high_port);

	return FALSE;
}

/* Check if the ip is in private ip address space */
/* ip: in host byte order */
/* This function has a unit test. */
int
is_priv_net(uint32_t ip)
{
    return ((ip & 0xFF000000) == 0x0A000000 ||      // 10/8
            (ip & 0xFFF00000) == 0xAC100000 ||      // 172.16/12
            (ip & 0xFFFF0000) == 0xC0A80000);       // 192.168/16
}

int
is_loopback_net(uint32_t ip)
{
    return ((ip & 0xFF000000) == 0x7F000000); // 127/8
}

int is_loopback_net_str(char const * /*ip*/)
{
//	struct in_addr sa;
//	if( is_ipaddr_no_wildcard(ip,&sa) ) {
//		return is_loopback_net(ntohl(sa.s_addr));
//	}
	return 0;
}

/* Check if two ip addresses, given in network byte, are in the same network */
int
in_same_net(uint32_t ipA, uint32_t ipB)
{
    unsigned char *byteA, *fA, *byteB;
    int i, index_to;

    fA = byteA = (unsigned char *)&ipA;
    byteB = (unsigned char *)&ipB;

    if (*fA < 128) { // A class
        index_to = 1;
    } else if(*fA < 192) { // B class
        index_to = 2;
    } else {    // C class
        index_to = 3;
    }

    for (i = 0; i < index_to; i++) {
        if (*byteA != *byteB) {
            return 0;
        }
        byteA++;
        byteB++;
    }

    return 1;
}

// ip: network-byte order
// port: network-byte order
char * ipport_to_string(const unsigned int ip, const unsigned short port)
{
    static  char    buf[24];
    struct in_addr inaddr;

    buf[0] = '<';
    buf[1] = '\0';
    if (ip == INADDR_ANY) {
        strcat(buf, my_ip_string());
    } else {
        inaddr.s_addr = ip;
        strcat(buf, inet_ntoa(inaddr));
    }
    sprintf(&buf[strlen(buf)], ":%d>", ntohs(port));
    return buf;
}

char *
prt_fds(int maxfd, fd_set *fds)
{
    static char buf[50];
    int i, size;

    sprintf(buf, "<");
    for(i=0; i<maxfd; i++) {
        if (fds && FD_ISSET(i, fds)) {
            if ((size = strlen(buf)) > 40) {
                strcat(buf, "...>");
                return buf;
            }
        sprintf(&buf[strlen(buf)], "%d ", i);
        }
    }
    strcat(buf, ">");
    return buf;
}

/* This function has a unit test. */
int
getPortFromAddr( const char* addr )
{
	const char *tmp;
	char *end;
	long port = -1;

	if( ! addr ) {
		return -1;
	}

	// skip the hostname part
	if (*addr == '<')
		addr++;
	if (*addr == '[') {
		addr = strchr(addr, ']');
		if (!addr)
			return -1;
		addr++;
	}

	tmp = strchr( addr, ':' );
	if( !tmp || ! *(tmp+1) ) {
			/* address didn't specify a port section */
		return -1;
	}

		/* clear out our errno so we know if it's set after the
		   strtol(), that it was set from there */
	errno = 0;
	port = strtol( tmp+1, &end, 10 );
	if( errno == ERANGE ) {
			/* port number was too big */
		return -1;
	}
	if( end == tmp+1 ) {
			/* port section of the address wasn't a number */
		return -1;
	}
	if( port < 0 ) {
			/* port numbers can't be negative */
		return -1;
	}
	if( port > INT_MAX ) {
			/* port number was too big to fit in an int */
		return -1;
	}
	return (int)port;
}


/* This function has a unit test. */
char*
getHostFromAddr( const char* addr )
{
	char *copy, *host = NULL, *tmp;

	if( ! (addr && addr[0]) ) {
		return 0;
	}

	copy = strdup( addr );

	if( (tmp = strchr(copy, ']')) ) {
		// if it is an IPv6 address, we want to end that host part there
		*tmp = '\0';

	} else if( (tmp = strchr(copy, ':')) ) {
		// 	// if there's a colon, we want to end the host part there
		*tmp = '\0';
	}

		// if it still ends with '>', we want to chop that off from
		// the string
	if( (tmp = strrchr(copy, '>')) ) {
		*tmp = '\0';
	}

		// if there's an '@' sign, we just want everything after that.
	if( (tmp = strchr(copy, '@')) ) {
		if( tmp[1] ) {
			host = strdup( &tmp[1] );
		}
		free( copy );
		return host;
	}

		// if there was no '@', but it begins with '<', we want to
		// skip that and just use what follows it...
		//
		// also, we want to skip '[' if it is an IPv6 address.
	if( copy[0] == '<' ) {
		if( copy[1] ) {
			if (copy[1] == '[') {
				if ( copy[2] )
					host = strdup( &copy[2] );
			} else
				host = strdup( &copy[1] );
		}
	} else if( copy[0] ) {
		host = strdup( copy );
	}
	free( copy );
	return host;
}


char*
getAddrFromClaimId( const char* id )
{
	char* tmp;
	char* addr;
	char* copy = strdup( id );
	tmp = strchr( copy, '#' );
	if( tmp ) {
		*tmp = '\0';
		if( is_valid_sinful(copy) ) {
			addr = strdup( copy );
			free( copy );
			return addr;
		}
	}
	free( copy );
	return NULL;
}


struct sockaddr_in *
getSockAddr(int sockfd)
{
    // do getsockname
    static struct sockaddr_in sa_in;
    SOCKET_LENGTH_TYPE namelen = sizeof(sa_in);
    if (getsockname(sockfd, (struct sockaddr *)&sa_in, (socklen_t*)&namelen) < 0) {
        dprintf(D_ALWAYS, "failed getsockname(%d): %s\n", sockfd, strerror(errno));
        return NULL;
    }
    // if getsockname returns INADDR_ANY, we rely upon my_ip_addr() since returning
    // 0.0.0.0 is not a good idea.
    if (sa_in.sin_addr.s_addr == ntohl(INADDR_ANY)) {
    	condor_sockaddr myaddr = get_local_ipaddr();
    	sa_in.sin_addr = myaddr.to_sin().sin_addr;
        //sa_in.sin_addr.s_addr = htonl(my_ip_addr());
    }
    return &sa_in;
}

//int
//condor_inet_aton(const char *ipstr, struct in_addr *result)
//{
//	if( !is_ipaddr(ipstr,result) ) {
//		return 0;
//	}
//	return 1;
//}

int generate_sinful(char* buf, int len, const char* ip, int port) {
	if (strchr(ip, ':')) {
		return snprintf(buf, len, "<[%s]:%d>", ip, port);
	} else {
		return snprintf(buf, len, "<%s:%d>", ip, port);
	}
}

#if defined(__cplusplus)
}

MyString generate_sinful(const char* ip, int port) {
	MyString buf;
	if (strchr(ip, ':')) {
		buf.formatstr("<[%s]:%d>", ip, port);
	} else {
		buf.formatstr("<%s:%d>", ip, port);
	}
	return buf;
}


bool sinful_to_ipstr(const char * addr, MyString & ipout)
{
	condor_sockaddr sa;
	if(!sa.from_sinful(addr)) { return false; }
	ipout = sa.to_ip_string();
	return true;
}

#endif
