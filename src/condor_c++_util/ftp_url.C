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

#include "condor_common.h"
#include "condor_fix_socket.h"
#include <netinet/in.h>
#include "url_condor.h"
#include "condor_debug.h"

/*
*	A full fledged ftp url looks like:
*	ftp://[username[:password]@]host/path/file
*	The current implementation does not support username or password, and
*	always does transfers in binary mode.  We can also only read from ftp URL's
*/


#define FTP_PORT	21
static char *uname = "anonymous";
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
int open_ftp( const char *name, int flags, size_t n_bytes )
{
	struct sockaddr_in	sin;
	int		sock_fd;
	int		status;
	char	*port_sep;
	char	*end_of_addr;
	int		port_num = FTP_PORT;
	struct hostent *he;
	char	ftp_cmd[1024];
	int		read_count;
	char	*ftp_resp;
	int		ip_addr[4];
	int		port[2];
	int		rval;

	/* We can only read via http */
/*	if (flags & O_WRONLY) {
		return -1;
	} */

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

	end_of_addr = (char *)strchr((const char *)name, '/');
	if (end_of_addr == 0) {
		return -1;
	}
	*end_of_addr = '\0';

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

	if (get_ftpd_response(sock_fd, FTP_LOGIN_RESP) == 0) {
		return -1;
	}
	
	sprintf(ftp_cmd, "USER %s\n", uname);
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
	rval = open_url(ftp_cmd, flags, n_bytes);
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
