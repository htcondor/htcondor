/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

 

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

#ifdef WANT_NETMAN
/* Perform a callback during file stream transfers so we can manage
   our network usage. */
void file_stream_progress_report(int bytes_moved);
#endif

/*
  Transfer a whole file from the "src_fd" to the "dst_fd".  Return
  bytes transferred.
*/
ssize_t
stream_file_xfer( int src_fd, int dst_fd, size_t n_bytes )
{
	char		buf[65536];		/* 64K */
	ssize_t		bytes_written;
	ssize_t		bytes_read;
	ssize_t		bytes_moved = 0;
	size_t		bytes_to_go = n_bytes;
	size_t		read_size;
	int			dont_know_file_size;
	int			rval;

	if (n_bytes == -1) {
		dont_know_file_size = 1;
	} else {
		dont_know_file_size = 0;
	}

	for(;;) {

			/* Read a block of the file */
		if (dont_know_file_size || sizeof(buf) < bytes_to_go) {
			read_size = sizeof(buf);
		} else {
			read_size = bytes_to_go;
		}
		bytes_read = read( src_fd, buf, read_size );
		if( bytes_read <= 0 ) {
			if (dont_know_file_size) {
				return bytes_moved;
			} else {
				return -1;
			}
		}

			/* Send it */
		for( bytes_written = 0;
			 bytes_written < bytes_read;
			 bytes_written += rval) {
			rval = write( dst_fd, buf+bytes_written, bytes_read-bytes_written );
			if( rval < 0 ) {
				dprintf( D_ALWAYS, "stream_file_xfer: %d bytes written, "
						 "%d bytes to go\n", bytes_moved, bytes_to_go );
				dprintf( D_ALWAYS, "stream_file_xfer: write returns %d "
						 "(errno=%d) when attempting to write %d bytes\n",
						 rval, errno, bytes_read );
				return -1;
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
#ifdef WANT_NETMAN
		else {
			file_stream_progress_report((int)bytes_moved);
		}
#endif
	}
}


ssize_t
multi_stream_file_xfer( int src_fd, int dst_fd_cnt, int *dst_fd_list,
					   size_t n_bytes )
{
	char		buf[65536];		/* 64K */
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
		if (dont_know_file_size || sizeof(buf) < bytes_to_go) {
			read_size =  sizeof(buf);
		} else {
			read_size = bytes_to_go;
		}
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
	
	if( ! _condor_local_bind(socket_fd) ) {
		close( socket_fd );
		fprintf(stderr, "condor_exec: bind failed\n");
		return -1;
	}
	
	listen(socket_fd, listen_count);
	
	len = sizeof(*sin);
	getsockname(socket_fd, (struct sockaddr *) sin, &len);
	sin->sin_addr.s_addr = htonl( my_ip_addr() );

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
