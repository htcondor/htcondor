/* 
** Copyright 1995 by Miron Livny and Jim Pruyne
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
** Author:  Jim Pruyne
**
*/ 

#define _POSIX_SOURCE
#include "condor_common.h"
#include "condor_fix_socket.h"
#include <netinet/in.h>
#include "url_condor.h"
#include "condor_debug.h"


/* Convert a string of the form "<xx.xx.xx.xx:pppp>" to a sockaddr_in  JCP */

int
string_to_sin(char *string, struct sockaddr_in *sin)
{
	int             i;
	char    *cur_byte;
	char    *end_string;

	string++;					/* skip the leading '<' */
	cur_byte = (char *) &(sin->sin_addr);
	for(end_string = string; end_string != 0; ) {
		end_string = (char *)strchr((const char *)string, '.');
		if (end_string == 0) {
			end_string = (char *)strchr((const char *)string, ':');
		}
		if (end_string) {
			*end_string = '\0';
			*cur_byte = atoi(string);
			cur_byte++;
			string = end_string + 1;
			*end_string = '.';
		}
	}
	
	end_string = (char *)strchr((const char *)string, '>');
	if (end_string) {
		*end_string = '\0';
	}
	sin->sin_port = htons(atoi(string));
	if (end_string) {
		*end_string = '>';
	}
}


int condor_bytes_stream_open_ckpt_file( const char *name, int flags, 
									   size_t n_bytes )
{
	struct sockaddr_in	sin;
	int		sock_fd;
	int		status;

	// cast discards const	
	string_to_sin((char *) name, &sin);
	sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (sock_fd < 0) {
		fprintf(stderr, "socket() failed, errno = %d\n", errno);
		fflush(stderr);
		return sock_fd;
	}
	sin.sin_family = AF_INET;
	status = connect(sock_fd, (struct sockaddr *) &sin, sizeof(sin));
	if (status < 0) {
		fprintf(stderr, "cbstp connect() FAILED, errno = %d\n", errno);
		fflush(stderr);
		return status;
	}
	return sock_fd;
}


#if 0
URLProtocol CBSTP_URL("cbstp", 
					 "CondorByteStreamProtocol",
					 condor_bytes_stream_open_ckpt_file);
#endif

void
init_cbstp()
{
    static URLProtocol	*CBSTP_URL = 0;

    if (CBSTP_URL == 0) {
        CBSTP_URL = new URLProtocol("cbstp", 
				    "CondorByteStreamProtocol",
				    condor_bytes_stream_open_ckpt_file);
    }
}
