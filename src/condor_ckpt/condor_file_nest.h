#ifndef CONDOR_FILE_NEST_H
#define CONDOR_FILE_NEST_H

#ifdef ENABLE_NEST

#include "condor_common.h"
#include "condor_file.h"
#include "nest_speak.h"

class CondorFileNest : public CondorFile {
public:
	CondorFileNest();
	virtual ~CondorFileNest();

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
	NestConnection con;
	NestReplyStatus status;

	char	url[_POSIX_PATH_MAX];
	char	method[_POSIX_PATH_MAX];
	char	server[_POSIX_PATH_MAX];
	char	path[_POSIX_PATH_MAX];
	int	port;
};

#endif /* ENABLE_NEST */

#endif
