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

#include <stdio.h>
#include <string.h>
#include "condor_common.h"
#include <sys/socket.h>
#include <netinet/in.h>
extern "C" {
#include <netdb.h>
}
#include "url_condor.h"
#include "condor_debug.h"

#define HTTP_PORT	80

#define HTTP_GET_FORMAT "GET %s HTTP/1.0\n\n"
#define ASCII_CR ('M' - 32)
#define ASCII_LF ('J' - 32)

int
readline(int fd, char *buf)
{
	int		count = 0;
	int		rval;

	for(;;) {
		if ((rval = read(fd, buf, 1)) < 0) {
			return rval;
		}
		if (*buf != '\n') {
			count++;
			buf++;
		} else {
			break;
		}
	}
	return	count;
}


static
int open_http( const char *name, int flags, size_t n_bytes )
{
	struct sockaddr_in	sin;
	int		sock_fd;
	int		status;
	char	*port_sep;
	char	*end_of_addr;
	int		port_num = HTTP_PORT;
	struct hostent *he;
	char	http_cmd[1024];
	int		read_count;
	
	/* We can only read via http */
	if (flags & O_WRONLY) {
		return -1;
	}

	sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (sock_fd < 0) {
		fprintf(stderr, "socket() failed, errno = %d\n", errno);
		fflush(stderr);
		return sock_fd;
	}

	if (name[0] != '/' || name[1] != '/') {
		return -1;
	}

	/* Skip those two leading '/'s */
	name++;
	name++;

	port_sep = strchr(name, ':');
	end_of_addr = strchr(name, '/');
	if (end_of_addr == 0) {
		return -1;
	}
	*end_of_addr = '\0';

	if (port_sep) {
		*port_sep = '\0';
		port_sep++;
		port_num = atoi(port_sep);
	}
	sin.sin_port = htons(port_num);

	he = gethostbyname( name );
	if ( he ) {
		sin.sin_family = he->h_addrtype;
		sin.sin_addr = *((struct in_addr *) he->h_addr_list[0]);
	} else {
		return -1;
	}

	*end_of_addr = '/';

	status = connect(sock_fd, (struct sockaddr *) &sin, sizeof(sin));
	if (status < 0) {
		fprintf(stderr, "http connect() FAILED, errno = %d\n", errno);
		fflush(stderr);
		return status;
	}

	name = end_of_addr;
	sprintf(http_cmd, HTTP_GET_FORMAT, name, ASCII_CR, ASCII_LF, ASCII_CR, 
			ASCII_LF);
	write(sock_fd, http_cmd, strlen(http_cmd));

	/* Skip the header part */
	while (readline(sock_fd, http_cmd) > 1) {
	}

	return sock_fd;
}


void
init_http()
{
	static URLProtocol	*HTTP_URL = 0;

	if (HTTP_URL == 0) {
		HTTP_URL = new URLProtocol("http", "HyperTextTransferProtocol", 
								   open_http);
	}
}
