
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
	virtual ~CondorFileBasic();

	virtual int cfile_open( const char *url, int flags, int mode );
	virtual int cfile_close();

	virtual int cfile_ioctl( int cmd, int arg );
	virtual int cfile_fcntl( int cmd, int arg );

	virtual int cfile_ftruncate( size_t s );
	virtual int cfile_fsync();
	virtual int cfile_flush();
	virtual int cfile_fstat( struct stat *buf );

	virtual int	is_readable();
	virtual int	is_writeable();
	virtual int	is_seekable();

	virtual int	get_size();
	virtual char	*get_url();

	virtual int	get_unmapped_fd();

protected:
	int	fd;		// the real fd used by this file
	char	url[_POSIX_PATH_MAX];

	int	readable;	// can this file be read?
	int	writeable;	// can this file be written?
	int	size;		// number of bytes in the file
	int	syscall_mode;	// mode to perform syscalls in
};

#endif
