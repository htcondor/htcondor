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

	for(;;) {

			/* Read a block of the file */
		read_size = sizeof(buf) < bytes_to_go ? sizeof(buf) : bytes_to_go;
		bytes_read = read( src_fd, buf, read_size );
		dprintf(D_FULLDEBUG,"The number of bytes read from %d are %d\n",src_fd,bytes_read);
		if( bytes_read <= 0 ) {
		dprintf(D_FULLDEBUG,"Entering bytes_read <=0, about to return -1\n");
			return -1;
		}

			/* Send it */
		bytes_written = write( dst_fd, buf, bytes_read );
		fflush(NULL); /* a must for a guaranteed successful restart  ..dhaval 11/29/95 */
		dprintf(D_FULLDEBUG,"The number of bytes read are %d and the number of bytes written to %d are %d\n",bytes_read,dst_fd,bytes_written);
		if( bytes_written != bytes_read ) {
		dprintf(D_FULLDEBUG,"Entering bytes_read <=0, about to return -1\n");
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
