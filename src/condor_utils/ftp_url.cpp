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

/*
*	A full fledged ftp url looks like:
*	ftp://[username[:password]@]host/path/file
*	The current implementation does not support username or password, and
*	always does transfers in binary mode.  We can also only read from ftp URL's
*/


#define FTP_PORT	21
static char *username = "anonymous";
static char *passwd = "CondorURLFTP@localhost.edu";

#define FTP_LOGIN_RESP	220
#define FTP_PASSWD_RESP 331
#define FTP_PASV_RESP	227
#define FTP_TYPE_RESP	200
#define FTP_CONNECTED_RESP 150

extern int readline(int, char *);

static
char *get_ftpd_response(int sock_fd, int resp_val)
{
	static char	resp[1024];
	int				ftp_resp_num;
	int				read_count;

	do {
		if ((read_count = readline(sock_fd, resp)) < 0) {
			close(sock_fd);
			return 0;
		}
		resp[read_count] = '\0';
/*		printf("ftpd responded: %s", resp); */
		sscanf(resp, "%d", &ftp_resp_num);
	} while (ftp_resp_num != resp_val);
/*	fflush(stdout); */
	return resp;
}

static
int open_ftp( const char *name, int flags )
{
	int		sock_fd = -1;
	int		status;
	//char	*port_sep; // commented out, because it is unused, avoiding warning
	char	*end_of_addr;
	int		port_num = FTP_PORT;
	char	ftp_cmd[1024];
	//int		read_count; // commented out, because it is unused, avoiding warning
	char	*ftp_resp;
	int		ip_addr[4];
	int		port[2];
	int		rval;

	/* We can only read via http */
/*	if (flags & O_WRONLY) {
		return -1;
	} */

	if (name[0] != '/' || name[1] != '/') {
		return -1;
	}

	/* Skip those two leading '/'s */
	name++;
	name++;

	end_of_addr = (char *)strchr((const char *)name, '/');
	if (end_of_addr == 0) {
		return -1;
	}
	*end_of_addr = '\0';

	addrinfo_iterator iter;
	int ret = ipv6_getaddrinfo(name, NULL, iter);
	*end_of_addr = '/';
	if (ret != 0) {
		fprintf(stderr, "ftp getaddrinfo() FAILED, return code = %d "
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
			fprintf(stderr, "ftp connect() FAILED, ip = %s errno = %d\n",
					addr.to_ip_string().Value(),
					errno);
			fflush(stderr);
			close( sock_fd );
			sock_fd = -1;
			continue;
		}
	}

	name = end_of_addr;

	if (get_ftpd_response(sock_fd, FTP_LOGIN_RESP) == 0) {
		return -1;
	}
	
	sprintf(ftp_cmd, "USER %s\n", username);
	write(sock_fd, ftp_cmd, strlen(ftp_cmd));
	if (get_ftpd_response(sock_fd, FTP_PASSWD_RESP) == 0) {
		return -1;
	}

	sprintf(ftp_cmd, "PASS %s\n", passwd);
	write(sock_fd, ftp_cmd, strlen(ftp_cmd));

	sprintf(ftp_cmd, "PASV\n");
	write(sock_fd, ftp_cmd, strlen(ftp_cmd));

	ftp_resp = get_ftpd_response(sock_fd, FTP_PASV_RESP);
	if (ftp_resp == 0) {
		return -1;
	}

	sprintf(ftp_cmd, "TYPE I\n");
	write(sock_fd, ftp_cmd, strlen(ftp_cmd));
	if (get_ftpd_response(sock_fd, FTP_TYPE_RESP) == 0) {
		return -1;
	}

	if (flags == O_RDONLY) {
		sprintf(ftp_cmd, "RETR %s\n", name);
	} else if (flags == O_WRONLY) {
		sprintf(ftp_cmd, "STOR %s\n", name);
	} else {
		close(sock_fd);
		return -1;
	}
	write(sock_fd, ftp_cmd, strlen(ftp_cmd));

	for ( ; *ftp_resp != '(' ; ftp_resp++) 
		;

	ftp_resp++;
	sscanf(ftp_resp, "%d,%d,%d,%d,%d,%d", &ip_addr[0], &ip_addr[1], 
		   &ip_addr[2], &ip_addr[3], &port[0], &port[1]);

	sprintf(ftp_cmd, "cbstp:<%d.%d.%d.%d:%d>", ip_addr[0], ip_addr[1], 
		   ip_addr[2], ip_addr[3], port[0] << 8 | port[1]);
	rval = open_url(ftp_cmd, flags);
	if (flags == O_WRONLY) {
		get_ftpd_response(sock_fd, FTP_CONNECTED_RESP);
	}
	close(sock_fd);
	return rval;
}


void
init_ftp()
{
	static URLProtocol	*FTP_URL = 0;

	if (FTP_URL == 0) {
		FTP_URL = new URLProtocol("ftp", "FileTransferProtocol", 
								   open_ftp);
	}
}
