/* 
** Copyright 1993 by Miron Livny, and Mike Litzkow
** 
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

#if !defined(WIN32)
#include <netdb.h>
#endif


// Return our hostname in a static data buffer.
// If we've already looked up the hostname, just return the answer.
char *
my_hostname()
{
	static char hostname[1024] = "";
	char* tmp;
	if( hostname[0] ) {
		return hostname;
	}
	if( gethostname(hostname, sizeof(hostname)) == 0 ) {
		tmp = strchr( hostname, '.' );
		if( tmp ) {
			*tmp = '\0';
		}
		return hostname;
	} else {
		hostname[0] = '\0';
		return NULL;
	}
}


// Return our full hostname (with domain) in a static data buffer.
// If we've already looked up the hostname, just return the answer.
char*
my_full_hostname()
{
	static char hostname[1024] = "\0";
	char this_host[1024];
	struct hostent *host_ptr;

	if( hostname[0] ) {
		return hostname;
	}
		// determine our own host name 
	if( gethostname(this_host, sizeof(this_host)) < 0 ) {
		hostname[0] = '\0';
		return NULL;
	}

	/* Look up out official host information */
	if( (host_ptr = gethostbyname(this_host)) == NULL ) {
		hostname[0] = '\0';
		return NULL;
	}

	/* copy out the official hostname */
	strcpy(hostname, host_ptr->h_name);
	return hostname;
}
