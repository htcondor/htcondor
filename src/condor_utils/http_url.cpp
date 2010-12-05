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


#include "condor_common.h"
#include "url_condor.h"
#include "internet.h"
#include "condor_debug.h"
#include "ipv6_addrinfo.h"
#include "condor_sockfunc.h"

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
int open_http( const char *name, int flags )
{
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

	addrinfo_iterator iter;
	int ret = ipv6_getaddrinfo(name, NULL, iter);
	*end_of_addr = '/';

	if (ret != 0) {
		fprintf(stderr, "http getaddrinfo() FAILED, return code = %d "
				"errno = %d\n", ret, errno);
		fflush(stderr);
		return -1;
	}
	while(addrinfo* ai = iter.next()) {
		if (ai->ai_socktype != SOCK_STREAM || ai->ai_protocol != 0)
			continue;

		sock_fd = socket(ai->ai_family, SOCK_STREAM, 0);
		if (sock_fd < 0)
			continue;

		if (!_condor_local_bind(TRUE, sock_fd)) {
			close(sock_fd);
			sock_fd = -1;
			continue;
		}
		ipaddr addr(ai->ai_addr);
		addr.set_port(port_num);
		status = condor_connect(sock_fd, addr);
		if (status < 0) {
			fprintf(stderr, "http connect() FAILED, ip = %s errno = %d\n",
					addr.to_ip_string().Value(),
					errno);
			fflush(stderr);
			close( sock_fd );
			sock_fd = -1;
			continue;
		}
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
