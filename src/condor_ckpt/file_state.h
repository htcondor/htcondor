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

 

#ifndef _FILE_STATE_H
#define _FILE_STATE_H

#include <limits.h>
#include <string.h>
#include <sys/types.h>

#include "file_types.h"
#include "buffer_cache.h"

class FilePointer;

/**
This class multiplexes number of UNIX file system calls.
The fd's used to index this table are "virtual file descriptors",
and could refer to real fds, remote fds, ioservers, sockets,
or who knows what.
<p>
It only makes sense to use this table when MappingFileDescriptors()
is in effect.  If it is not, just perform syscalls remotely or
locally, as the SyscallMode indicates.
<p>
This class does several things:
<dir> 
	<li> checks validity of vfds  
	<li> multiplexes vfds to file types
	<li> buffers seek pointers   
	<li> talks to the buffer cache
	<li> oversees checkpoint and restore
</dir>

This class does _not_:
<dir>
	<li> Decide whether mapping is in effect.
	     The system call stubs do that.
	<li> Implement read, write, seek, etc. for _any_ file.
	     Subclasses of File do that (file_types.C)
	<li> Implement buffering.
	     A BufferCache does that (buffer_cache.C)
	<li> Perform operations on names (i.e. stat()).
	     To operate on a name, open() it, and
	     then perform the fd equivalent of that op.
	     (i.e. fstat())  
</dir>
*/

class OpenFileTable {
public:

	/** Prepare the table for use */
	void	init();

	/** Write out any buffered data */
	void	flush();

	/** Display debug info */
	void	dump();

	/** Configure and use the buffer */
	void	init_buffer();

	/** Turn off buffering */
	void	disable_buffer();

	/** Map a virtual fd to the same real fd.  This is generally only
	used by the startup code to bootstrap a usable stdin/stdout until
	things get rolling. */

	int	pre_open( int fd, int readable, int writable, int is_remote );

	/** Standard UNIX operations: open, close, read, etc...
	These should all perform as their UNIX counterparts. */

	int	open( const char *path, int flags, int mode );
	int	close( int fd );

	ssize_t	read( int fd, void *data, size_t length );
	ssize_t	write( int fd, const void *data, size_t length );
	off_t	lseek( int fd, off_t offset, int whence );

	int	dup( int old );
	int	dup2( int old, int nfd );

	/** Like dup2, but use any free fd >= search. */
	int	search_dup2( int old, int search );

	int	fchdir( int fd );
	int	fstat( int fd, struct stat *buf );
	int	fcntl( int fd, int cmd, int arg );
	int	ioctl( int fd, int cmd, int arg );
	int	flock( int fd, int op );
	int	fstatfs( int fsync, struct statfs * buf );
	int	fchown( int fd, uid_t owner, gid_t group );
	int	fchmod( int fd, mode_t mode );
	int	ftruncate( int fd, size_t length );
	int	fsync( int fd );

	/* Flush buffers and save everything I know, as a precaution. */
	void	checkpoint();

	/* Same as checkpoint, but close everything in preparation for exit */
	void	suspend();

	/* A checkpoint has resumed, so open everything up again. */
	void	resume();

	/* Support the MapFd and LocalAccess interfaces for now */
	int	map_fd_hack( int fd );
	int	local_access_hack( int fd );

private:

	int	find_name(const char *path);
	int	find_empty();

	FilePointer	**pointers;
	int		length;
	BufferCache	*buffer;
	char		working_dir[_POSIX_PATH_MAX];
	int		prefetch_size;
};

/** This is a pointer to the single global instance of the file
table.  The only user of this pointer should be the system call
switches, who need to send some syscalls to the open file table.
*/

extern OpenFileTable *FileTab;

#endif



