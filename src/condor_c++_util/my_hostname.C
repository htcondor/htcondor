/* 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the copyright holders not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the 
** copyright holders make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE COPYRIGHT HOLDERS DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Derek Wright
**
*/ 

#include "condor_common.h"
#include "condor_debug.h"
static char *_FileName_ = __FILE__;

static struct hostent *host_ptr = NULL;
static char hostname[MAXHOSTNAMELEN];
static char full_hostname[MAXHOSTNAMELEN];
static unsigned int ip_addr;
static int hostnames_initialized = 0;
static void init_hostnames();

// Return our hostname in a static data buffer.
char *
my_hostname()
{
	if( ! hostnames_initialized ) {
		init_hostnames();
	}
	return hostname;
}


// Return our full hostname (with domain) in a static data buffer.
char*
my_full_hostname()
{
	if( ! hostnames_initialized ) {
		init_hostnames();
	}
	return full_hostname;
}


// Return the host-ordered, unsigned int version of our hostname.
unsigned int
my_ip_addr()
{
	if( ! hostnames_initialized ) {
		init_hostnames();
	}
	return ip_addr;
}


void
init_hostnames()
{
	char* tmp;
	int len;

		// Get our local hostname, and strip off the domain if
		// gethostname returns it.
	if( gethostname(hostname, sizeof(hostname)) == 0 ) {
		tmp = strchr( hostname, '.' );
		if( tmp ) {
			*tmp = '\0';
		}
	} else {
		EXCEPT( "gethostname failed, errno = %d", errno );
	}

		// Look up our official host information
	if( (host_ptr = gethostbyname(hostname)) == NULL ) {
		EXCEPT( "gethostbyname(%s) failed, errno = %d", hostname, errno );
	}

		// Grab our ip_addr and fully qualified hostname
	memcpy( &ip_addr, host_ptr->h_addr, (size_t)host_ptr->h_length );
	ip_addr = ntohl( ip_addr );

	strcpy( full_hostname, host_ptr->h_name );
		// Make sure we've got a trailing '.' to signify a fully
		// qualified name.
	len = strlen( full_hostname );
	if( full_hostname[len-1] != '.' ) {
		full_hostname[len] = '.';
		full_hostname[len+1] = '\0';
	}		
	
	hostnames_initialized = TRUE;
}

