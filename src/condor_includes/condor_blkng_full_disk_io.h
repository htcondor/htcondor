#ifndef _CONDOR_BLKNG_FULL_DISK_IO_H
#define _CONDOR_BLKNG_FULL_DISK_IO_H

/* This file implementes blocking read/write operations on regular files.
	The main purpose is to read/write however many bytes you actually
	specified. These functions are NOT drop in replacements for 
	read() and write(), please read the comments in the implementation
	file as to their usage. */

BEGIN_C_DECLS

ssize_t condor_blkng_full_disk_read(int fd, char *ptr, int nbytes);
ssize_t condor_blkng_full_disk_write(int fd, void *ptr, int nbytes);

END_C_DECLS

#endif
