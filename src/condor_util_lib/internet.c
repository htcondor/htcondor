/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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

#include "condor_common.h"
#include "condor_debug.h"
#include "internet.h"
#include "my_hostname.h"
#include "condor_config.h"
#include "get_port_range.h"
#include "condor_socket_types.h"

int bindWithin(const int fd, const int low_port, const int high_port);


/* Convert a string of the form "<xx.xx.xx.xx:pppp>" to a sockaddr_in  TCP */
/* (Also allow strings of the form "<hostname>:pppp>")  */
int
string_to_sin( const char *addr, struct sockaddr_in *sin )
{
	char    *cur_byte;
	char    *end_string;
	int 	temp=0;
	char*	addrCpy;
	char*	string;
	char*   colon = 0;
	struct  hostent *hostptr;

	if( ! addr ) {
		return 0;
	}
	addrCpy = strdup(addr);
	string = addrCpy;
	string++;					/* skip the leading '<' */

	/* allow strings of the form "<hostname:pppp>" */
	if( !(colon = strchr(string, ':')) ) {
			// Not a valid sinful string, return failure
		free(addrCpy);
		return 0;
	}
	*colon = '\0';
	if ( is_ipaddr(string,NULL) != TRUE) {

		if (((hostptr=gethostbyname(string)) != NULL && 
			 hostptr->h_addrtype==AF_INET)) {
			sin->sin_addr = *(struct in_addr *)(hostptr->h_addr_list[0]);
			string = colon + 1;
		} else {
			return 0;
		}
	}
	else
	{	
		/* parse the string in the traditional <xxx.yyy.zzz.aaa> form ... */	
		*colon = ':';
		cur_byte = (char *) &(sin->sin_addr);
		for(end_string = string; end_string != 0; ) {
			end_string = strchr(string, '.');
			if (end_string == 0) {
				end_string = strchr(string, ':');
				if (end_string) colon = end_string;
			}
			if (end_string) {
				*end_string = '\0';
				*cur_byte = atoi(string);
				cur_byte++;
				string = end_string + 1;
				*end_string = '.';
			}
		}
	}
	
	string[strlen(string) - 1] = '\0'; /* Chop off the trailing '>' */
	sin->sin_port = htons((short)atoi(string));
	sin->sin_family = AF_INET;
	string[temp-1] = '>';
	string[temp] = '\0';
	*colon = ':';
	free(addrCpy);
	return 1;
}


char *
sin_to_string(const struct sockaddr_in *sin)
{
	int             i;
	static  char    buf[24];
	char    tmp_buf[10];
	char    *cur_byte;
	unsigned char   this_byte;

	buf[0] = '\0';
	if (!sin) return buf;
	buf[0] = '<';
	buf[1] = '\0';
	cur_byte = (char *) &(sin->sin_addr);
	for (i = 0; i < sizeof(sin->sin_addr); i++) {
		this_byte = (unsigned char) *cur_byte;
		sprintf(tmp_buf, "%u.", this_byte);
		cur_byte++;
		strcat(buf, tmp_buf);
	}
	buf[strlen(buf) - 1] = ':';
	sprintf(tmp_buf, "%d>", ntohs(sin->sin_port));
	strcat(buf, tmp_buf);
	return buf;
}

char *
sock_to_string(SOCKET sockd)
{
	struct sockaddr_in	addr;
	SOCKET_LENGTH_TYPE	addr_len;
	static char *mynull = "\0";

	addr_len = sizeof(addr);

	if (getsockname(sockd, (struct sockaddr *)&addr, &addr_len) < 0) 
		return mynull;

	return ( sin_to_string( &addr ) );
}

char *
sin_to_hostname( const struct sockaddr_in *from, char ***aliases)
{
    struct hostent  *hp;
#ifndef WIN32
	struct hostent  *gethostbyaddr();
#endif

	if( !from ) {
		// make certain from is not NULL before derefencing it
		return NULL;
	}

    if( (hp=gethostbyaddr((char *)&from->sin_addr,
                sizeof(struct in_addr), AF_INET)) == NULL ) {
		// could not find a name for this address
        return NULL;
    } else {
		// CAREFULL: we are returning a staic buffer from gethostbyaddr.
		// The caller had better use the result immediately or copy it.
		// Also note this is not thread safe.  (as are lots of things in internet.c).
		if( aliases ) {
			*aliases = hp->h_aliases;
		}
		return hp->h_name;
    }
}


void
display_from( from )
struct sockaddr_in  *from;
{
    struct hostent  *hp;
#ifndef WIN32
	struct hostent  *gethostbyaddr();
#endif

	if( !from ) {
		dprintf( D_ALWAYS, "from NULL source\n" );
		return;
	}

    if( (hp=gethostbyaddr((char *)&from->sin_addr,
                sizeof(struct in_addr), AF_INET)) == NULL ) {
        dprintf( D_ALWAYS, "from (%s), port %d\n",
            inet_ntoa(from->sin_addr), ntohs(from->sin_port) );
    } else {
        dprintf( D_ALWAYS, "from %s, port %d\n",
                                        hp->h_name, ntohs(from->sin_port) );
    }
}

char *
calc_subnet_name(char* host)
{
	char			subnetname[MAXHOSTNAMELEN];
	char			*subnet_ptr;
	char			*host_addr_string;
	int				subnet_length;
	struct			in_addr	in;
	unsigned int	host_ordered_addr;
	unsigned int		net_ordered_addr;

	if ( !(host_ordered_addr = my_ip_addr()) ) {
		return strdup("");
	}

	net_ordered_addr = htonl(host_ordered_addr);
	memcpy((char *) &in,(char *)&net_ordered_addr, sizeof(host_ordered_addr));
	host_addr_string = inet_ntoa( in );
	if( host_addr_string ) {
		subnet_ptr = (char *) strrchr(host_addr_string, '.');
		if(subnet_ptr == NULL) {
			return strdup("");
		}
		subnet_length = subnet_ptr - host_addr_string;
		strncpy(subnetname, host_addr_string, subnet_length);
		subnetname[subnet_length] = '\0';
		return (strdup(subnetname));
	}
	return strdup("");
}

int
same_host(const char *h1, const char *h2)
{
	struct hostent *he1, *he2;
	char cn1[MAXHOSTNAMELEN];

	if (strcmp(h1, h2) == MATCH) {
		return TRUE;
	}
	
	if ((he1 = gethostbyname(h1)) == NULL) {
		return -1;
	}

	// stash h_name before our next call to gethostbyname
	strncpy(cn1, he1->h_name, MAXHOSTNAMELEN);

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
	const char	*ptr;

	for( ptr=host; *ptr; ptr++ ) {
		if( stricmp(ptr,domain) == MATCH ) {
			return TRUE;
		}
	}
	return FALSE;
}


/*
  is_ipaddr() returns TRUE if buf is an ascii IP address (like
  "144.11.11.11") and false if not (like "cs.wisc.edu").  Allow
  wildcard "*".  If we return TRUE, and we were passed in a non-NULL 
  sin_addr, it's filled in with the integer version of the ip address. 
NOTE: it looks like sin_addr may be modified even if the return
  value is FALSE.  -zmiller
*/
int
is_ipaddr(const char *inbuf, struct in_addr *sin_addr)
{
	int len;
	char buf[17];
	int part = 0;
	int i,j,x;
	char save_char;
	unsigned char *cur_byte = NULL;
	if( sin_addr ) {
		cur_byte = (unsigned char *) sin_addr;
	}

	len = strlen(inbuf);
	if ( len < 3 || len > 16 ) 
		return FALSE;	// shortest possible IP addr is "1.*" - 3 chars
	
	// copy to our local buf
	strncpy( buf, inbuf, 17 );

	// on IP addresses, wildcards only permitted at the end, 
	// i.e. 144.92.* , _not_ *.92.11
	if ( buf[0] == '*' ) 
		return FALSE;

	// strip off any trailing wild card or '.'
	if ( buf[len-1] == '*' || buf[len-1] == '.' ) {
		if ( buf[len-2] == '.' )
			buf[len-2] = '\0';
		else
			buf[len-1] = '\0';
	}

	// Make certain we have a valid IP address, and count the parts,
	// and fill in sin_addr
	i = 0;
	for(;;) {
		
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
	
	if( cur_byte ) {
		for (i=0; i < 4 - part; i++) {
			*cur_byte = (unsigned char) 255;
			cur_byte++;
		}
	}
	return TRUE;
}


// checks to see if 'network' is a valid ip/netmask.  if given pointers to
// ip_addr structs, they will be filled in.
int
is_valid_network( const char *network, struct in_addr *ip, struct in_addr *mask)
{
	// copy the string, only 32 is necessary since the lonest
	// legitimate one is 123.567.901.345/789.123.567.901
	//                            1         2         3
	// 31 characters.
	//
	// we make a copy because we then find the slash and
	// overwrite it with a null to create two separate strings.
	// those are then validated and parsed into the structures
	// that were optionally passed in.
	char nmcopy[32];
	char *tmp;
	int  numbits;
	strncpy( nmcopy, network, 31 );
	nmcopy[31] = '\0';

	// find a slash and make sure both sides are valid
	tmp = strchr(nmcopy, '/');
	if (tmp) {
		// separate by overwriting the slash with a null, and moving tmp
		// to point to the begining of the second string.
		*tmp++ = 0;

		// now validate
		if (is_ipaddr(nmcopy, ip)) {
			// first part is a valid ip, now validate the netmask.  two
			// different formats are valid, we check for both.
			if (is_ipaddr(tmp, mask)) {
				// format is a.b.c.d/m.a.s.k
				// is_ipaddr fills in the value for both ip and mask,
				// so we are done!
				return TRUE;
			} else {
				// try format a.b.c.d/num
				// this doesn't allow /0, but that isn't useful
				// anyways... easier to say '*'.
				numbits = atoi(tmp);
				if (numbits) {
					if (mask) {
						// fill in the structure
					    mask->s_addr = 0;
					    mask->s_addr = htonl(~(~(mask->s_addr) >> numbits));
					}
					return TRUE;
				} else {
					dprintf (D_SECURITY, "ISVALIDNETWORK: malformed netmask: %s\n", network);
				}
			}
		}
	}

	return FALSE;
}


int
is_valid_sinful( const char *sinful )
{
	char* tmp;
	char* copy;
	if( !sinful ) return FALSE;
	if( !(sinful[0] == '<') ) return FALSE;
	if( !(tmp = strrchr(sinful, '>')) ) return FALSE;
	copy = strdup( sinful );

	if( !(tmp = strchr(copy, ':')) ) {
		free( copy );
		return FALSE;
	}
	*tmp = '\0';
	if( ! copy[1] ) {
		free( copy );
		return FALSE;
	}
	if( ! is_ipaddr(&copy[1], NULL) ) {
		free( copy );
		return FALSE;
	}
	free( copy );
	return TRUE;
}


int
string_to_port( const char* addr )
{
	char *sinful, *tmp;
	int port = 0;

	if( ! (addr && is_valid_sinful(addr)) ) {
		return 0;
	}

	sinful = strdup( addr );
	if( (tmp = strrchr(sinful, '>')) ) {
		*tmp = '\0';
		tmp = strchr( sinful, ':' );
		if( tmp && tmp[1] ) {
			port = atoi( &tmp[1] );
		} 
	}
	free( sinful );
	return port;
}


unsigned int
string_to_ip( const char* addr )
{
	char *sinful, *tmp;
	unsigned int ip = 0;
	struct in_addr sin_addr;

	if( ! (addr && is_valid_sinful(addr)) ) {
		return 0;
	}

	sinful = strdup( addr );
	if( (tmp = strchr(sinful, ':')) ) {
		*tmp = '\0';
		if( is_ipaddr(&sinful[1], &sin_addr) ) {
			ip = sin_addr.s_addr;
		}
	} else {
		EXCEPT( "is_valid_sinful(\"%s\") is true, but can't find ':'" );
	}
	free( sinful );
	return ip;
}


char*
string_to_ipstr( const char* addr ) 
{
	char *tmp;
	static char result[MAXHOSTNAMELEN];
	char sinful[MAXHOSTNAMELEN];

	if( ! (addr && is_valid_sinful(addr)) ) {
		return NULL;
	}

	strncpy( sinful, addr, MAXHOSTNAMELEN );
	tmp = strchr( sinful, ':' );
	if( tmp ) {
		*tmp = '\0';
	} else {
		return NULL;
	}
	if( is_ipaddr(&sinful[1], NULL) ) {
		strncpy( result, &sinful[1], MAXHOSTNAMELEN );
		return result;
	}
	return NULL;
}


char*
string_to_hostname( const char* addr ) 
{
	char *tmp;
	static char result[MAXHOSTNAMELEN];
    struct sockaddr_in sin;

	if( ! (addr && is_valid_sinful(addr)) ) {
		return NULL;
	}

    string_to_sin( addr, &sin );
	if( (tmp = sin_to_hostname(&sin, NULL)) ) {
		strncpy( result, tmp, MAXHOSTNAMELEN );
	} else {
		return NULL;
	}
	return result;
}


/* Bind the given fd to the correct local interface. */
int
_condor_local_bind( int fd )
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
	if ( get_port_range(&lowPort, &highPort) == TRUE ) {
		if ( bindWithin(fd, lowPort, highPort) == TRUE )
            return TRUE;
        else
			return FALSE;
	} else {
		struct sockaddr_in sin;
		memset( (char *)&sin, 0, sizeof(sin) );
		sin.sin_family = AF_INET;
		sin.sin_port = 0;
		sin.sin_addr.s_addr = htonl(INADDR_ANY);
		if( bind(fd, (struct sockaddr*)&sin, sizeof(sin)) < 0 ) {
			dprintf( D_ALWAYS, "ERROR: bind(%s:%d) failed, errno: %d\n",
					 inet_ntoa(sin.sin_addr), sin.sin_port, errno );
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
		struct sockaddr_in sin;

		memset(&sin, 0, sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = htonl(INADDR_ANY);
		sin.sin_port = htons((u_short)this_trial++);

		if (bind(fd, (struct sockaddr *)&sin, sizeof(sin)) == 0) { // success
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


int
getPortFromAddr( const char* addr )
{
	char *copy, *tmp;
	int port = 0;

	if( ! addr ) {
		return 0;
	}
	
	copy = strdup( addr );
		// if it ends with '>', we want to chop that off from the
		// string so we don't confuse atoi() 
	if( (tmp = strrchr(copy, '>')) ) {
		*tmp = '\0';
	}
	tmp = strchr( copy, ':' );
	if( tmp && tmp[1] ) {
		port = atoi( &tmp[1] );
	}
	free( copy );
	return port;
}


char*
getHostFromAddr( const char* addr )
{
	char *copy, *host = NULL, *tmp; 

	if( ! (addr && addr[0]) ) {
		return 0;
	}
	
	copy = strdup( addr );

		// if there's a colon, we want to end the host part there
	if( (tmp = strchr(copy, ':')) ) {
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
	if( copy[0] == '<' ) {
		if( copy[1] ) { 
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

char *
hostname_to_string (const char * hostname, const int default_port) {
	static char hostname_buff[MAXHOSTNAMELEN];

	if (!(hostname) || !(*hostname))
		return NULL;

	if (strchr(hostname, ':')) {
 		sprintf (hostname_buff, "<%s>", hostname);
	} else if (default_port > 0) {
		sprintf (hostname_buff, "<%s:%d>", hostname, default_port);
	} else {
		return NULL;
	}

	struct sockaddr_in sin;
	if (!string_to_sin (hostname_buff, &sin)) {
		return NULL;
	}

	return sin_to_string (&sin);
}
