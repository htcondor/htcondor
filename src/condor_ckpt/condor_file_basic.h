
#ifndef CONDOR_FILE_BASIC
#define CONDOR_FILE_BASIC

#include "condor_file.h"

/**
CondorFileBasic represents the operations that are common to files
accessed by syscalls, whether local or remote.  This object should
not be instantiated, it only serves as a starting point for
CondorFileLocal and CondorFileRemote.
*/

class CondorFileBasic : public CondorFile {
public:
	CondorFileBasic( int mode );

	virtual int open( const char *path, int flags, int mode );
	virtual int close();

	virtual int ioctl( int cmd, int arg );
	virtual int fcntl( int cmd, int arg );

	virtual int ftruncate( size_t s );
	virtual int fsync();

	virtual void checkpoint();
	virtual void suspend();
	virtual void resume( int resume_count );

	virtual int attach( int f, char *n, int r, int w );

	virtual int	is_readable();
	virtual int	is_writeable();
	virtual void	set_size(size_t size);
	virtual int	get_size();
	virtual char	*get_name();

	virtual int	map_fd_hack();

protected:
	int	fd;		// the real fd used by this file
	char	name[_POSIX_PATH_MAX];

	int	readable;	// can this file be read?
	int	writeable;	// can this file be written?
	int	size;		// number of bytes in the file
	int	attached;	// was I attached or opened?
	int	open_flags;	// what flags to use at re-open?

	int	ioctl_sig;	// signal flag stored by ioctl
	int	fcntl_fl;	// file flags stored by fcntl
	int	fcntl_fd;	// fd flags stored by fcntl

	int	syscall_mode;	// mode to perform syscalls in
	int	resume_count;	// how many times have I been resumed?
};

#endif
