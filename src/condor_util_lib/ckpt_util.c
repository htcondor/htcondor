/* 
** Copyright 1986, 1987, 1988, 1989, 1990, 1991, 1995 by the Condor Design Team
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the Condor Design Team not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the Condor
** Design Team make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE CONDOR DESIGN TEAM DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE CONDOR DESIGN TEAM BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Jim Pruyne
** 	        University of Wisconsin, Computer Sciences Dept.
** 
*/ 

/*
   These are utility functions which are used in moving and managing 
   checkpoint files.  They're placed here so the same set of routines
   can be used by the shadow and the checkpoint server.
*/

#define _POSIX_SOURCE

#include "condor_common.h"
#include "condor_debug.h"
#include <sys/socket.h>
#include <netinet/in.h>

/*
  Transfer a whole file from the "src_fd" to the "dst_fd".  Return
  bytes transferred.
*/
ssize_t
stream_file_xfer( int src_fd, int dst_fd, size_t n_bytes )
{
	char		buf[4096];
	ssize_t		bytes_written;
	ssize_t		bytes_read;
	ssize_t		bytes_moved = 0;
	size_t		bytes_to_go = n_bytes;
	size_t		read_size;
	int			dont_know_file_size;

	if (n_bytes == -1) {
		dont_know_file_size = 1;
	} else {
		dont_know_file_size = 0;
	}

	for(;;) {

			/* Read a block of the file */
		read_size = sizeof(buf) < bytes_to_go ? sizeof(buf) : bytes_to_go;
		bytes_read = read( src_fd, buf, read_size );
		if( bytes_read <= 0 ) {
			if (dont_know_file_size) {
				return bytes_moved;
			} else {
				return -1;
			}
		}

			/* Send it */
		bytes_written = write( dst_fd, buf, bytes_read );
		if( bytes_written != bytes_read ) {
			return -1;
		}

			/* Accumulate */
		bytes_moved += bytes_written;
		bytes_to_go -= bytes_written;
		if( bytes_to_go == 0 ) {
			dprintf( D_FULLDEBUG,
				"\tChild Shadow: STREAM FILE XFER COMPLETE - %d bytes\n",
				bytes_moved
			);
			return bytes_moved;
		}
	}
}


ssize_t
multi_stream_file_xfer( int src_fd, int dst_fd_cnt, int *dst_fd_list,
					   size_t n_bytes )
{
	char		buf[4096];
	ssize_t		bytes_written;
	ssize_t		bytes_read;
	ssize_t		bytes_moved = 0;
	size_t		bytes_to_go = n_bytes;
	size_t		read_size;
	int			dont_know_file_size;
	int			i;

	if (n_bytes == -1) {
		dont_know_file_size = 1;
	} else {
		dont_know_file_size = 0;
	}

	for(;;) {

			/* Read a block of the file */
		read_size = sizeof(buf) < bytes_to_go ? sizeof(buf) : bytes_to_go;
		bytes_read = read( src_fd, buf, read_size );
		if( bytes_read <= 0 ) {
			if (dont_know_file_size) {
				return bytes_moved;
			} else {
				return -1;
			}
		}

			/* Send it */
		for (i = 0; i < dst_fd_cnt; i++) {
			bytes_written = write( dst_fd_list[i], buf, bytes_read );
			if( bytes_written != bytes_read ) {
				dprintf(D_ALWAYS, "Chocked sending to one fd in my list(%d)\n",
						dst_fd_list[i]);
				dst_fd_list[i] = dst_fd_list[dst_fd_cnt - 1];
				dst_fd_cnt--;
				if (dst_fd_cnt == 0) {
					return -1;
				}
			}
		}

			/* Accumulate */
		bytes_moved += bytes_written;
		bytes_to_go -= bytes_written;
		if( bytes_to_go == 0 ) {
			dprintf( D_FULLDEBUG,
				"\tChild Shadow: STREAM FILE XFER COMPLETE - %d bytes\n",
				bytes_moved
			);
			return bytes_moved;
		}
	}
}


int
create_socket(sin, listen_count)
struct sockaddr_in *sin;
int             listen_count;
{
	int             socket_fd;
	int             len;
	
	socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_fd < 0) {
		fprintf(stderr, "condor_exec: unable to create socket\n");
		return -1;
	}
	
	memset((char *) sin,0, sizeof(*sin));
	sin->sin_family = AF_INET;
	sin->sin_port = 0;
	
	if ( bind(socket_fd, (struct sockaddr *) sin, sizeof(*sin)) < 0) {
		fprintf(stderr, "condor_exec: bind failed\n");
		return -1;
	}
	
	listen(socket_fd, listen_count);
	
	len = sizeof(*sin);
	getsockname(socket_fd, (struct sockaddr *) sin, &len);
	get_inet_address(&(sin->sin_addr));  /* from internet.c */
	
	return socket_fd;
}


int
create_socket_url(url_buf, listen_count)
char	*url_buf;
int		listen_count;
{
	struct sockaddr_in	sin;
	int		sock_fd;

	sock_fd = create_socket(&sin, listen_count);
	if (sock_fd >= 0) {
		sprintf(url_buf, "cbstp:%s", sin_to_string(&sin));
	}
	return sock_fd;
}


int
wait_for_connections(sock_fd, count, return_list)
int		sock_fd;
int		count;
int		*return_list;
{
	int		i;
	struct sockaddr from;
	int		from_len = sizeof(from); /* FIX : dhruba */

	for (i = 0; i < count; i++) {
		return_list[i] = tcp_accept_timeout(sock_fd, &from, &from_len, 300);
	}
	return i;
}
