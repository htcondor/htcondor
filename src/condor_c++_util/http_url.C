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

#include "condor_common.h"
#include "url_condor.h"
#include "internet.h"
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

	if( ! _condor_local_bind(sock_fd) ) {
		close( sock_fd );
		return -1;
	}

	if (name[0] != '/' || name[1] != '/') {
		return -1;
	}

	/* Skip those two leading '/'s */
	name++;
	name++;

	port_sep = (char *)strchr((const char *)name, ':');
	end_of_addr = (char *)strchr((const char *)name, '/');
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
		close( sock_fd );
		return status;
	}

	name = end_of_addr;
	// We used to pass CR/LF CR/LF to the sprintf, but the format
	// string can't accept them (It just has %s), so I removed
	// them. Should they be there though? -Alain 30-Dec-2001
	sprintf(http_cmd, HTTP_GET_FORMAT, name);//, ASCII_CR, ASCII_LF, ASCII_CR, 
	//ASCII_LF);
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
