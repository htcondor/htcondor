#include "condor_common.h"

/* This file implementes blocking read/write operations on regular files.
	The main purpose is to read/write however many bytes you actually
	specified. */

/* 
	condor_blkng_full_disk_read()

	Arguments:
		fd: file descriptor to a regular file
		ptr: pointer to a buffer
		nbytes: how many bytes that should try to read at once

	Return Value:
		If the return value equals nbytes, then all bytes requested
		were read successfully.

		If the return value is not equal to nbytes and errno ==
		0, then the returned value is the number of bytes actually
		read up to when an EOF happened.
		
		If the return value is not equal to nbytes and errno !=
		0, then some bytes had been read, but an error happened
		in the final read.
*/

ssize_t
condor_blkng_full_disk_read(int fd, char *ptr, int nbytes)
{
	int nleft, nread;

	nleft = nbytes;
	while (nleft > 0) {

#ifndef WIN32
		REISSUE_READ: 
#endif
		nread = read(fd, ptr, nleft);
		if (nread < 0) {

#ifndef WIN32
			/* error happened, ignore if EINTR, otherwise inform the caller */
			if (errno == EINTR) {
				goto REISSUE_READ;
			}
#endif
			/* return what I was able to read, if anything. */
			return(nbytes - nleft);

		} else if (nread == 0) {
			/* We've reached the end of file marker */

			/* in this case, make sure we let the user know what when the 
				bytes returned does not equal the bytes requested, it was
				because of an EOF. */
			errno = 0;
			break;
		}

		nleft -= nread;
		ptr   += nread;
	}

	/* return how much was actually read, which could include 0 in an
		EOF situation */
	return(nbytes - nleft);	 
}

/* 
	condor_blkng_full_disk_write()

	Arguments:
		fd: file descriptor to a regular file
		ptr: pointer to a buffer
		nbytes: how many bytes that should try to written at once

	Return Value:
		If the return value equals nbytes, then all bytes requested
		were written successfully.

		If the return value is not equal to nbytes and errno ==
		0, then the returned value is the number of bytes actually
		written up to when an EOF happened.
		
		If the return value is not equal to nbytes and errno !=
		0, then some bytes had been written, but an error happened
		in the final read.
		
	Notes:
		It is possible for this call to block forever taking up 100%
		of the cpu if for some reason the write() call repeatedly returns 
		zero bytes written when >0 bytes were expected to have been written.
*/
ssize_t
condor_blkng_full_disk_write(int fd, void *ptr, int nbytes)
{
	int nleft, nwritten;

	nleft = nbytes;
	while (nleft > 0) {
#ifndef WIN32
		REISSUE_WRITE: 
#endif
		nwritten = write(fd, ptr, nleft);
		if (nwritten < 0) {
#ifndef WIN32
			/* error happened, ignore if EINTR, otherwise inform the caller */
			if (errno == EINTR) {
				goto REISSUE_WRITE;
			}
#endif
			/* return what I was able to write */
			return(nbytes - nleft);
		}

		nleft -= nwritten;
		ptr   += nwritten;
	}
	
	/* return how much was actually written, which could include 0 */
	return(nbytes - nleft);
}


