
#ifndef CONDOR_FILE_REMOTE_H
#define CONDOR_FILE_REMOTE_H

#include "condor_file.h"

/**
This class sends all I/O operations to a remotely opened file.
*/

class CondorFileRemote : public CondorFile {
public:
	CondorFileRemote();

	virtual int open(const char *path, int flags, int mode);
	virtual int close();
	virtual int read(int offset, char *data, int length);
	virtual int write(int offset, char *data, int length);

	virtual int fcntl( int cmd, int arg );
	virtual int ioctl( int cmd, int arg );
	virtual int ftruncate( size_t length ); 
	virtual int fsync();

	virtual void checkpoint();
	virtual void suspend();
	virtual void resume(int count);

	virtual int local_access_hack();
	virtual int map_fd_hack();
};

#endif
