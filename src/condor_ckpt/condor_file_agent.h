
#ifndef CONDOR_FILE_AGENT_H
#define CONDOR_FILE_AGENT_H

#include "condor_file.h"

/**
This object takes an existing CondorFile and arranges for
the entire thing to be moved to a local temp file, where
it can be accessed quickly. When the file is closed, the
local copy is moved back to the original location.
<p>
The temp file is chosen by using tmpnam(), which generally
choose a file in /var/tmp.  A better place can and should
be chosen when Condor moves toward using a general-purpose
caching mechanism at each host.
*/

class CondorFileAgent : public CondorFile {
public:
	CondorFileAgent( CondorFile *f );
	virtual ~CondorFileAgent();

	virtual int open( const char *url, int flags, int mode );
	virtual int close();
	virtual int read( int offset, char *data, int length );
	virtual int write( int offset, char *data, int length );

	virtual int fcntl( int cmd, int arg );
	virtual int ioctl( int cmd, int arg );
	virtual int ftruncate( size_t length ); 
	virtual int fsync();
	virtual int flush();
	virtual int fstat( struct stat *buf );

	virtual int	is_readable();
	virtual int	is_writeable();
	virtual int	is_seekable();

	virtual int	get_size();
	virtual char	*get_url();

	virtual int get_unmapped_fd();
	virtual int is_file_local();

private:
	CondorFile *original;
	CondorFile *local_copy;
	char local_filename[_POSIX_PATH_MAX];
	char local_url[_POSIX_PATH_MAX];
	char url[_POSIX_PATH_MAX];
};

#endif



