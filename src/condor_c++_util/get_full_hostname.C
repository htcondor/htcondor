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
** Author:  Derek Wright <wright@cs.wisc.edu>, 1/7/98
**
*/ 

#include "condor_common.h"

// Returns the full hostname of the given host in a static buffer.
char*
get_full_hostname( const char* host ) 
{
	static char full_host[MAXHOSTNAMELEN];
	struct hostent *hostent_ptr;
	int len;

		// Check if we've got a '.' at the end of the name.  If so,
		// we've already got a fully qualified hostname.
	len = strlen( host );
	if( host[len-1] == '.' ) {
		return (char*)host;
	}

		// Since we don't have it, we need to ask the resolver.
	if( (hostent_ptr = gethostbyname( host )) == NULL ) {
			// If the resolver can't find it, just return NULL
		return NULL;
	}

		// Make sure we've got a trailing '.' to signify a fully
		// qualified name.
	strcpy( full_host, hostent_ptr->h_name );
	len = strlen( full_host );
	if( full_host[len-1] != '.' ) {
		full_host[len] = '.';
		full_host[len+1] = '\0';
	}		
	return full_host;
}
