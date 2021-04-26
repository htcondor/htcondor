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
	XXX:  known problems:  This function allows anything after the :, it
	doesn't have to be a port number.  ~tristan 8/20/07
*/
/* This function has a unit test. */
int
is_valid_sinful( const char *sinful )
{
	dprintf(D_HOSTNAME, "Checking if %s is a sinful address\n", sinful);
	char addrbuf[INET6_ADDRSTRLEN];
	const char* acc = sinful;
	const char* tmp;
	const char* addr_begin;
	const char* addr_end;
	if (!acc)
		return FALSE;

	if (*acc != '<') {
		dprintf(D_HOSTNAME, "%s is not a sinful address: does not begin with \"<\"\n", sinful);
		return FALSE;
	}
	acc++;
	if (*acc == '[') {
		dprintf(D_HOSTNAME, "%s is an ipv6 address\n", sinful);
		tmp = strchr(acc, ']');
		if (!tmp) {
			dprintf(D_HOSTNAME, "%s is not a sinful address: could not find closing \"]\"\n", sinful);
			return FALSE;
		}
		addr_begin = acc + 1;
		addr_end = tmp;
		// too long
		if (addr_end - addr_begin > INET6_ADDRSTRLEN) {
			dprintf(D_HOSTNAME, "%s is not a sinful address: addr too long %d\n", sinful, (int)(addr_end - addr_begin));
			return FALSE;
		}

		strncpy(addrbuf, addr_begin, addr_end - addr_begin);
		addrbuf[addr_end - addr_begin] = '\0';
		dprintf(D_HOSTNAME, "tring to convert %s using inet_pton, %s\n", sinful, addrbuf);
		in6_addr tmp_addr;
		if (inet_pton(AF_INET6, addrbuf, &tmp_addr) <= 0) {
			dprintf(D_HOSTNAME, "%s is not a sinful address: inet_pton(AF_INET6, %s) failed\n", sinful, addrbuf);
			return FALSE;
		}
		acc = tmp + 1;
	} else {
		MyString ipaddr = acc;
		int colon_pos = ipaddr.FindChar(':');
		if(colon_pos == -1) { return false; }
		ipaddr.truncate(colon_pos);
		if( ! is_ipv4_addr_implementation(ipaddr.c_str(),NULL,NULL,false) ) {
			return FALSE;
		}
		acc = acc + colon_pos;
	}
	if (*acc != ':') {
		dprintf(D_HOSTNAME, "%s is not a sinful address: no colon found\n", sinful);
		return FALSE;
	}

	tmp = strchr(acc, '>');
	if (!tmp) {
		dprintf(D_HOSTNAME, "%s is not a sinful address: no closing \">\" found\n", sinful);
		return FALSE;
	}
	dprintf(D_HOSTNAME, "%s is a sinful address!\n", sinful);
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

	if( (copy[0] == '[' || copy[1] == '[') && (tmp = strchr(copy, ']')) ) {
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
	tmp = copy;
	if( tmp[0] == '<' ) {
		tmp++;
	}
	if( tmp[0] == '[' ) {
		tmp++;
	}
	host = strdup( tmp );

	free( copy );
	return host;
}


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
