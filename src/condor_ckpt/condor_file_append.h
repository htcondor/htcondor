
#ifndef CONDOR_FILE_APPEND_H
#define CONDOR_FILE_APPEND_H

/*
This class passes all operations through, except open.
For open(), it forces the O_APPEND flag to be set.
*/


#include "condor_file.h"

class CondorFileAppend : public CondorFile {
public:
	CondorFileAppend( CondorFile *original );
	virtual ~CondorFileAppend();

	virtual int cfile_open( const char *url, int flags, int mode );
	virtual int cfile_close();
	virtual int cfile_read( int offset, char *data, int length );
	virtual int cfile_write( int offset, char *data, int length );

	virtual int cfile_fcntl( int cmd, int arg );
	virtual int cfile_ioctl( int cmd, int arg );
	virtual int cfile_ftruncate( size_t length ); 
	virtual int cfile_fsync();
	virtual int cfile_flush();
	virtual int cfile_fstat( struct stat *buf );

	virtual int	is_readable();
	virtual int	is_writeable();
	virtual int	is_seekable();

	virtual int	get_size();
	virtual char	*get_url();

	virtual int get_unmapped_fd();
	virtual int is_file_local();

private:
	char url[_POSIX_PATH_MAX];
	CondorFile *original;
};

#endif
